// @book: 网络多人在线游戏开发实战教程
// @topic: mysql-query-cost
// @source: 第7章 MySQL数据库（讲义）.md §7.6
// @expect: 建哈希索引后查同一行只要 1 次键比较, 但建索引本身要写 20 万个键
#include <chrono>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int kRows = 200000;

std::string account_of(int i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "player_%06d", i);
    return buf;
}

struct CountingEqual {
    long* ops;
    bool operator()(const std::string& a, const std::string& b) const {
        ++(*ops);
        return a == b;
    }
};

}  // namespace

int main() {
    std::vector<std::string> table;
    table.reserve(kRows);
    for (int i = 0; i < kRows; ++i) table.push_back(account_of(i));

    long build_ops = 0;
    auto t0 = std::chrono::steady_clock::now();
    std::unordered_map<std::string, int, std::hash<std::string>, CountingEqual> index(
        0, std::hash<std::string>{}, CountingEqual{&build_ops});
    for (int i = 0; i < kRows; ++i) index.emplace(table[i], i);
    auto build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    const std::string target = account_of(kRows - 1);
    long lookup_ops = 0;
    t0 = std::chrono::steady_clock::now();
    auto it = index.find(target);
    auto lookup_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();

    std::printf("hash_index: rows=%d build_key_inserts=%d build_equal_ops=%ld build_ms=%.1f\n",
                kRows, kRows, build_ops, build_ms);
    std::printf("lookup: equal_ops=%ld found_row=%d lookup_us=%.1f\n",
                lookup_ops + (it != index.end() ? 1 : 0), it == index.end() ? -1 : it->second, lookup_us);
    return 0;
}
