// @book: 网络多人游戏架构与编程
// @topic: transport-reliability-model
// @source: 第2章 互联网（讲义）.md §2.6 传输层：端口、UDP 与 TCP
// @expect: UDP 不重传, 丢就是丢了, 而且会乱序; 代价是最新状态可能永远收不到
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
            return false;                                   // ① 丢包判定
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
    Link link(0.0, 40.0, 25.0, 12345u);   // 平均延迟 40ms, 抖动 ±25ms -> 会产生乱序
    const int kDgrams = 5;

    std::vector<int> arrived;
    double now = 0.0;
    for (int d = 1; d <= kDgrams; ++d) {
        if (d == 3) {
            link.DropNext();   // 固定丢掉第 3 个数据报, 场景可复现
        }
        const bool ok = link.Send(d, now);
        std::printf("[UDP] t=%6.1fms 发数据报#%d %s\n", now, d, ok ? "已发出" : "链路丢弃");
        now += 10.0;
    }

    for (double t = now; t < 1000.0; t += 5.0) {
        for (int id : link.Poll(t)) {
            arrived.push_back(id);
            std::printf("[UDP] t=%6.1fms 收到数据报#%d\n", t, id);
        }
    }

    std::size_t out_of_order = 0;
    for (std::size_t i = 1; i < arrived.size(); ++i) {
        if (arrived[i] < arrived[i - 1]) {
            ++out_of_order;
        }
    }
    std::printf("[UDP] 发出 %d 个, 收到 %zu 个, 丢失 %zu 个, 乱序 %zu 次\n", kDgrams,
                arrived.size(), kDgrams - arrived.size(), out_of_order);
    std::printf("[UDP] 丢失的那个再也不会来: 没有重传, 发送端也不关心\n");
    std::printf("[UDP] 没有重传, 没有保序, 也不占带宽 -> 这正是游戏想要的\n");
    std::printf("[UDP] 但 TCP 式的可靠有序在游戏里是毒性: 见 03-head-of-line-blocking.cpp\n");
    const bool as_expected = arrived.size() == static_cast<std::size_t>(kDgrams - 1);
    return as_expected ? 0 : 1;
}
