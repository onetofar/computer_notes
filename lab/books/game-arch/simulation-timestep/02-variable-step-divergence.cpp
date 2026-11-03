// @book: 网络多人游戏架构与编程
// @topic: simulation-timestep
// @source: 第6章 网络拓扑和游戏案例（讲义）.md §6.3.3 双钟：渲染一个钟，模拟另一个钟
// @expect: 模拟步长跟着渲染耗时走时, 同样 15 帧在两台机器上推出两条不同的时间线
#include <cstdio>

namespace {

struct Unit {
    double x;
    double vx;
};

}  // namespace

int main() {
    const int kFrames = 15;   // 同样是 15 帧
    Unit fast{0.0, 10.0};
    Unit slow{0.0, 10.0};
    const double fast_frame = 0.033;
    const double slow_frame = 0.050;

    for (int f = 0; f < kFrames; ++f) {
        // 可变步长: 模拟步长 = 实际帧耗时
        fast.x += fast.vx * fast_frame;
        slow.x += slow.vx * slow_frame;
    }

    std::printf("[可变步长] 同样 %d 帧: 快机 x=%.3f (模拟了 %.0fms 游戏时间)\n", kFrames, fast.x,
                fast_frame * 1000.0 * kFrames);
    std::printf("[可变步长] 同样 %d 帧: 慢机 x=%.3f (模拟了 %.0fms 游戏时间)\n", kFrames, slow.x,
                slow_frame * 1000.0 * kFrames);
    std::printf("[可变步长] 位置偏差 %.3f, 连走过的游戏时间都不一样 -> 两条时间线\n",
                slow.x - fast.x);
    std::printf("[可变步长] 8 台机器同样命令会推出 8 条不同的时间线 -> 确定性当场死亡\n");
    std::printf("[可变步长] 固定步长是锁步的前提, 慢是它交的税\n");
    return slow.x != fast.x ? 0 : 1;
}
