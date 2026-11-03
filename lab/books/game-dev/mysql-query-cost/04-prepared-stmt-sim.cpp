// @book: 网络多人在线游戏开发实战教程
// @topic: mysql-query-cost
// @source: 第7章 MySQL数据库（讲义）.md §7.4
// @expect: 每次执行都拼整条 SQL 送服务器解析, 1000 次要解析 1000 遍; 预处理后只解析 1 遍, 其余只填参数
#include <chrono>
#include <cstdio>
#include <string>

namespace {

constexpr int kExecutes = 1000;

// 解析代价用 SQL 文本长度近似: 真实解析更贵, 但比例关系不变
long parse_cost(const std::string& sql) { return static_cast<long>(sql.size()); }

std::string make_sql(int sn, const char* name) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "insert into player (sn, account, base, misc) values (%d, '%s', 'blob', 'blob')",
                  sn, name);
    return buf;
}

}  // namespace

int main() {
    long parse_each = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kExecutes; ++i) {
        std::string sql = make_sql(i, "player_000001");  // 每次都拼一条完整 SQL
        parse_each += parse_cost(sql);                   // 每次都让服务器重新解析
    }
    auto each_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::string template_sql = "insert into player (sn, account, base, misc) values (?, ?, ?, ?)";
    long parse_once = parse_cost(template_sql);
    long bind_each = 0;  // 绑定 4 个参数
    t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kExecutes; ++i) {
        (void)template_sql;
        bind_each += 4;
    }
    auto prep_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

    std::printf("prepared: executes=%d parse_each_total=%ld parse_once=%ld bind_each_total=%ld\n",
                kExecutes, parse_each, parse_once, bind_each);
    std::printf("parse_ratio=%.1fx bind_cost_each=4 elapsed_each_path_ms=%.1f elapsed_prepared_path_ms=%.3f\n",
                static_cast<double>(parse_each) / static_cast<double>(parse_once),
                each_ms, prep_ms);
    return 0;
}
