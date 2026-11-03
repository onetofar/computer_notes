// @book: 网络多人在线游戏开发实战教程
// @topic: mysql-query-cost
// @source: 第7章 MySQL数据库（讲义）.md §7.6
// @expect: 20 万行表里查最后一行, 全表扫描要做 20 万次账号比较, 命中行号 199999
#include <chrono>
#include <cstdio>
#include <string>
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
    std::vector<std::string> table;
    table.reserve(kRows);
    for (int i = 0; i < kRows; ++i) table.push_back(account_of(i));

    const std::string target = account_of(kRows - 1);  // 最后一行: 扫描的最坏情况
    long comparisons = 0;
    int found = -1;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kRows; ++i) {
        ++comparisons;                             // 每行都要比一次账号
        if (table[i] == target) { found = i; break; }
    }
    auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("fullscan: rows=%d comparisons=%ld found_row=%d elapsed_ms=%.1f\n",
                kRows, comparisons, found, ms);
    std::printf("rows_scanned=%ld\n", comparisons);
    return 0;
}
