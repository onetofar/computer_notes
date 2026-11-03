// @book: 网络多人在线游戏开发实战教程
// @topic: tcp-stream-vs-udp-datagram
// @source: 第2章 网络IO多路复用（讲义）.md §2.1.12
// @expect: 同样 3 条消息加上 2 字节长度头后, 服务端能稳定切出 3 条完整消息
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

void put_frame(std::string& out, const std::string& body) {
    unsigned short len = static_cast<unsigned short>(body.size());
    out.push_back(static_cast<char>((len >> 8) & 0xff));
    out.push_back(static_cast<char>(len & 0xff));
    out += body;
}

}  // namespace

int main() {
    ::alarm(15);
    unsigned short port = 0;
    int listen_fd = make_listener(port);
    if (listen_fd < 0) { std::printf("socket setup failed\n"); return 1; }

    std::thread server([&] {
        int conn = ::accept(listen_fd, nullptr, nullptr);
        if (conn < 0) { std::printf("[server] accept timeout\n"); return; }
        timeval tv{2, 0};
        ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        std::string buffer;
        char chunk[256];
        int frames = 0;
        for (;;) {
            ssize_t n = ::recv(conn, chunk, sizeof(chunk), 0);
            if (n <= 0) break;
            buffer.append(chunk, static_cast<size_t>(n));
            for (;;) {
                if (buffer.size() < 2) break;
                unsigned short len = static_cast<unsigned short>(
                    (static_cast<unsigned char>(buffer[0]) << 8) |
                    static_cast<unsigned char>(buffer[1]));
                size_t frame_len = 2u + static_cast<size_t>(len);
                if (buffer.size() < frame_len) break;
                ++frames;
                std::printf("[server] 切出第%d条消息: %.*s\n", frames, len, buffer.data() + 2);
                ::send(conn, buffer.data(), frame_len, 0);
                buffer.erase(0, frame_len);
            }
        }
        std::printf("[server] 共切出 %d 条消息\n", frames);
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
    std::string wire;
    put_frame(wire, "AAA");
    put_frame(wire, "BBBBB");
    put_frame(wire, "CC");
    ::send(cli, wire.data(), wire.size(), 0);
    char back[256];
    size_t want = wire.size();
    size_t got = 0;
    while (got < want) {
        ssize_t n = ::recv(cli, back + got, want - got, 0);
        if (n <= 0) break;
        got += static_cast<size_t>(n);
    }
    std::printf("[client] 一次send发出%zu字节(含3个长度头), 回读%zu字节\n", wire.size(), got);
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
