// @book: 网络多人游戏架构与编程
// @topic: socket-io-multiplexing
// @source: 第3章 伯克利套接字（讲义）.md §3.6 路三：select——谁好了叫谁
// @expect: select 用位图集合表示就绪, 每轮必须重建集合, 且 fd 超过 1024 直接装不下
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr int kFdSetSize = 1024;   // FD_SETSIZE: select 的集合上限

// fd_set 的等价物: 一张位图
class FdSet {
public:
    void Zero() {
        bits_.fill(0);
        count_ = 0;
    }

    void Set(int fd) {
        if (fd < 0 || fd >= kFdSetSize) {
            overflow_ = true;       // 超出上限: 直接装不下
            return;
        }
        bits_[static_cast<std::size_t>(fd / 64)] |= std::uint64_t{1} << (fd % 64);
        ++count_;
    }

    bool IsSet(int fd) const {
        if (fd < 0 || fd >= kFdSetSize) {
            return false;
        }
        return (bits_[static_cast<std::size_t>(fd / 64)] >> (fd % 64)) & std::uint64_t{1};
    }

    void Clear(int fd) {
        if (!IsSet(fd)) {
            return;
        }
        bits_[static_cast<std::size_t>(fd / 64)] &= ~(std::uint64_t{1} << (fd % 64));
        --count_;
    }

    int Count() const { return count_; }
    bool overflow() const { return overflow_; }

private:
    std::array<std::uint64_t, kFdSetSize / 64> bits_{};
    int count_ = 0;
    bool overflow_ = false;
};

// select 的等价物: 传入的集合会被改写, 返回后只剩就绪的 fd
int Select(FdSet& readfds, const std::vector<int>& ready) {
    FdSet subset;                       // 内核算出的就绪子集
    for (int fd : ready) {
        if (readfds.IsSet(fd)) {
            subset.Set(fd);
        }
    }
    readfds = subset;                  // ★ 传进去的集合被整体改写成就绪子集
    return readfds.Count();
}

}  // namespace

int main() {
    const std::vector<int> fds = {3, 7, 11, 19, 23};

    FdSet readfds;
    readfds.Zero();
    for (int fd : fds) {
        readfds.Set(fd);
    }
    std::printf("[select] 第一轮: 集合里放了 %d 个 fd, 系统调用 1 次\n", readfds.Count());
    const int n1 = Select(readfds, {7, 19});
    std::printf("[select] 第一轮返回 %d 个就绪, 集合被整体改写成就绪子集 -> 集合里只剩 %d 个 fd\n",
                n1, readfds.Count());

    // 经典坑: 第二轮连接#11 有数据了, 但集合没重建 -> #11 根本不在集合里
    const int n2 = Select(readfds, {11});
    std::printf("[select] 第二轮忘了 FD_ZERO/FD_SET: #11 有数据却返回 %d 个 -> 漏事件\n", n2);

    readfds.Zero();
    for (int fd : fds) {
        readfds.Set(fd);
    }
    const int n3 = Select(readfds, {11});
    std::printf("[select] 第二轮重建集合后: 返回 %d 个 -> #11 不再漏\n", n3);

    // 上限
    FdSet big;
    big.Zero();
    big.Set(1023);
    std::printf("[select] fd=1023 放进去了, 溢出=%s\n", big.overflow() ? "yes" : "no");
    big.Set(1024);
    std::printf("[select] fd=1024 放不进去, 溢出=%s -> FD_SETSIZE=%d 是硬上限\n",
                big.overflow() ? "yes" : "no", kFdSetSize);
    std::printf("[select] 每个集合 4 个位图 x 1024 位, 而且每轮都要重建\n");
    return (n2 == 0 && n3 == 1) ? 0 : 1;
}
