// @book: 网络多人在线游戏开发实战教程
// @topic: io-multiplexing
// @source: 第2章 网络IO多路复用（讲义）.md §2.2
// @expect: 64 个 fd 只做 64 次 epoll_ctl 注册, 之后每次 epoll_wait 只返回就绪的那几个
#include <cstdio>
#include <vector>

#if defined(__linux__)
#include <sys/epoll.h>
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

    int epfd = ::epoll_create1(0);
    if (epfd < 0) { std::printf("epoll_create1 failed\n"); return 1; }
    long ctl_ops = 0;
    for (int fd : readers) {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
            std::printf("epoll_ctl failed\n");
            return 1;
        }
        ++ctl_ops;  // 注册一次, 之后一直有效
    }

    std::vector<epoll_event> events(kSockets);
    long wait_calls = 0, returned_fd = 0;
    for (int round = 0; round < kRounds; ++round) {
        for (int i = 0; i < kPerRound; ++i) {
            int idx = round * kPerRound + i;
            ::write(writers[idx], "x", 1);
        }
        int n = ::epoll_wait(epfd, events.data(), kSockets, 1000);
        ++wait_calls;
        if (n < 0) { std::printf("epoll_wait failed\n"); return 1; }
        for (int i = 0; i < n; ++i) {  // 只处理这次真正就绪的 fd
            char c = 0;
            ::read(events[i].data.fd, &c, 1);
            ++returned_fd;
        }
    }
    std::printf("epoll: sockets=%d rounds=%d epoll_ctl_add=%ld epoll_wait_calls=%ld returned_fd=%ld\n",
                kSockets, kRounds, ctl_ops, wait_calls, returned_fd);
    ::close(epfd);
    for (int fd : readers) ::close(fd);
    for (int fd : writers) ::close(fd);
    return 0;
}

#else

int main() {
    std::printf("本演示需要 Linux 的 epoll, 请在 Linux 上运行\n");
    return 0;
}

#endif
