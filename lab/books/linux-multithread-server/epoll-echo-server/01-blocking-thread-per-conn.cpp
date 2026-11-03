// @book: Linux多线程服务端编程
// @topic: epoll-echo-server
// @source: 第3章 多线程服务器的适用场合与常用编程模型（讲义）.md §3.5
// @expect: 阻塞 IO + 每连接一线程, 20 个连接全部回显成功, 线程数=21
#include <cstdio>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

int main() {
    constexpr int kClients = 20;
    constexpr char kPayload[] = "ping";

    int lfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) { std::perror("socket"); return 1; }
    int one = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);  // 只绑回环
    addr.sin_port = 0;                                  // 让内核分配端口
    if (::bind(lfd, (sockaddr*)&addr, sizeof addr) < 0) { std::perror("bind"); return 1; }
    if (::listen(lfd, 64) < 0) { std::perror("listen"); return 1; }

    socklen_t len = sizeof addr;
    ::getsockname(lfd, (sockaddr*)&addr, &len);
    int port = ::ntohs(addr.sin_port);
    std::printf("[server] listen 127.0.0.1:%d (阻塞 IO, 每连接一线程)\n", port);

    std::atomic<int> ok{0};
    std::vector<std::thread> clients;
    for (int i = 0; i < kClients; ++i) {
        clients.emplace_back([&] {
            int cfd = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
            a.sin_port = ::htons(port);
            if (::connect(cfd, (sockaddr*)&a, sizeof a) < 0) { ::close(cfd); return; }
            ::send(cfd, kPayload, sizeof kPayload - 1, 0);
            char buf[16]{};
            ssize_t n = ::recv(cfd, buf, sizeof buf, 0);
            if (n == (ssize_t)(sizeof kPayload - 1) &&
                std::memcmp(buf, kPayload, sizeof kPayload - 1) == 0) {
                ok.fetch_add(1);
            }
            ::close(cfd);
        });
    }

    std::atomic<int> served{0};
    std::vector<std::thread> workers;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (served.load() < kClients && std::chrono::steady_clock::now() < deadline) {
        // listen socket 用 poll 等可读, 否则 accept 会一直阻塞着等下一个连接
        pollfd pfd{lfd, POLLIN, 0};
        if (::poll(&pfd, 1, 100) <= 0) continue;
        int cfd = ::accept(lfd, nullptr, nullptr);
        if (cfd < 0) continue;
        workers.emplace_back([cfd, &served] {
            char buf[64];
            ssize_t n = ::recv(cfd, buf, sizeof buf, 0);
            if (n > 0) {
                ::send(cfd, buf, n, 0);
                served.fetch_add(1);
            }
            ::close(cfd);
        });
    }

    for (auto& t : clients) t.join();
    for (auto& t : workers) t.join();
    ::close(lfd);

    std::printf("[server] 回显成功=%d/%d, 服务端线程数=%lu (1 accept + %d 工作线程)\n",
                ok.load(), kClients, (unsigned long)workers.size(), kClients);
    return 0;
}
#else
int main() {
    std::printf("此对照实验需要 Linux (阻塞 socket + 线程模型)\n");
    return 0;
}
#endif
