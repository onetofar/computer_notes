// @book: 网络多人在线游戏开发实战教程
// @topic: mysql-query-cost
// @source: 第7章 MySQL数据库（讲义）.md §7.6
// @expect: 有序索引用二分查找, 20 万行只要约 18 次键比较, 代价是建索引时要排序
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kRows = 200000;

std::string account_of(int i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "player_%06d", i);
    return buf;
}

}  // namespace

int main() {
    std::vector<std::pair<std::string, int>> index;
    index.reserve(kRows);
    for (int i = 0; i < kRows; ++i) index.emplace_back(account_of(i), i);

    auto t0 = std::chrono::steady_clock::now();
    std::sort(index.begin(), index.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    auto build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    const std::string target = account_of(kRows - 1);
    long comparisons = 0;
    int found = -1;
    t0 = std::chrono::steady_clock::now();
    size_t lo = 0, hi = index.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        ++comparisons;
        if (index[mid].first < target) {
            lo = mid + 1;
        } else if (target < index[mid].first) {
            hi = mid;
        } else {
            found = index[mid].second;
            break;
        }
    }
    auto lookup_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();

    std::printf("sorted_index: rows=%d build_ms=%.1f\n", kRows, build_ms);
    std::printf("lookup: comparisons=%ld found_row=%d lookup_us=%.1f\n", comparisons, found, lookup_us);
    return 0;
}
