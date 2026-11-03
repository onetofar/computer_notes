// @book: 网络多人在线游戏开发实战教程
// @topic: tcp-stream-vs-udp-datagram
// @source: 第1章 网络编程基础（讲义）.md §1.3
// @expect: 3 个 UDP 数据报各自保持边界, 服务端一次 recvfrom 只拿到一个完整报文
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#if defined(__linux__)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

int make_udp(unsigned short& port_out) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { ::close(fd); return -1; }
    timeval tv{2, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) { ::close(fd); return -1; }
    port_out = ::ntohs(addr.sin_port);
    return fd;
}

}  // namespace

int main() {
    ::alarm(15);
    unsigned short port = 0;
    int srv = make_udp(port);
    if (srv < 0) { std::printf("socket setup failed\n"); return 1; }

    const char* msgs[3] = {"AAA", "BBBBB", "CC"};
    std::thread server([&] {
        char buf[256];
        int count = 0;
        for (;;) {
            sockaddr_in peer{};
            socklen_t plen = sizeof(peer);
            ssize_t n = ::recvfrom(srv, buf, sizeof(buf), 0,
                                   reinterpret_cast<sockaddr*>(&peer), &plen);
            if (n <= 0) break;
            ++count;
            std::printf("[server] recvfrom 第%d个数据报 %zd bytes: %.*s\n", count, n,
                        static_cast<int>(n), buf);
            ::sendto(srv, buf, static_cast<size_t>(n), 0,
                     reinterpret_cast<sockaddr*>(&peer), plen);  // 数据报必须按来源地址回
        }
        std::printf("[server] 共收到 %d 个数据报\n", count);
    });

    int cli = ::socket(AF_INET, SOCK_DGRAM, 0);
    timeval ctv{2, 0};
    ::setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &ctv, sizeof(ctv));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    char back[256];
    for (int i = 0; i < 3; ++i) {
        size_t len = std::strlen(msgs[i]);
        ::sendto(cli, msgs[i], len, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ssize_t n = ::recv(cli, back, sizeof(back), 0);
        if (n > 0) std::printf("[client] 回读第%d个数据报 %zd bytes\n", i + 1, n);
    }
    std::printf("[client] 3次sendto, 每个数据报独立成帧\n");
    ::close(cli);
    server.join();
    ::close(srv);
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX socket, 请在 Linux 上运行\n");
    return 0;
}

#endif
