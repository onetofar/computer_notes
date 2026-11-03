// @book: 网络多人游戏架构与编程
// @topic: simulation-timestep
// @source: 第6章 网络拓扑和游戏案例（讲义）.md §6.3 双钟：渲染一个钟，模拟另一个钟
// @expect: 固定模拟步长下, 33ms 机与 50ms 机跑出同一条轨迹, 慢机只是整体变慢
#include <cstdio>

namespace {

struct Unit {
    double x;
    double vx;
};

}  // namespace

int main() {
    const double kSub = 1.0 / 30.0;   // 固定子轮 33ms
    const int kSubsPerRound = 3;      // 3 个子轮凑成 100ms 一轮
    const int kRounds = 5;
    const int kFrames = kRounds * kSubsPerRound;

    Unit fast{0.0, 10.0};
    Unit slow{0.0, 10.0};
    const double fast_frame = 0.033;   // 好机器: 33ms 一帧
    const double slow_frame = 0.050;   // 破机器: 50ms 一帧
    double fast_clock = 0.0;
    double slow_clock = 0.0;

    for (int f = 0; f < kFrames; ++f) {
        // 每放一帧固定走 1/3 步棋: 模拟步长与渲染帧耗时无关
        fast.x += fast.vx * kSub;
        slow.x += slow.vx * kSub;
        fast_clock += fast_frame;
        slow_clock += slow_frame;
    }

    std::printf("[固定步长] 两台机器各渲染 %d 帧 (%d 轮 x %d 子轮)\n", kFrames, kRounds,
                kSubsPerRound);
    std::printf("[固定步长] 快机 帧耗时%.0fms 墙上时间%.0fms 位置 x=%.3f\n",
                fast_frame * 1000.0, fast_clock * 1000.0, fast.x);
    std::printf("[固定步长] 慢机 帧耗时%.0fms 墙上时间%.0fms 位置 x=%.3f\n",
                slow_frame * 1000.0, slow_clock * 1000.0, slow.x);
    std::printf("[固定步长] 两机世界状态一致=%s (每帧都固定走 %.1fms)\n",
                fast.x == slow.x ? "yes" : "no", kSub * 1000.0);
    std::printf("[固定步长] 慢机只是墙上时间走得久(%.0fms vs %.0fms), 棋谱和谁都一样\n",
                slow_clock * 1000.0, fast_clock * 1000.0);
    return fast.x == slow.x ? 0 : 1;
}
