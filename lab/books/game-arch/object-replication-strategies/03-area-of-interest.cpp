// @book: 网络多人游戏架构与编程
// @topic: object-replication-strategies
// @source: 第9章 可扩展性（讲义）.md §9.1 对象范围和相关性：先裁剪，再复制
// @expect: 相关性裁剪后单客户端字节数远小于全量; 但半径一改带宽就穿帮
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

struct Actor {
    std::uint32_t net_id;
    std::int32_t x;
    std::int32_t y;
    std::int32_t hp;
};

std::int32_t Dist2(const Actor& a, const Actor& b) {
    const std::int32_t dx = a.x - b.x;
    const std::int32_t dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::size_t BytesPerObject() { return 4 + 4 + 4 + 4; }

}  // namespace

int main() {
    std::vector<Actor> world = {
        {1, 0, 0, 100}, {2, 300, 200, 80}, {3, 900, 400, 60},
        {4, 1800, 100, 40}, {5, 2600, 300, 30}, {6, 3400, 500, 20},
        {7, 3900, 900, 10}, {8, 50, 3900, 90},
    };
    const Actor viewer{1, 0, 0, 100};

    std::printf("[相关] 世界对象数=%zu 全量发给一个客户端=%zu 字节\n", world.size(),
                world.size() * BytesPerObject());

    const std::int32_t radii[] = {1000, 2000};
    for (std::int32_t r : radii) {
        const std::int32_t r2 = r * r;
        std::size_t relevant = 0;
        for (const Actor& a : world) {
            if (Dist2(viewer, a) <= r2) {
                ++relevant;
            }
        }
        std::printf("[相关] 半径=%d 米 相关对象=%zu 单客户端字节=%zu (占全量字节 %zu%%)\n", r,
                    relevant, relevant * BytesPerObject(),
                    relevant * BytesPerObject() * 100 / (world.size() * BytesPerObject()));
    }

    std::printf("[相关] 记分牌的血量与距离无关, 永远相关 -> 现实里要按字段拆相关性\n");
    std::printf("[相关] 业务假设写死在底层机制里, 策划改需求就穿帮(狙击枪射程翻倍)\n");
    std::printf("[相关] 顺序: 先决定发给谁, 再谈发什么、发多勤\n");
    return 0;
}
