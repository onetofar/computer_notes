// @book: 网络多人游戏架构与编程
// @topic: wire-format-encoding
// @source: 第4章 对象序列化（讲义）.md §4.4 压缩：一比特一比特抠带宽
// @expect: 同一只猫定长 30 字节而变长 12 字节; 0x12345678 主机序与网络序字节不同
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
// 定宽写入: 网络序(大端)
void PutFixed(Bytes& b, std::uint64_t v, int width) {
    for (int i = width - 1; i >= 0; --i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

// 变长整数: 值越小字节越少, 高位为 1 表示后面还有字节
void PutVarU32(Bytes& b, std::uint32_t v) {
    while (v >= 0x80u) {
        b.push_back(static_cast<std::uint8_t>((v & 0x7Fu) | 0x80u));
        v >>= 7;
    }
    b.push_back(static_cast<std::uint8_t>(v));
}

std::uint32_t GetVarU32(const Bytes& b, std::size_t& h) {
    std::uint32_t v = 0;
    std::uint32_t shift = 0;
    while (true) {
        const std::uint8_t byte = b[h++];
        v |= static_cast<std::uint32_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) {
            break;
        }
        shift += 7;
    }
    return v;
}

void PrintHex(const char* tag, const Bytes& b) {
    std::printf("%s=", tag);
    for (std::uint8_t c : b) {
        std::printf("%02x", c);
    }
    std::printf("\n");
}
}  // namespace

int main() {
    const std::uint32_t net_id = 7;
    const std::uint32_t health = 83;
    const std::uint32_t pos_x = 1200;
    const std::uint32_t pos_y = 400;
    const std::string name = "Fuzzy";

    // --- 定长: 每字段固定宽度 ---
    Bytes fixed;
    PutFixed(fixed, net_id, 4);
    PutFixed(fixed, health, 2);
    PutFixed(fixed, pos_x, 4);
    PutFixed(fixed, pos_y, 4);
    fixed.push_back(static_cast<std::uint8_t>(name.size()));
    for (char c : name) {
        fixed.push_back(static_cast<std::uint8_t>(c));
    }

    // --- 变长: varint + 长度前缀 ---
    Bytes var;
    PutVarU32(var, net_id);
    PutVarU32(var, health);
    PutVarU32(var, pos_x);
    PutVarU32(var, pos_y);
    var.push_back(static_cast<std::uint8_t>(name.size()));
    for (char c : name) {
        var.push_back(static_cast<std::uint8_t>(c));
    }

    std::printf("[变长] 定长字节数=%zu 变长字节数=%zu 省了 %zu 字节\n", fixed.size(), var.size(),
                fixed.size() - var.size());
    PrintHex("[变长] 定长线格式", fixed);
    PrintHex("[变长] 变长线格式", var);

    std::size_t h = 0;
    const std::uint32_t r_id = GetVarU32(var, h);
    const std::uint32_t r_hp = GetVarU32(var, h);
    const std::uint32_t r_x = GetVarU32(var, h);
    const std::uint32_t r_y = GetVarU32(var, h);
    const std::size_t n = var[h++];
    std::string r_name;
    for (std::size_t i = 0; i < n; ++i) {
        r_name.push_back(static_cast<char>(var[h++]));
    }
    const bool same = r_id == net_id && r_hp == health && r_x == pos_x && r_y == pos_y &&
                      r_name == name;
    std::printf("[变长] 读回 net_id=%u health=%u pos=(%u,%u) name=%s 一致=%s\n", r_id, r_hp, r_x,
                r_y, r_name.c_str(), same ? "yes" : "no");
    std::printf("[变长] 定长 30 字节里 16 字节是 name 定长槽位, 只装了 %zu 字节\n", name.size());

    // --- 字节序: 主机序 vs 网络序 ---
    const std::uint32_t probe = 0x01020304u;
    std::uint8_t first = 0;
    std::memcpy(&first, &probe, 1);
    const bool host_is_little = (first == 0x04u);

    const std::uint32_t value = 0x12345678u;
    Bytes host_bytes(4);
    std::memcpy(host_bytes.data(), &value, sizeof value);   // 主机序原样落地
    Bytes net_bytes;
    PutFixed(net_bytes, value, 4);                          // 网络序(大端)

    std::printf("[字节序] 本机字节序=%s\n", host_is_little ? "小端" : "大端");
    PrintHex("[字节序] 主机序 0x12345678", host_bytes);
    PrintHex("[字节序] 网络序 0x12345678", net_bytes);
    std::printf("[字节序] 网络序首字节是 0x12=%s; 主机序首字节是 0x%02x\n",
                net_bytes[0] == 0x12u ? "yes" : "no", host_bytes[0]);
    std::printf("[字节序] 不做交换就跨平台读, 高低字节会整个颠倒\n"); return same ? 0 : 1;
}
