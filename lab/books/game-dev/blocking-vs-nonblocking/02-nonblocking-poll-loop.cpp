// @book: 网络多人在线游戏开发实战教程
// @topic: blocking-vs-nonblocking
// @source: 第1章 网络编程基础（讲义）.md §1.5
// @expect: 非阻塞 recv 没有数据立刻返回; 统计出空问次数远多于命中次数, 进程自动退出
#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int make_listener(unsigned short& port_out) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(fd, 8) != 0) { ::close(fd); return -1; }
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) { ::close(fd); return -1; }
    port_out = ::ntohs(addr.sin_port);
    return fd;
}

}  // namespace

int main() {
    ::alarm(15);
    unsigned short port = 0;
    int listen_fd = make_listener(port);
    if (listen_fd < 0) { std::printf("socket setup failed\n"); return 1; }

    std::atomic<bool> done{false};
    std::thread client([&] {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(port);
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            done = true; ::close(fd); return;
        }
        for (int i = 0; i < 5; ++i) {
            ::usleep(20 * 1000);
            ::send(fd, "ping", 4, 0);
            char rb[64];
            ssize_t n = ::recv(fd, rb, sizeof(rb), 0);
            if (n > 0) std::printf("[client] echo %zd bytes\n", n);
        }
        ::close(fd);
        done = true;
    });

    std::vector<int> conns;
    long rounds = 0, recv_calls = 0, data_hits = 0, empty_calls = 0;
    while (!done) {
        ++rounds;
        for (;;) {  // 每帧把等待队列取尽
            int c = ::accept(listen_fd, nullptr, nullptr);
            if (c < 0) break;
            int flags = ::fcntl(c, F_GETFL, 0);
            ::fcntl(c, F_SETFL, flags | O_NONBLOCK);
            conns.push_back(c);
        }
        pollfd pfd{};
        pfd.fd = listen_fd;
        pfd.events = POLLIN;
        ::poll(&pfd, 1, 1);
        for (size_t i = 0; i < conns.size();) {
            ++recv_calls;
            char buf[64];
            ssize_t n = ::recv(conns[i], buf, sizeof(buf), 0);
            if (n > 0) {
                ++data_hits;
                ::send(conns[i], buf, static_cast<size_t>(n), 0);
                ++i;
            } else if (n == 0) {
                ::close(conns[i]);
                conns[i] = conns.back();
                conns.pop_back();
            } else {
                ++empty_calls;  // EAGAIN: 非阻塞立刻返回, 不睡
                ++i;
            }
        }
        ::usleep(1000);
    }
    for (int c : conns) ::close(c);
    client.join();
    ::close(listen_fd);
    double ratio = recv_calls ? 100.0 * static_cast<double>(empty_calls) / static_cast<double>(recv_calls) : 0.0;
    std::printf("rounds=%ld recv_calls=%ld data_hits=%ld empty_calls=%ld empty_ratio=%.1f%%\n",
                rounds, recv_calls, data_hits, empty_calls, ratio);
    std::printf("nonblocking loop done\n");
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX socket, 请在 Linux 上运行\n");
    return 0;
}

#endif
