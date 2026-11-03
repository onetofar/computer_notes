// @book: 网络多人游戏架构与编程
// @topic: transport-reliability-model
// @source: 第2章 互联网（讲义）.md §2.6 传输层：端口、UDP 与 TCP
// @expect: 丢失的分段被重传, 接收端严格按序交付, 但总线上字节数多于净荷
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>
namespace {

// 链路模型(教材 §7.7 的四步): ① 丢包判定 ② 到达时间打标
// ③ 按模拟到达时间插入有序队列 ④ 到点才交给游戏
class Link {
public:
    Link(double loss_rate, double mean_ms, double max_jitter_ms, std::uint32_t seed)
        : rng_(seed), loss_(loss_rate), mean_(mean_ms), jitter_(max_jitter_ms) {}

    void DropNext() { drop_next_ = true; }

    bool Send(int id, double now_ms) {
        if (drop_next_) {
            drop_next_ = false;
            return false;                                  // ① 丢包判定
        }
        if (Unit() < loss_) {
            return false;
        }
        const double arrival = now_ms + mean_ + (Unit() - 0.5) * 2.0 * jitter_;   // ② 打标
        queue_.emplace_back(arrival, id);                                        // ③ 排序入队
        std::sort(queue_.begin(), queue_.end());
        return true;
    }

    std::vector<int> Poll(double now_ms) {
        std::vector<int> out;
        while (!queue_.empty() && queue_.front().first <= now_ms) {              // ④ 放行
            out.push_back(queue_.front().second);
            queue_.erase(queue_.begin());
        }
        return out;
    }

private:
    // 用 mt19937 的整数输出直接换算, 保证跨平台结果一致
    double Unit() { return static_cast<double>(rng_() % 1000000u) / 1000000.0; }
    std::mt19937 rng_;
    double loss_;
    double mean_;
    double jitter_;
    bool drop_next_ = false;
    std::vector<std::pair<double, int>> queue_;
};

}  // namespace

int main() {
    Link link(0.25, 40.0, 10.0, 12345u);   // 25% 丢包, 平均延迟 40ms, 抖动 ±10ms
    const int kSegs = 5;
    const int kSegBytes = 100;
    const double kRto = 120.0;             // 重传超时

    std::vector<bool> received(kSegs + 1, false);
    std::vector<double> sent_at(kSegs + 1, 0.0);
    std::vector<int> delivered;
    int next_to_deliver = 1;
    int retransmits = 0;
    int wire_bytes = 0;
    double now = 0.0;

    for (int seg = 1; seg <= kSegs; ++seg) {
        if (seg == 3) {
            link.DropNext();   // 固定丢掉第 3 段, 场景可复现
        }
        link.Send(seg, now);
        sent_at[seg] = now;
        wire_bytes += kSegBytes;
        now += 10.0;
    }
    std::printf("[TCP] 发出 %d 个分段, 每个 %d 字节 (第 3 段被链路丢弃)\n", kSegs, kSegBytes);

    while (next_to_deliver <= kSegs && now < 2000.0) {
        for (int id : link.Poll(now)) {
            received[static_cast<std::size_t>(id)] = true;
            std::printf("[TCP] t=%6.1fms 分段#%d 到达\n", now, id);
            while (next_to_deliver <= kSegs &&
                   received[static_cast<std::size_t>(next_to_deliver)]) {
                delivered.push_back(next_to_deliver);
                std::printf("[TCP]          -> 交付应用层 分段#%d (第 %zu 个)\n", next_to_deliver,
                            delivered.size());
                ++next_to_deliver;
            }
        }
        for (int seg = 1; seg <= kSegs; ++seg) {
            const bool done = received[static_cast<std::size_t>(seg)];
            if (!done && now - sent_at[static_cast<std::size_t>(seg)] >= kRto) {
                link.Send(seg, now);
                sent_at[static_cast<std::size_t>(seg)] = now;
                ++retransmits;
                wire_bytes += kSegBytes;
                std::printf("[TCP] t=%6.1fms 分段#%d 重传超时, 重新发出\n", now, seg);
                break;
            }
        }
        now += 5.0;
    }

    std::printf("[TCP] 交付 %zu/%d 个分段, 重传 %d 次\n", delivered.size(), kSegs, retransmits);
    std::printf("[TCP] 净荷 %d 字节, 总线上 %d 字节 (重传多花 %d 字节)\n", kSegs * kSegBytes,
                wire_bytes, wire_bytes - kSegs * kSegBytes);
    std::printf("[TCP] 保证: 不丢, 不乱序, 代价是旧字节重传 + 队头阻塞\n");
    return delivered.size() == static_cast<std::size_t>(kSegs) ? 0 : 1;
}
