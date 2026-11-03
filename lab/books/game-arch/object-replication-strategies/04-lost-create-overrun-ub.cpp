// @book: 网络多人游戏架构与编程
// @topic: object-replication-strategies
// @source: 第5章 对象复制（讲义）.md §5.4 世界状态增量：从"照片"到"只寄变化"
// @expect: 故意演示创建包丢失后更新包到达, 读头被带偏后越过包长 -> 未定义行为
// @ub: yes
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
}  // namespace

int main() {
    // 增量包: [动作=Update][网络ID=9][x=0xDEADBEEF 小端][hp=60]
    // 对应的"创建"包已经丢了, 接收端手里没有对象 9
    Bytes packet = {2, 0, 0, 0, 9, 0xEF, 0xBE, 0xAD, 0xDE, 0, 0, 0, 60};

    std::size_t h = 0;
    const std::uint8_t action = packet[h++];        // 动作
    const std::uint32_t id = packet[h] << 24 | packet[h + 1] << 16 | packet[h + 2] << 8 |
                             packet[h + 3];
    h += 4;
    std::printf("[越界] 动作=%u net_id=%u 读头=%zu 包长度=%zu\n", action, id, h, packet.size());

    // 错误实现: 复制头里没有 size 字段, 接收端不知道这条记录有多长,
    // 只好去猜"下一个记录有多长" —— 把 x 的第一个字节当成了长度
    const std::uint8_t guessed_len = packet[h];
    h += 1;
    if (h + guessed_len > packet.size()) {
        std::printf("runtime error: 猜出的记录长度 %u 字节, 读头 %zu 越过包长 %zu -> 未定义行为\n",
                    guessed_len, h, packet.size());
        std::exit(1);
    }

    // 正确做法: 复制头带 size 字段(或先查对象在不在), 整段跳过, 流位置不错
    std::printf("[越界] 本例安全: 读头 %zu <= 包长 %zu\n", h, packet.size());
    return 0;
}
