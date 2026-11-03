// @book: 网络多人在线游戏开发实战教程
// @topic: blocking-vs-nonblocking
// @source: 第1章 网络编程基础（讲义）.md §1.4
// @expect: 阻塞 recv 有数据时返回真实字节数并原样回显; 2s 内无数据按超时返回, 进程自行退出
#include <cstdio>
#include <string>
#include <thread>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

int make_listener(unsigned short& port_out) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    timeval tv{2, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));  // accept 也不会永远睡死
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { ::close(fd); return -1; }
    if (::listen(fd, 8) != 0) { ::close(fd); return -1; }
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) { ::close(fd); return -1; }
    port_out = ::ntohs(addr.sin_port);
    return fd;
}

}  // namespace

int main() {
    ::alarm(15);  // 兜底: 任何异常都不会留下挂起进程
    unsigned short port = 0;
    int listen_fd = make_listener(port);
    if (listen_fd < 0) { std::printf("socket setup failed\n"); return 1; }

    std::thread server([&] {
        int conn = ::accept(listen_fd, nullptr, nullptr);
        if (conn < 0) { std::printf("[server] accept timeout\n"); return; }
        timeval tv{2, 0};
        ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        char buf[64];
        for (;;) {
            ssize_t n = ::recv(conn, buf, sizeof(buf), 0);
            if (n <= 0) { std::printf("[server] recv stopped n=%zd\n", n); break; }
            std::printf("[server] recv %zd bytes: %.*s\n", n, static_cast<int>(n), buf);
            ::send(conn, buf, static_cast<size_t>(n), 0);
        }
        ::close(conn);
    });

    int cli = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(cli, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::printf("connect failed\n");
        server.join();
        ::close(listen_fd);
        return 1;
    }
    for (int i = 0; i < 3; ++i) {
        ::send(cli, "ping", 4, 0);
        char rb[64];
        ssize_t n = ::recv(cli, rb, sizeof(rb), 0);
        std::printf("[client] recv %zd bytes\n", n);
    }
    ::close(cli);
    server.join();
    ::close(listen_fd);
    std::printf("blocking echo done\n");
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX socket, 请在 Linux 上运行\n");
    return 0;
}

#endif
