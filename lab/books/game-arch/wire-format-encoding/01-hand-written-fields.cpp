// @book: 网络多人游戏架构与编程
// @topic: wire-format-encoding
// @source: 第4章 对象序列化（讲义）.md §4.2 流：字段组装车间
// @expect: 手写 Write/Read 镜像对把一只猫编码成 30 字节, 读回逐字段一致
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void PutU32(Bytes& b, std::uint32_t v) {          // 网络序(大端), 固定 4 字节
    b.push_back(static_cast<std::uint8_t>(v >> 24));
    b.push_back(static_cast<std::uint8_t>(v >> 16));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v));
}

void PutU16(Bytes& b, std::uint16_t v) {          // 固定 2 字节
    b.push_back(static_cast<std::uint8_t>(v >> 8));
    b.push_back(static_cast<std::uint8_t>(v));
}

void PutName16(Bytes& b, const std::string& s) {  // 定长 16 字节槽位
    for (std::size_t i = 0; i < 16; ++i) {
        b.push_back(i < s.size() ? static_cast<std::uint8_t>(s[i]) : std::uint8_t{0});
    }
}

struct Cat {
    std::uint32_t net_id = 0;
    std::uint16_t health = 0;
    std::uint32_t pos_x = 0;
    std::uint32_t pos_y = 0;
    std::string name;
};

// 写端: 每个字段自己决定怎么进流
void Write(const Cat& c, Bytes& b) {
    PutU32(b, c.net_id);
    PutU16(b, c.health);
    PutU32(b, c.pos_x);
    PutU32(b, c.pos_y);
    PutName16(b, c.name);
}

// 读端: 与写端逐字段严格镜像, 流里没有字段名只有位置
std::size_t ReadU32(const Bytes& b, std::size_t h, std::uint32_t& v) {
    v = (std::uint32_t{b[h]} << 24) | (std::uint32_t{b[h + 1]} << 16) |
        (std::uint32_t{b[h + 2]} << 8) | std::uint32_t{b[h + 3]};
    return h + 4;
}

std::size_t ReadU16(const Bytes& b, std::size_t h, std::uint16_t& v) {
    v = static_cast<std::uint16_t>((static_cast<std::uint16_t>(b[h]) << 8) |
                                    static_cast<std::uint16_t>(b[h + 1]));
    return h + 2;
}

std::size_t ReadName16(const Bytes& b, std::size_t h, std::string& s) {
    s.clear();
    for (std::size_t i = 0; i < 16; ++i) {
        const std::uint8_t ch = b[h + i];
        if (ch == 0) {
            break;
        }
        s.push_back(static_cast<char>(ch));
    }
    return h + 16;
}

}  // namespace

int main() {
    const Cat src{7, 83, 1200, 400, "Fuzzy"};
    Bytes buf;
    Write(src, buf);

    std::printf("[手写] 字段数=5 字节数=%zu\n", buf.size());
    std::printf("[手写] 线格式=");
    for (std::uint8_t c : buf) {
        std::printf("%02x", c);
    }
    std::printf("\n");

    std::size_t h = 0;
    Cat dst;
    h = ReadU32(buf, h, dst.net_id);
    h = ReadU16(buf, h, dst.health);
    h = ReadU32(buf, h, dst.pos_x);
    h = ReadU32(buf, h, dst.pos_y);
    h = ReadName16(buf, h, dst.name);

    const bool same = dst.net_id == src.net_id && dst.health == src.health &&
                      dst.pos_x == src.pos_x && dst.pos_y == src.pos_y &&
                      dst.name == src.name;
    std::printf("[手写] 读回 net_id=%u health=%u pos=(%u,%u) name=%s 一致=%s\n",
                dst.net_id, dst.health, dst.pos_x, dst.pos_y, dst.name.c_str(),
                same ? "yes" : "no");
    std::printf("[手写] 定长槽位代价: name 槽 16 字节, 实际只装了 %zu 字节\n",
                src.name.size());
    return same ? 0 : 1;
}
