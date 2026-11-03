// @book: 网络多人在线游戏开发实战教程
// @topic: tcp-stream-vs-udp-datagram
// @source: 第1章 网络编程基础（讲义）.md §1.4
// @expect: 3 条逻辑消息一次 send 发出共 10 字节, 服务端只看到一段字节流, 无法从字节本身还原消息边界
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
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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
    ::alarm(15);
    unsigned short port = 0;
    int listen_fd = make_listener(port);
    if (listen_fd < 0) { std::printf("socket setup failed\n"); return 1; }

    const std::string logical = "AAABBBBBCC";  // 3 条逻辑消息: AAA / BBBBB / CC
    std::thread server([&] {
        int conn = ::accept(listen_fd, nullptr, nullptr);
        if (conn < 0) { std::printf("[server] accept timeout\n"); return; }
        timeval tv{2, 0};
        ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        char buf[256];
        long total = 0;
        int calls = 0;
        for (;;) {
            ssize_t n = ::recv(conn, buf, sizeof(buf), 0);
            if (n <= 0) break;
            ++calls;
            total += n;
            std::printf("[server] recv#%d %zd bytes: ", calls, n);
            std::fwrite(buf, 1, static_cast<size_t>(n), stdout);
            std::printf("\n");
            ::send(conn, buf, static_cast<size_t>(n), 0);
        }
        std::printf("[server] 逻辑消息=3 收到字节=%ld recv调用次数=%d\n", total, calls);
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
    ::send(cli, logical.data(), logical.size(), 0);
    char back[256];
    long got = 0;
    int calls = 0;
    while (got < static_cast<long>(logical.size())) {
        ssize_t n = ::recv(cli, back + got, sizeof(back) - got, 0);
        if (n <= 0) break;
        got += n;
        ++calls;
    }
    std::printf("[client] 一次send发出%zu字节(内含3条消息), 回读%ld字节用了%d次recv\n",
                logical.size(), got, calls);
    ::close(cli);
    server.join();
    ::close(listen_fd);
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX socket, 请在 Linux 上运行\n");
    return 0;
}

#endif
