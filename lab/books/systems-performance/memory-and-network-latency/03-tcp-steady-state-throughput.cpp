// @book: Systems Performance
// @topic: memory-and-network-latency
// @source: Chapter 10. Network（讲义）.md §10.3.5
// @expect: 复用一条连接连续传 64MB, 稳态吞吐远高于"每次重连"的摊薄结果; 瓶颈从延迟变成带宽

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>

namespace {

constexpr long long kTotalBytes = 64LL * 1024 * 1024;
constexpr int kChunk = 65536;

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

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 ||
        listen(srv, 128) != 0) {
        perror("bind/listen");
        close(srv);
        return 1;
    }
    socklen_t len = sizeof addr;
    getsockname(srv, reinterpret_cast<sockaddr*>(&addr), &len);

    std::atomic<bool> done{false};
    long long received = 0;

    // 服务端必须并发读走数据, 否则 socket 缓冲区填满后客户端 write() 会阻塞
    std::thread reader([&] {
        int c = accept(srv, nullptr, nullptr);
        if (c < 0)
            return;
        char* buf = static_cast<char*>(malloc(kChunk));
        if (!buf) {
            close(c);
            return;
        }
        ssize_t n;
        while ((n = read(c, buf, kChunk)) > 0)
            received += n;
        free(buf);
        close(c);
        done.store(true);
    });

    int c = socket(AF_INET, SOCK_STREAM, 0);
    if (c < 0 || connect(c, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        perror("connect");
        close(srv);
        return 1;
    }

    char* buf = static_cast<char*>(malloc(kChunk));
    if (!buf) {
        close(c);
        close(srv);
        return 1;
    }
    memset(buf, 'x', kChunk);

    long long t0 = now_ns();
    long long sent = 0;
    while (sent < kTotalBytes) {
        ssize_t n = write(c, buf, kChunk);
        if (n <= 0)
            break;
        sent += n;
    }
    long long t1 = now_ns();

    shutdown(c, SHUT_WR);
    reader.join();

    double secs = static_cast<double>(t1 - t0) / 1e9;
    std::printf("bytes_sent      : %lld\n", sent);
    std::printf("bytes_received  : %lld\n", received);
    std::printf("elapsed_ns      : %lld\n", t1 - t0);
    std::printf("throughput_MBs  : %.1f\n", static_cast<double>(sent) / secs / 1e6);
    std::printf("note: one connection, one handshake, amortized over the whole transfer.\n");

    free(buf);
    close(c);
    close(srv);
    return 0;
}
