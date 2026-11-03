// @book: 网络多人游戏架构与编程
// @topic: object-replication-strategies
// @source: 第5章 对象复制（讲义）.md §5.4 世界状态增量：从"照片"到"只寄变化"
// @expect: 增量包远小于全量; 但创建包一丢, 后到的更新只能跳过, 对象一直缺位
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
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

enum class Action : std::uint8_t { Create = 1, Update = 2, Destroy = 3 };

// 增量包: [动作][网络ID][可选类ID][对象数据]
Bytes BuildDelta(const std::vector<Object>& dirty, Action act, bool with_class_id) {
    Bytes b;
    for (const Object& o : dirty) {
        b.push_back(static_cast<std::uint8_t>(act));
        PutU32(b, o.net_id);
        if (with_class_id) {
            b.push_back(o.class_id);
        }
        PutU32(b, static_cast<std::uint32_t>(o.x));
        PutU32(b, static_cast<std::uint32_t>(o.hp));
    }
    return b;
}

int main() {
    std::map<std::uint32_t, Object> local;

    // --- 场景 A: 创建包先到, 更新包后到 ---
    Bytes create = BuildDelta({{9, 1, 111, 70}}, Action::Create, true);
    Bytes update = BuildDelta({{9, 1, 222, 60}}, Action::Update, false);

    std::size_t h = 0;
    const Action act = static_cast<Action>(create[h++]);
    Object o;
    o.net_id = GetU32(create, h);
    o.class_id = create[h++];
    o.x = static_cast<std::int32_t>(GetU32(create, h));
    o.hp = static_cast<std::int32_t>(GetU32(create, h));
    local[o.net_id] = o;
    std::printf("[增量] 场景A 创建包动作=%u net_id=%u -> 本地对象数=%zu\n",
                static_cast<unsigned>(act), o.net_id, local.size());

    h = 0;
    const Action act2 = static_cast<Action>(update[h++]);
    const std::uint32_t id2 = GetU32(update, h);
    const bool known = local.find(id2) != local.end();
    std::size_t skipped = 0;
    if (known) {
        local[id2].x = static_cast<std::int32_t>(GetU32(update, h));
        local[id2].hp = static_cast<std::int32_t>(GetU32(update, h));
    } else {
        // 对象不存在: 必须把这段数据从流里"读掉", 否则流位置错位
        skipped = update.size() - h;
    }
    std::printf("[增量] 场景A 更新包动作=%u net_id=%u 已知=%s 跳过字节=%zu x=%d\n",
                static_cast<unsigned>(act2), id2, known ? "yes" : "no", skipped,
                known ? local[id2].x : -1);

    // --- 场景 B: 创建包丢了, 更新包先到 ---
    std::map<std::uint32_t, Object> local2;
    h = 0;
    const Action act3 = static_cast<Action>(update[h++]);
    const std::uint32_t id3 = GetU32(update, h);
    const bool known2 = local2.find(id3) != local2.end();
    const std::size_t skip3 = update.size() - h;   // 对象不存在, 整段必须读掉
    std::printf("[增量] 场景B 创建包丢失 -> 更新包先到 动作=%u net_id=%u 本地有没有=%s 必须整段跳过=%zu 字节\n",
                static_cast<unsigned>(act3), id3, known2 ? "yes" : "no", skip3);
    std::printf("[增量] 场景B 更新被跳过, 对象一直缺位, 只能等下一次全量或再来一个创建\n");

    // --- 带宽账 ---
    const std::size_t world_bytes = 1 + 6 * (4 + 1 + 4 + 4);
    const std::size_t delta_bytes = create.size() + update.size();
    std::printf("[增量] 世界全量=%zu 字节/包; 两个增量包合计=%zu 字节\n", world_bytes,
                delta_bytes);
    std::printf("[增量] 增量靠历史: 正确性建立在发送方精确知道接收方现状之上\n");
    return (local.size() == 1 && local[9].x == 222 && local2.empty()) ? 0 : 1;
}
