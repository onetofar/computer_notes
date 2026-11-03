// @book: 网络多人游戏架构与编程
// @topic: latency-smoothing
// @source: 第7章 延迟、抖动和可靠性（讲义）.md §7.6 对象复制可靠性：重传"最新值"，不是"旧字节"
// @expect: 包丢失后 TCP 式重发旧字节让客户端回退, 重批最新值则直接给当前真值
#include <cstdint>
#include <cstdio>

namespace {

struct Sent {
    std::uint32_t net_id;
    std::int32_t x;
    int tick;
};

}  // namespace

int main() {
    // 服务器: 玩家每 50ms 走一次, 每次 +10
    Sent sent[5];
    std::int32_t x = 0;
    for (int t = 0; t < 5; ++t) {
        x += 10;
        sent[t] = {7, x, t};
    }
    const int lost = 2;
    std::printf("[冗余] 发了 5 个包, tick=%d 的包(x=%d)丢失\n", lost, sent[lost].x);

    // 丢包通知到达时, 玩家已经又走了两步
    const std::int32_t now_x = x;
    std::printf("[冗余] 丢包通知到达时玩家真实位置已是 x=%d (半秒前的旧值是 %d)\n", now_x,
                sent[lost].x);

    // TCP 式: 重发当时的旧字节
    std::printf("[冗余] TCP式重传 -> 重发旧字节 x=%d, 客户端位置倒回 %d\n", sent[lost].x,
                sent[lost].x);

    // 改进: 查账本, 重批现在的最新状态
    std::printf("[冗余] 重批最新值 -> 查现状得到 x=%d, 客户端直接拿到当前真值\n", now_x);

    // 在途优化: 同一属性的更新还在路上, 不必重发
    std::printf("[冗余] 在途优化 -> tick=4 的更新(x=%d)还在路上, 同属性不再重发, 冗余字节=0\n",
                sent[4].x);

    std::printf("[冗余] 三条重批规则: Create 丢了先查对象还在不在; Update 丢了也要查现状; "
                "Destroy 丢了无条件重批\n");
    std::printf("[冗余] 结果: 旧字节方案客户端停在 x=%d, 重批最新值客户端看到 x=%d\n",
                sent[lost].x, now_x);
    return 0;
}
