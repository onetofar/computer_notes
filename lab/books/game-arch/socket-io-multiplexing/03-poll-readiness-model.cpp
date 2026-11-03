// @book: 网络多人游戏架构与编程
// @topic: socket-io-multiplexing
// @source: 第3章 伯克利套接字（讲义）.md §3.6 阻塞：游戏服务器的头号天敌
// @expect: poll 用数组+事件掩码, 没有 1024 上限, 一轮一次系统调用问一批; 延伸演示(讲义 §3.6 只讲了 select 与 epoll, 全讲义未出现 poll( )), 本文件按路二"挨个问=O(N) 次系统调用, 大半是白问"补中间形态
#include <cstdio>
#include <vector>

namespace {

constexpr short kPollIn = 0x001;   // POLLIN
constexpr short kPollOut = 0x004;  // POLLOUT

struct PollFd {
    int fd;
    short events;
    short revents;
};

}  // namespace

int main() {
    std::vector<PollFd> pfds = {{3, kPollIn, 0}, {7, kPollIn, 0}, {11, kPollIn, 0},
                                {5000, kPollIn, 0}, {5001, kPollOut, 0}};
    std::printf("[poll] 一轮 poll 数组里有 %zu 项, 系统调用 1 次\n", pfds.size());
    std::printf("[poll] fd=5000 fd=5001 都能放进去 -> 没有 select 的 %d 上限\n", 1024);
    std::printf("[poll] 事件用掩码表示(POLLIN/POLLOUT), 数组长度自己定\n");

    // 模拟内核返回就绪项
    for (PollFd& p : pfds) {
        p.revents = (p.fd == 7) ? kPollIn : (p.fd == 5000 ? kPollOut : 0);
    }
    int ready = 0;
    for (const PollFd& p : pfds) {
        if (p.revents != 0) {
            ++ready;
            std::printf("[poll] fd=%d 可读 (revents=0x%x)\n", p.fd, p.revents);
        }
    }
    std::printf("[poll] 本轮就绪 %d 个, 未就绪的 %zu 个不用挨个 recv 白问\n", ready,
                pfds.size() - static_cast<std::size_t>(ready));

    // 与"挨个问"的对比
    std::printf("[poll] 对比: 纯非阻塞轮询 %zu 个连接 = %zu 次 recv 系统调用; "
                "poll = 1 次系统调用 + 用户态遍历 %zu 项\n",
                pfds.size(), pfds.size(), pfds.size());
    std::printf("[poll] 代价仍在: 用户态每轮还是要扫一遍 N 项, 而且数组同样要每轮重建\n");
    std::printf("[poll] 下一步: 把这份数组交给内核长期保管 -> epoll\n");
    return ready == 2 ? 0 : 1;
}
