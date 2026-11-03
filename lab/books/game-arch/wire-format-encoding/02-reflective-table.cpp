// @book: 网络多人游戏架构与编程
// @topic: wire-format-encoding
// @source: 第4章 对象序列化（讲义）.md §4.5 可维护性：省到最后省出了 bug 温床
// @expect: 成员表驱动的通用 Serialize 产出与手写版逐字节相同的 30 字节
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

struct Cat {
    std::uint32_t net_id;
    std::uint16_t health;
    std::uint32_t pos_x;
    std::uint32_t pos_y;
    char name[16];
};

enum class Kind { U32, U16, Name16 };
struct FieldDesc { const char* name; Kind kind; std::size_t offset; };

// 穷人版反射: 手写或工具生成的"成员变量表"(名字 + 类型 + 内存偏移)
const FieldDesc kCatFields[] = {
    {"net_id", Kind::U32, offsetof(Cat, net_id)},
    {"health", Kind::U16, offsetof(Cat, health)},
    {"pos_x",  Kind::U32, offsetof(Cat, pos_x)},
    {"pos_y",  Kind::U32, offsetof(Cat, pos_y)},
    {"name",   Kind::Name16, offsetof(Cat, name)},
};

// 定宽写入: 网络序(大端), 宽度由调用方指定
void PutFixed(Bytes& b, std::uint64_t v, int width) {
    for (int i = width - 1; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

std::uint64_t GetFixed(const Bytes& b, std::size_t& h, int width) {
    std::uint64_t v = 0;
    for (int i = 0; i < width; ++i) {
        v = (v << 8) | std::uint64_t{b[h++]};
    }
    return v;
}

// 通用 Serialize: 查表拿偏移, 按类型分发
void Serialize(const Cat& c, Bytes& b) {
    for (const FieldDesc& f : kCatFields) {
        const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&c) + f.offset;
        switch (f.kind) {
            case Kind::U32: {
                std::uint32_t v = 0;
                std::memcpy(&v, p, sizeof v);
                PutFixed(b, v, 4);
                break;
            }
            case Kind::U16: {
                std::uint16_t v = 0;
                std::memcpy(&v, p, sizeof v);
                PutFixed(b, v, 2);
                break;
            }
            case Kind::Name16:
                for (std::size_t i = 0; i < 16; ++i) {
                    b.push_back(p[i]);
                }
                break;
        }
    }
}
}  // namespace

int main() {
    Cat src{};
    src.net_id = 7;
    src.health = 83;
    src.pos_x = 1200;
    src.pos_y = 400;
    std::memcpy(src.name, "Fuzzy", 6);

    Bytes buf;
    Serialize(src, buf);
    std::printf("[反射] 成员表条目=%zu 字节数=%zu\n",
                sizeof(kCatFields) / sizeof(kCatFields[0]), buf.size());
    std::printf("[反射] 线格式=");
    for (std::uint8_t c : buf) {
        std::printf("%02x", c);
    }
    std::printf("\n");

    std::size_t h = 0;
    const auto net_id = static_cast<std::uint32_t>(GetFixed(buf, h, 4));
    const auto health = static_cast<std::uint16_t>(GetFixed(buf, h, 2));
    const auto pos_x = static_cast<std::uint32_t>(GetFixed(buf, h, 4));
    const auto pos_y = static_cast<std::uint32_t>(GetFixed(buf, h, 4));
    char name[16] = {};
    std::memcpy(name, &buf[h], 16);
    const bool same = net_id == src.net_id && health == src.health && pos_x == src.pos_x &&
                      pos_y == src.pos_y && std::memcmp(name, src.name, 6) == 0;
    std::printf("[反射] 读回 net_id=%u health=%u pos=(%u,%u) name=%s 一致=%s\n", net_id, health,
                pos_x, pos_y, name, same ? "yes" : "no");
    std::printf("[反射] 新增字段只需往成员表加一行, 读写两端同时生效\n");
    std::printf("[反射] 代价: 每次读写多一次查表 + 一次按类型分发\n");
    return same ? 0 : 1;
}
