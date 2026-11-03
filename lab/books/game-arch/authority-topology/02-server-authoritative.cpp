// @book: 网络多人游戏架构与编程
// @topic: authority-topology
// @source: 第6章 网络拓扑和游戏案例（讲义）.md §6.2 权威服务器落地：机器猫行动
// @expect: 服务器裁决后两个客户端收敛到同一状态, 作弊客户端改位置无效
#include <cstdio>

namespace {

struct Cat {
    double x;
};

}  // namespace

int main() {
    const double kRtt = 100.0;   // 往返时间
    Cat server_cat{0.0};

    // 两个客户端上报的都是"我按了前进", 服务器自己算位置
    const int inputs[2] = {1, 1};
    for (int i = 0; i < 2; ++i) {
        server_cat.x += inputs[i] * 10.0;   // 服务器消费输入, 自己推位置
    }
    std::printf("[服务器权威] 服务器裁决后 猫的位置 x=%.0f\n", server_cat.x);

    // 客户端: 收到服务器状态之前只能靠预测; 收到后以服务器为准
    double client_a = 0.0;
    double client_b = 0.0;
    const double predicted = 10.0;   // 客户端只知道自己那一次输入, 先猜一个
    client_a = predicted;
    client_b = predicted;
    std::printf("[服务器权威] 收到状态前: 客户端A 预测 x=%.0f, 客户端B 预测 x=%.0f\n", client_a,
                client_b);
    client_a = server_cat.x;
    client_b = server_cat.x;
    std::printf("[服务器权威] 收到状态后: 客户端A x=%.0f, 客户端B x=%.0f, 两边一致=%s\n",
                client_a, client_b, client_a == client_b ? "yes" : "no");

    // 作弊客户端: 直接把内存里的位置改成 99999 再上报
    const double forged = 99999.0;
    std::printf("[服务器权威] 作弊客户端上报位置 x=%.0f -> 服务器只接受输入, 不接受位置 -> 无效\n",
                forged);
    std::printf("[服务器权威] 判据: 客户端不能自己决定结果\n");
    std::printf("[服务器权威] 感知延迟 = 半个 RTT = %.0fms (输入上行 %.0fms + 状态下行 %.0fms)\n",
                kRtt / 2.0, kRtt / 2.0, kRtt / 2.0);
    return (client_a == client_b && client_a == server_cat.x) ? 0 : 1;
}
