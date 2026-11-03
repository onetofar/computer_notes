// @book: 网络多人在线游戏开发实战教程
// @topic: io-multiplexing
// @source: 第2章 网络IO多路复用（讲义）.md §2.1
// @expect: poll 数组只建一次不用每帧重塞, 但每次调用仍要扫全部 64 个 fd, 合计扫描 512 次
#include <cstdio>
#include <vector>

#if defined(__linux__)
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kSockets = 64;
constexpr int kRounds = 8;
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

    std::vector<pollfd> pfds(readers.size());
    for (size_t i = 0; i < readers.size(); ++i) {
        pfds[i].fd = readers[i];
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
    }
    long build_ops = static_cast<long>(pfds.size());  // 数组只建一次
    long poll_calls = 0, scanned_fd = 0, returned_fd = 0;
    for (int round = 0; round < kRounds; ++round) {
        for (int i = 0; i < kPerRound; ++i) {
            int idx = round * kPerRound + i;
            ::write(writers[idx], "x", 1);
        }
        int n = ::poll(pfds.data(), pfds.size(), 1000);
        ++poll_calls;
        if (n < 0) { std::printf("poll failed\n"); return 1; }
        scanned_fd += static_cast<long>(pfds.size());  // 每次调用都要把整个数组过一遍
        for (size_t i = 0; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                char c = 0;
                ::read(pfds[i].fd, &c, 1);
                ++returned_fd;
            }
            pfds[i].revents = 0;
        }
    }
    std::printf("poll: sockets=%d rounds=%d poll_calls=%ld build_fd_ops=%ld scanned_fd=%ld returned_fd=%ld\n",
                kSockets, kRounds, poll_calls, build_ops, scanned_fd, returned_fd);
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
