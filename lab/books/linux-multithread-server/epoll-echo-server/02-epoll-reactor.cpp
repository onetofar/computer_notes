// @book: Linux多线程服务端编程
// @topic: epoll-echo-server
// @source: 第8章 muduo网络库设计与实现（讲义）.md §8.13
// @expect: 单线程 epoll reactor 在 127.0.0.1 上完成 20 个连接的回显, 线程数=1
#include <cstdio>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
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

    int lfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (lfd < 0) { std::perror("socket"); return 1; }
    int one = 1;
    ::setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(lfd, (sockaddr*)&addr, sizeof addr) < 0) { std::perror("bind"); return 1; }
    if (::listen(lfd, 64) < 0) { std::perror("listen"); return 1; }

    socklen_t len = sizeof addr;
    ::getsockname(lfd, (sockaddr*)&addr, &len);
    int port = ::ntohs(addr.sin_port);
    std::printf("[server] listen 127.0.0.1:%d (单线程 epoll reactor)\n", port);

    int epfd = ::epoll_create1(0);
    if (epfd < 0) { std::perror("epoll_create1"); return 1; }
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    ::epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    std::atomic<int> ok{0};
    std::atomic<int> served{0};
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

    std::vector<epoll_event> events(64);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (served.load() < kClients && std::chrono::steady_clock::now() < deadline) {
        int n = ::epoll_wait(epfd, events.data(), (int)events.size(), 200);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == lfd) {
                for (;;) {  // accept 到 EAGAIN 为止, 对应 level trigger 的写法
                    int cfd = ::accept(lfd, nullptr, nullptr);
                    if (cfd < 0) break;
                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLRDHUP;
                    cev.data.fd = cfd;
                    ::epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                }
                continue;
            }
            char buf[64];
            ssize_t r = ::recv(fd, buf, sizeof buf, 0);
            if (r > 0) {
                ::send(fd, buf, r, 0);
            } else {
                ::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                ::close(fd);
                served.fetch_add(1);
            }
        }
    }

    for (auto& t : clients) t.join();
    ::close(epfd);
    ::close(lfd);

    std::printf("[server] 回显成功=%d/%d, 服务端线程数=1 (epoll_wait 返回活动 fd 列表)\n",
                ok.load(), kClients);
    return 0;
}
#else
int main() {
    std::printf("此对照实验需要 Linux (epoll 只在 Linux 可用)\n");
    return 0;
}
#endif
