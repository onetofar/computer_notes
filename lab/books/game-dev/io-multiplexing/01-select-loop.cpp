// @book: 网络多人在线游戏开发实战教程
// @topic: io-multiplexing
// @source: 第2章 网络IO多路复用（讲义）.md §2.1
// @expect: 每轮 select 前都要重建 fd_set, 8 轮 x 64 个 fd = 512 次 FD_SET; 返回的就绪 fd 合计 64
#include <cstdio>
#include <vector>

#if defined(__linux__)
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kSockets = 64;  // 64 对 socketpair, 只把读端交给 select
constexpr int kRounds = 8;    // 8 轮, 每轮让 8 个 fd 就绪
constexpr int kPerRound = 8;

}  // namespace

int main() {
    std::vector<int> readers;
    std::vector<int> writers;
    for (int i = 0; i < kSockets; ++i) {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            std::printf("socketpair failed\n");
            return 1;
        }
        readers.push_back(sv[0]);
        writers.push_back(sv[1]);
    }
    if (kSockets > FD_SETSIZE) {
        std::printf("fd 数超过 FD_SETSIZE=%d, select 不可用\n", FD_SETSIZE);
        return 1;
    }

    int maxfd = 0;
    for (int fd : readers) maxfd = fd > maxfd ? fd : maxfd;

    long select_calls = 0, rebuild_ops = 0, returned_fd = 0;
    for (int round = 0; round < kRounds; ++round) {
        for (int i = 0; i < kPerRound; ++i) {
            int idx = round * kPerRound + i;
            ::write(writers[idx], "x", 1);  // 让第 idx 个 fd 变为可读
        }
        fd_set rfds;
        FD_ZERO(&rfds);
        for (int fd : readers) {  // 每轮重新把名单塞回去
            FD_SET(fd, &rfds);
            ++rebuild_ops;
        }
        timeval tv{1, 0};
        int n = ::select(maxfd + 1, &rfds, nullptr, nullptr, &tv);
        ++select_calls;
        if (n < 0) { std::printf("select failed\n"); return 1; }
        for (int fd : readers) {
            if (FD_ISSET(fd, &rfds)) {
                char c = 0;
                ::read(fd, &c, 1);
                ++returned_fd;
            }
        }
    }
    std::printf("select: sockets=%d rounds=%d select_calls=%ld rebuild_fd_ops=%ld returned_fd=%ld FD_SETSIZE=%d\n",
                kSockets, kRounds, select_calls, rebuild_ops, returned_fd, FD_SETSIZE);
    for (int fd : readers) ::close(fd);
    for (int fd : writers) ::close(fd);
    return 0;
}

#else

int main() {
    std::printf("本演示使用 POSIX socketpair, 请在 Linux 上运行\n");
    return 0;
}

#endif
