// @book: 网络多人游戏架构与编程
// @topic: authority-topology
// @source: 第8章 改进的延迟处理（讲义）.md §8.0 开局：按了跳跃键，100ms 后人才离地
// @expect: 服务器 15 包/秒时 60fps 客户端每 4 帧画同一个位置(幻灯片), 提到 60 包/秒带宽翻 4 倍
#include <cstdio>

int main() {
    const int kClientFps = 60;
    const int kPacketBytes = 40;
    const int rates[2] = {15, 60};

    for (int rate : rates) {
        const double frames_per_packet = static_cast<double>(kClientFps) / rate;
        const int bytes_per_sec = rate * kPacketBytes;
        std::printf("[同步频率] 服务器 %d 包/秒 -> 客户端 60fps 每个位置要画 %.1f 帧, 下行 %d B/s\n",
                    rate, frames_per_packet, bytes_per_sec);
        if (frames_per_packet > 1.0) {
            std::printf("[同步频率]     -> 幻灯片: 花钱买的高级 GPU 被网络限成了 %d fps\n", rate);
        } else {
            std::printf("[同步频率]     -> 每个位置一帧, 没有跳变\n");
        }
    }
    const int b15 = 15 * kPacketBytes;
    const int b60 = 60 * kPacketBytes;
    std::printf("[同步频率] 15->60 包/秒: 下行 %d B/s -> %d B/s, 带宽 %.1f 倍\n", b15, b60,
                static_cast<double>(b60) / b15);
    std::printf("[同步频率] 代价是带宽, 收益是幻灯片消失 -> 这笔账要按真实上行能力算\n");
    std::printf("[同步频率] 插值能把 15 包/秒抹平成 60fps, 但它治不了半个 RTT 的地板\n");
    return b60 == 4 * b15 ? 0 : 1;
}
