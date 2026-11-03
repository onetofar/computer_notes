// @book: 网络多人游戏架构与编程
// @topic: latency-smoothing
// @source: 第8章 改进的延迟处理（讲义）.md §8.2 客户端插值：治幻灯片
// @expect: IP 取 PP 时插值一直有下一个目标; IP 取 PP/2 则滑到一半卡住
#include <cstdio>

namespace {

struct State {
    double t;
    double x;
};

}  // namespace

int main() {
    const double kPP = 66.7;     // 数据包周期: 服务器 15 包/秒
    const double kFrame = 16.67; // 客户端 60fps
    const double kRtt = 100.0;   // 感知延迟地板: 半个 RTT

    // 服务器每 66.7ms 一个状态, 位置每次 +10
    State states[8];
    for (int k = 0; k < 8; ++k) {
        states[k].t = k * kPP;
        states[k].x = k * 10.0;
    }

    for (int mode = 0; mode < 2; ++mode) {
        const double ip = (mode == 0) ? kPP : kPP / 2.0;
        std::printf("[插值] --- IP=%.2f ms %s ---\n", ip, mode == 0 ? "(取 PP, 最优)" : "(取 PP/2)");
        int stall = 0;
        for (int f = 0; f <= 12; ++f) {
            const double now = f * kFrame;
            int newest = 0;
            for (int k = 0; k < 8; ++k) {
                if (states[k].t <= now) {
                    newest = k;
                }
            }
            const double sample = now - ip;
            double shown = states[newest].x;
            const char* mode_txt = "卡住";
            if (sample <= states[0].t) {
                shown = states[0].x;
                mode_txt = "最早状态";
            } else if (sample <= states[newest].t) {
                int lo = 0;
                for (int k = 0; k < 8; ++k) {
                    if (states[k].t <= sample) {
                        lo = k;
                    }
                }
                const int hi = lo + 1 < 8 ? lo + 1 : lo;
                const double span = states[hi].t - states[lo].t;
                const double f2 = span > 0.0 ? (sample - states[lo].t) / span : 0.0;
                shown = states[lo].x + (states[hi].x - states[lo].x) * f2;
                mode_txt = "插值";
            } else {
                ++stall;
            }
            std::printf("[插值] 帧=%2d 墙钟=%6.1f 最新收到包#%d 采样时刻=%6.1f 显示=%6.2f %s\n", f,
                        now, newest, sample, shown, mode_txt);
        }
        std::printf("[插值] IP=%.2f ms 卡住帧数=%d\n", ip, stall);
    }

    std::printf("[插值] 规则: 插值周期 IP 不能小于数据包周期 PP, 否则滑到一半没有新目标\n");
    std::printf("[插值] 代价: 显示延迟 = 1/2 RTT(%.1fms) + IP(%.1fms) = %.1fms\n", kRtt / 2.0,
                kPP, kRtt / 2.0 + kPP);
    std::printf("[插值] 插值仍是保守算法: 只显示服务器两个真实状态之间的状态, 从未猜测\n");
    return 0;
}
