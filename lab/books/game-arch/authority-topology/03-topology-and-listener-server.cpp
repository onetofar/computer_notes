// @book: 网络多人游戏架构与编程
// @topic: authority-topology
// @source: 第6章 网络拓扑和游戏案例（讲义）.md §6.1 选拓扑：先算两笔账
// @expect: 8 人房 C/S 8 条连接而 P2P 28 条; 监听服务器把权威交给玩家, 三个代价
#include <cstdio>

int main() {
    const int n = 8;
    const int cs_links = n;                 // 星形: 每个客户端连服务器一条
    const int p2p_links = n * (n - 1) / 2;  // 全互联
    std::printf("[拓扑] 8 人房连接数账: C/S 星形 %d 条, P2P 全互联 %d 条 (人数翻倍连接数翻约 4 倍)\n",
                cs_links, p2p_links);

    const double kUploadPerPeer = 30.0;     // 每个客户端上行 kB/s
    std::printf("[拓扑] 带宽账: C/S 服务器上行=%.0f kB/s (随人数线性涨), 每个客户端恒定 %.0f kB/s\n",
                n * kUploadPerPeer, kUploadPerPeer);
    std::printf("[拓扑]         P2P 每个客户端上行=%.0f kB/s (发给其余 %d 个人, 线性涨摊到每人)\n",
                (n - 1) * kUploadPerPeer, n - 1);
    std::printf("[拓扑] 延迟账: C/S 里 A 的动作要 A->服务器->B 绕一圈, P2P 直连\n");

    // 监听服务器: 让某个玩家的电脑兼任服务器
    std::printf("[拓扑] 监听服务器省钱的办法: 让某个玩家的电脑兼任服务器\n");
    std::printf("[拓扑]   代价1 权威数据在玩家手里 -> 开挂的是服务器本身\n");
    std::printf("[拓扑]   代价2 他一断线全场崩\n");
    std::printf("[拓扑]   代价3 他退出还得迁移主机\n");
    std::printf("[拓扑] 监听服务器不是对等网络: 它仍然有一台服务器, 只是恰巧被玩家托管了\n");
    std::printf("[拓扑] C/S 可扩展的原因: 加人只苦服务器, 不苦玩家\n");
    return p2p_links == 28 ? 0 : 1;
}
