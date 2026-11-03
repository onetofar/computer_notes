// @book: 网络多人游戏架构与编程
// @topic: transport-reliability-model
// @source: 第7章 延迟、抖动和可靠性（讲义）.md §7.4 TCP 还是 UDP：TCP 的三宗罪
// @expect: 低优先级的音效包一丢, TCP 让高优先级的枪声包陪着多等一个 RTO
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>

namespace {

// 链路模型(教材 §7.7 四步): ①丢包判定 ②到达时间打标 ③按模拟到达时间排入 ④到点放行
class Link {
public:
    Link(double loss_rate, double mean_ms, double max_jitter_ms, std::uint32_t seed)
        : rng_(seed), loss_(loss_rate), mean_(mean_ms), jitter_(max_jitter_ms) {}

    void DropNext() { drop_next_ = true; }

    bool Send(int id, double now_ms) {
        if (drop_next_) {
            drop_next_ = false;
            return false;
        }
        if (Unit() < loss_) {
            return false;
        }
        const double arrival = now_ms + mean_ + (Unit() - 0.5) * 2.0 * jitter_;
        queue_.emplace_back(arrival, id);
        std::sort(queue_.begin(), queue_.end());
        return true;
    }

    std::vector<int> Poll(double now_ms) {
        std::vector<int> out;
        while (!queue_.empty() && queue_.front().first <= now_ms) {
            out.push_back(queue_.front().second);
            queue_.erase(queue_.begin());
        }
        return out;
    }

private:
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
    Link link(0.0, 40.0, 0.0, 999u);   // 做成不丢不抖的链路, 单独制造音效包丢失
    const double kRto = 120.0;
    std::printf("[HOL] TCP 场景: t=0 同时发出 音效包(低优先级) 与 枪声包(高优先级)\n");
    link.DropNext();
    link.Send(1, 0.0);   // 音效包: 链路丢弃
    link.Send(2, 0.0);   // 枪声包: 正常上路

    std::vector<bool> got(3, false);
    bool sound_delivered = false;  // 低优先级
    bool gun_delivered = false;
    double sound_at = 0.0;
    double gun_at = 0.0;
    int next_to_deliver = 1;
    double now = 0.0;
    bool resent = false;
    while (now < 1000.0 && !gun_delivered) {   // 接收端: 缺一个都不放行
        for (int id : link.Poll(now)) {
            got[static_cast<std::size_t>(id)] = true;
            std::printf("[HOL] t=%6.1fms 包#%d 到达接收端缓冲区\n", now, id);
            if (id == 2 && !sound_delivered) { std::printf("[HOL] 前面缺低优先级的 #1, 只能先存着(队头阻塞)\n"); }
        }
        while (next_to_deliver <= 2 && got[static_cast<std::size_t>(next_to_deliver)]) {
            const int id = next_to_deliver;
            if (id == 1) {
                sound_delivered = true;
                sound_at = now;
            } else {
                gun_delivered = true;
                gun_at = now;
            }
            std::printf("[HOL] t=%6.1fms 按序交付包#%d\n", now, id);
            ++next_to_deliver;
        }
        if (!resent && !sound_delivered && now >= kRto) {
            link.Send(1, now);
            resent = true;
            std::printf("[HOL] t=%6.1fms 音效包重传超时, 重新发出\n", now);
        }
        now += 5.0;
    }
    std::printf("[HOL] TCP: 音效包 t=%.1fms 交付, 枪声包 t=%.1fms 交付\n", sound_at, gun_at);
    std::printf("[HOL] TCP: 枪声包本可以 40ms 就到, 却因为一个音效包丢了多等了 %.1fms\n", gun_at - 40.0);

    Link udp(0.0, 40.0, 0.0, 999u);
    udp.DropNext();
    udp.Send(1, 0.0);
    udp.Send(2, 0.0);
    double udp_gun_at = 0.0;
    bool udp_gun = false;
    for (double t = 0.0; t < 500.0; t += 5.0) {
        for (int id : udp.Poll(t)) {
            if (id == 2) {
                udp_gun = true;
                udp_gun_at = t;
            }
        }
    }
    std::printf("[HOL] UDP: 枪声包 t=%.1fms 交付(不等音效), 音效包永远不到\n", udp_gun_at);
    std::printf("[HOL] 结论: 可靠有序把低优先级数据的命运绑在高优先级数据上\n");
    return (sound_delivered && gun_delivered && udp_gun) ? 0 : 1;
}
