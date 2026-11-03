// @book: 网络多人游戏架构与编程
// @topic: latency-smoothing
// @source: 第8章 改进的延迟处理（讲义）.md §8.3 客户端预测：从"平滑"升级到"推测"
// @expect: 沉默终端显示半 RTT 前的旧位置, 航位推测按速度外推; 猜错后三档纠偏观感不同
#include <cstdio>
#include <cmath>

int main() {
    const double kRtt = 100.0;   // RTT
    const double kAge = kRtt / 2.0;

    // 服务器真值: t=0 时 x=100, 速度 +10/秒
    const double x0 = 100.0;
    const double v0 = 10.0;

    std::printf("[推测] 服务器真值 t=0: x=%.1f 速度=%.1f/秒; RTT=%.0fms -> 数据年龄=%.0fms\n", x0, v0,
                kRtt, kAge);

    // 客户端在 t=50ms 收到 t=0 的状态
    const double truth_at_50 = x0 + v0 * (kAge / 1000.0);
    const double dumb_shown = x0;                          // 沉默终端: 只转述
    const double guessed = x0 + v0 * (kAge / 1000.0);       // 航位推测: 外推 1/2 RTT
    std::printf("[推测] 沉默终端显示 x=%.2f (真相 %.2f, 误差 %.2f) 保守算法, 永不猜测\n",
                dumb_shown, truth_at_50, std::fabs(dumb_shown - truth_at_50));
    std::printf("[推测] 航位推测显示 x=%.2f (真相 %.2f, 误差 %.2f) 乐观算法, 猜对多数\n", guessed,
                truth_at_50, std::fabs(guessed - truth_at_50));

    // 玩家在 t=50ms 转向, 速度变成 -10
    const double v1 = -10.0;
    const double dt = 0.0167;   // 下一个服务器状态到来前又过了 16.7ms
    const double truth_next = truth_at_50 + v1 * dt;
    const double wrong_guess = guessed + v0 * dt;   // 客户端还按老速度外推
    std::printf("[推测] 玩家转向(v=%.0f)后: 真相 x=%.2f, 客户端仍按老速度猜 x=%.2f, 误差 %.2f\n",
                v1, truth_next, wrong_guess, std::fabs(wrong_guess - truth_next));

    // 三档纠偏
    std::printf("[推测] 纠偏档位1 即时更新: 显示从 %.2f 直接跳到 %.2f (闪跳)\n", wrong_guess,
                truth_next);
    const double half = wrong_guess + (truth_next - wrong_guess) * 0.5;
    std::printf("[推测] 纠偏档位2 插值: 显示从 %.2f 平滑滑到 %.2f (小误差用这个)\n", wrong_guess,
                half);
    const double smooth = wrong_guess + (truth_next - wrong_guess) * 0.25;
    std::printf("[推测] 纠偏档位3 二阶调整: 连加速度都平滑修正, 首帧显示 %.2f (最看不出痕迹)\n",
                smooth);

    std::printf("[推测] 本地玩家不能用航位推测, 必须用移动重放: 用服务器状态覆盖后重放未确认输入\n");
    std::printf("[推测] 地板: 半个 RTT 是感知延迟的地板, 所有方案都只在这条线之上做文章\n");
    return 0;
}
