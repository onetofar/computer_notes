// @book: 网络多人游戏架构与编程
// @topic: object-replication-strategies
// @source: 第5章 对象复制（讲义）.md §5.3 朴素方案：全量快照
// @expect: 每包自足; 对象从世界消失后下一包自动把它销毁(缺席即销毁)
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>
namespace {

using Bytes = std::vector<std::uint8_t>;

void PutU32(Bytes& b, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

std::uint32_t GetU32(const Bytes& b, std::size_t& h) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v = static_cast<std::uint32_t>((v << 8) | std::uint32_t{b[h++]});
    }
    return v;
}

struct Object {
    std::uint32_t net_id = 0;
    std::uint8_t class_id = 1;
    std::int32_t x = 0;
    std::int32_t hp = 0;
};

}  // namespace

int main() {
    std::map<std::uint32_t, Object> local;   // 接收端的世界

    // 服务器世界: 6 个对象
    std::vector<Object> world = {
        {1, 1, 100, 50}, {2, 1, 200, 50}, {3, 2, 300, 80},
        {4, 2, 400, 80}, {5, 1, 500, 50}, {6, 1, 600, 50},
    };

    for (int tick = 1; tick <= 3; ++tick) {
        if (tick == 2) {
            for (std::size_t i = 0; i < world.size(); ++i) {
                if (world[i].net_id == 5) {
                    world[i].x += 10;      // 动了
                }
            }
        }
        if (tick == 3) {
            world.erase(std::remove_if(world.begin(), world.end(),
                                       [](const Object& o) { return o.net_id == 5; }),
                        world.end());        // 服务器上它没了
        }

        // ---- 写端: 整个世界塞进一个包 ----
        Bytes packet;
        packet.push_back(static_cast<std::uint8_t>(world.size()));
        for (const Object& o : world) {
            PutU32(packet, o.net_id);
            packet.push_back(o.class_id);
            PutU32(packet, static_cast<std::uint32_t>(o.x));
            PutU32(packet, static_cast<std::uint32_t>(o.hp));
        }

        // ---- 读端: 存在就覆盖, 不存在就按类 ID 创建 ----
        std::size_t h = 0;
        const std::uint8_t count = packet[h++];
        std::set<std::uint32_t> present;
        std::size_t created = 0;
        std::size_t updated = 0;
        for (std::uint8_t i = 0; i < count; ++i) {
            Object o;
            o.net_id = GetU32(packet, h);
            o.class_id = packet[h++];
            o.x = static_cast<std::int32_t>(GetU32(packet, h));
            o.hp = static_cast<std::int32_t>(GetU32(packet, h));
            present.insert(o.net_id);
            if (local.find(o.net_id) == local.end()) {
                ++created;
            } else {
                ++updated;
            }
            local[o.net_id] = o;
        }

        // ---- 缺席即销毁: 本地有, 这个包里没出现 -> 销毁 ----
        std::size_t destroyed = 0;
        for (auto it = local.begin(); it != local.end();) {
            if (present.find(it->first) == present.end()) {
                it = local.erase(it);
                ++destroyed;
            } else {
                ++it;
            }
        }

        std::printf("[全量] tick=%d 对象数=%zu 包字节=%zu 创建=%zu 更新=%zu 销毁=%zu 本地=%zu\n",
                    tick, world.size(), packet.size(), created, updated, destroyed, local.size());
    }

    std::printf("[全量] 代价: 每包都带全世界, 字节数随对象数线性涨\n");
    std::printf("[全量] 好处: 每包自足, 丢一包也不影响下一包\n");
    return local.size() == 5 ? 0 : 1;
}
