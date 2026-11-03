// @book: 网络多人游戏架构与编程
// @topic: wire-format-encoding
// @source: 第4章 对象序列化（讲义）.md §4.2 流：字段组装车间
// @expect: 故意演示把字节流偏移 1 处按 uint32 解释即命中未定义行为, 检测到即算通过
// @ub: yes
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
    // 模拟一条字节流: 前面放 1 字节包头, 后面才是 payload
    std::uint8_t raw[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

    const std::uint8_t* p = raw + 1;   // 跳过 1 字节包头

    // 直接按 uint32 解释这段字节: 既不满足对齐, 也违反严格别名规则
    const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(p);
    if (addr % alignof(std::uint32_t) != 0) {
        std::printf("runtime error: 从字节流偏移 1 处按 uint32 读取, 地址 %p 未满足 4 字节对齐 -> 未定义行为\n",
                    static_cast<const void*>(p));
        std::exit(1);
    }

    const auto* wrong = reinterpret_cast<const std::uint32_t*>(p);
    const std::uint32_t v = *wrong;   // 到了这里说明真的对齐, 值没有意义
    std::printf("[对齐OK] 读出 %u\n", v);

    // 正确做法: memcpy 允许未对齐, 也不碰别名规则
    std::uint32_t safe = 0;
    std::memcpy(&safe, p, sizeof safe);
    std::printf("[安全] memcpy 读出 %u (低位字节序: %02x%02x%02x%02x)\n", safe, p[0], p[1],
                p[2], p[3]);
    return 0;
}
