// @book: 网络多人游戏架构与编程
// @topic: socket-io-multiplexing
// @source: 第3章 伯克利套接字（讲义）.md §3.6 这一节的下一站
// @expect: epoll 用持久兴趣表, 水平触发下不读干净就反复报, 边缘触发下不读到 EAGAIN 就漏事件
#include <cstdio>
#include <vector>

namespace {

constexpr unsigned kIn = 0x01u;
constexpr unsigned kEdge = 0x02u;

struct Interest {
    int fd;
    unsigned events;
};

}  // namespace

int main() {
    // 内核侧长期保管的兴趣表: ADD/MOD/DEL 改的是它, 不是每轮传的集合
    std::vector<Interest> interest;
    interest.push_back({3, kIn});
    interest.push_back({7, kIn});
    interest.push_back({11, kIn | kEdge});
    std::printf("[epoll] 兴趣表长期持有 %zu 项, 不必每轮重建\n", interest.size());

    // 模拟: fd 7 还有 100 字节没读
    int pending[16] = {0};
    pending[7] = 100;
    pending[11] = 50;

    // 水平触发: 只要还有数据就一直报
    std::printf("[epoll] --- 水平触发(fd=7, 故意只读 40 字节) ---\n");
    for (int round = 1; round <= 2; ++round) {
        bool reported = false;
        for (const Interest& it : interest) {
            if ((it.events & kEdge) == 0 && pending[it.fd] > 0) {
                std::printf("[epoll]   第%d轮 报 fd=%d 可读 (还剩 %d 字节)\n", round, it.fd,
                            pending[it.fd]);
                reported = true;
            }
        }
        if (!reported) {
            std::printf("[epoll]   第%d轮 没人报\n", round);
        }
        if (round == 1) {
            pending[7] -= 40;   // 只读了一部分
        }
    }
    std::printf("[epoll] 水平触发: 不读干净就一直报, 不会漏\n");

    // 边缘触发: 只在状态变化时报一次
    std::printf("[epoll] --- 边缘触发(fd=11, 故意只读 40 字节) ---\n");
    bool edge_notified[16] = {false};
    for (int round = 1; round <= 2; ++round) {
        for (const Interest& it : interest) {
            if ((it.events & kEdge) != 0 && pending[it.fd] > 0 &&
                !edge_notified[static_cast<std::size_t>(it.fd)]) {
                std::printf("[epoll]   第%d轮 报 fd=%d 可读 (还剩 %d 字节)\n", round, it.fd,
                            pending[it.fd]);
                edge_notified[static_cast<std::size_t>(it.fd)] = true;
                pending[it.fd] -= 40;   // 只读 40 字节, 没读到 EAGAIN
            }
        }
    }
    std::printf("[epoll]   第2轮没人再报 fd=11 -> 还剩 %d 字节永远等不到(漏事件)\n", pending[11]);
    std::printf("[epoll] 边缘触发: 必须一次读到 EAGAIN, 否则剩余数据不再通知\n");
    std::printf("[epoll] 对照 select: 集合每轮传一次且会被改写; epoll 的兴趣表是持久的\n");
    return pending[7] == 60 ? 0 : 1;
}
