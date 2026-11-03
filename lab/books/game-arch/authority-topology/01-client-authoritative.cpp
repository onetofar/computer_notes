// @book: 网络多人游戏架构与编程
// @topic: authority-topology
// @source: 第6章 网络拓扑和游戏案例（讲义）.md §6.1 选拓扑：先算两笔账
// @expect: 客户端权威时两台显示器为同一只猫打架, 作弊客户端瞬移服务器照单全收
#include <cstdio>

namespace {

struct Report {
    int client;
    double cat_x;
    bool cheat;
};

}  // namespace

int main() {
    // 客户端各算各的, 只把"猫在哪"上报给服务器
    const Report reports[3] = {{1, 100.0, false}, {2, 60.0, false}, {3, 99999.0, true}};

    std::printf("[客户端权威] 三个客户端上报同一只猫的位置:\n");
    for (const Report& r : reports) {
        std::printf("[客户端权威]   客户端#%d 上报 x=%.0f%s\n", r.client, r.cat_x,
                    r.cheat ? " (改了内存里的值)" : "");
    }

    double min_x = reports[0].cat_x;
    double max_x = reports[0].cat_x;
    for (const Report& r : reports) {
        if (r.cat_x < min_x) {
            min_x = r.cat_x;
        }
        if (r.cat_x > max_x) {
            max_x = r.cat_x;
        }
    }
    std::printf("[客户端权威] 服务器没有裁决权: 同一只猫出现 %.0f 与 %.0f 两个位置 -> 两台显示器开始打架\n",
                min_x, max_x);
    std::printf("[客户端权威] 作弊客户端上报 x=%.0f, 服务器照单全收 -> 客户端权威就是作弊前兆\n",
                reports[2].cat_x);
    std::printf("[客户端权威] 反事实: 客户端只发输入(我按了前进), 上行天然只剩输入\n");
    std::printf("[客户端权威] 代价: 每个动作都要等一个来回(RTT), 收益是所有人共享同一份真相\n");
    return max_x - min_x > 1000.0 ? 0 : 1;
}
