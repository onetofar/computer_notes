// @book: 网络多人游戏架构与编程
// @topic: socket-io-multiplexing
// @source: 第3章 伯克利套接字（讲义）.md §3.6 阻塞：游戏服务器的头号天敌
// @expect: 5 条连接里只要有一条没数据, 单线程阻塞 recv 就让全服(含游戏模拟)一起停
#include <cstdio>
#include <vector>

namespace {

struct Conn {
    int id;
    bool has_data;
    int bytes;
};

}  // namespace

int main() {
    std::vector<Conn> conns = {{1, false, 0}, {2, true, 64}, {3, true, 32}, {4, false, 0},
                                {5, true, 16}};
    std::printf("[阻塞] 服务器有 5 条 TCP 连接, 主循环每 tick 依次 recv 每条\n");
    std::printf("[阻塞] 连接状态: #1 无数据, #2 有64, #3 有32, #4 无数据, #5 有16\n");

    // 场景 A: 连接 #1 没数据, 线程睡死
    int ticks = 0;
    int served_bytes = 0;
    int served_conns = 0;
    for (int round = 1; round <= 3; ++round) {
        std::printf("[阻塞] tick=%d 开始, 在连接#1 上 recv ...\n", round);
        bool stuck = false;
        for (const Conn& c : conns) {
            if (!c.has_data) {
                std::printf("[阻塞]   连接#%d 没数据 -> 睡死在这里, 本 tick 后面的连接没机会处理\n",
                            c.id);
                stuck = true;
                break;
            }
            served_bytes += c.bytes;
            ++served_conns;
            ++ticks;
        }
        if (stuck) {
            std::printf("[阻塞] tick=%d 根本没跑完: 游戏模拟也一帧没走\n", round);
        }
    }
    std::printf("[阻塞] 场景A 三轮下来: 完成 recv %d 次, 收到 %d 字节, 模拟帧数=%d\n",
                served_conns, served_bytes, ticks);

    // 场景 B: 所有连接都有数据, 同样的循环一次跑完
    std::vector<Conn> busy = {{1, true, 8}, {2, true, 64}, {3, true, 32}, {4, true, 16},
                               {5, true, 16}};
    int busy_ticks = 0;
    int busy_bytes = 0;
    for (const Conn& c : busy) {
        busy_bytes += c.bytes;
        ++busy_ticks;
    }
    std::printf("[阻塞] 场景B 每条连接都有数据时: 一次循环收满 %d 条 / %d 字节\n", busy_ticks,
                busy_bytes);
    std::printf("[阻塞] 结论: 一个慢客户端就能冻住全服 -> 出路是多线程 / 非阻塞 / select\n");
    return ticks == 0 ? 0 : 1;
}
