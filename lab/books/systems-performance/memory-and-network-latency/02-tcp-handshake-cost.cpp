// @book: Systems Performance
// @topic: memory-and-network-latency
// @source: Chapter 10. Network（讲义）.md §10.3.5 Latency：全章的核心概念
// @expect: 回环上每次 connect() 的握手成本约几十 us; 连接建立延迟与连接寿命只差一个词, 含义差一个量级

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <thread>

namespace {

constexpr int kConnections = 500;

long long now_ns()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000000LL +
           static_cast<long long>(ts.tv_nsec);
}

}  // namespace

int main()
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }

    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, 128) != 0) {
        perror("listen");
        close(srv);
        return 1;
    }

    socklen_t len = sizeof addr;
    getsockname(srv, reinterpret_cast<sockaddr*>(&addr), &len);
    std::printf("listening_port  : %d\n", ntohs(addr.sin_port));
    std::fflush(stdout);

    // 服务端必须同时 accept, 否则 backlog 填满后 connect() 会阻塞
    std::atomic<bool> stop{false};
    std::thread acceptor([&] {
        while (!stop.load()) {
            int c = accept(srv, nullptr, nullptr);
            if (c >= 0)
                close(c);
        }
    });

    long long total = 0;
    for (int i = 0; i < kConnections; i++) {
        long long t0 = now_ns();
        int c = socket(AF_INET, SOCK_STREAM, 0);
        if (c < 0) {
            perror("socket");
            break;
        }
        if (connect(c, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
            perror("connect");
            close(c);
            break;
        }
        total += now_ns() - t0;
        close(c);
    }

    stop.store(true);
    shutdown(srv, SHUT_RDWR);
    close(srv);
    acceptor.join();

    std::printf("connections     : %d\n", kConnections);
    std::printf("total_handshake_ns : %lld\n", total);
    std::printf("per_handshake_us   : %.1f\n",
                static_cast<double>(total) / kConnections / 1000.0);
    std::printf("note: connect() carries the TCP handshake; the data path is only one of its costs.\n");
    return 0;
}
