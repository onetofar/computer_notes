# 一次查询要花多少功夫：全表扫描 / 索引 / 预处理

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第7章 MySQL数据库（讲义）.md` §7.4 / §7.6 / §7.7

**服务器上没有 MySQL 客户端与服务端**，所以本主题一律是**纯逻辑模拟**：
用 `std::vector` / `std::unordered_map` 复刻"读多少行、比多少次键"这类代价，
不连任何数据库，输出的是可复算的计数而不是"感觉快了"。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-fullscan-sim.cpp` | 20 万行表按账号查最后一行：一行一行比 | `rows=200000 comparisons=200000 found_row=199999`、`rows_scanned=200000` |
| `02-hash-index-sim.cpp` | 先建哈希索引再查（统计键比较次数） | `build_key_inserts=200000 build_equal_ops=211`、`lookup: equal_ops=1 found_row=199999` |
| `03-sorted-index-sim.cpp` | 先排序再二分查找 | `lookup: comparisons=17 found_row=199999`（≈ log₂200000） |
| `04-prepared-stmt-sim.cpp` | 每次执行都拼完整 SQL 让服务器重新解析 vs 预处理模板只解析一次 | `parse_each_total=89890 parse_once=64 bind_each_total=4000`、`parse_ratio=1404.5x` |

四份输出放在一起看：**查同一行，扫描比 20 万次、哈希索引比 1 次、二分比 17 次；
而"每次重新拼 SQL"比"预处理一次"多付了约 1400 倍的解析量。**

## 教材原话

讲义 §7.4 讲预处理时，先说清了"每次重发完整 SQL"有多浪费：

> 往 player 表插一条角色的 SQL 会反复执行（每个新玩家一次）。每次都把完整 SQL 发过去让服务器解析一遍，是浪费。**预处理**把这件事拆开：
>
> ```
> ① mysql_stmt_prepare("insert into player (...) values (?,?,...)")   ← 模板，建一次
> ② mysql_stmt_bind_param(...)    ← 每次执行前，把「?」逐个填上实际值
> ③ mysql_stmt_execute(...)       ← 执行；mysql_stmt_affected_rows 看影响了几行
> ```

—— §7.4

讲义 §7.5 给出框架里的落地方式与它的收益：

> 组件初始化时就把常用 SQL 做成预处理，存进字典：

讲义紧接着给出「建一次，存字典」的代码骨架（`InitStmts()` 把两条预处理登记进 `_mapStmt`），并说明之后的收益：

> 之后「创建角色」协议一到，从字典里按 `StmtCreate` 拿出来、填玩家名字和初始数据、execute、入库。**模板解析只发生一次，之后全是填空。**

—— §7.5

讲义 §7.6 讲查询路径：

> 查询走另一套 API，固定四步：
>
> ```
> mysql_query(...)            → 执行 select
> mysql_store_result(...)     → 整个结果集缓存进 MYSQL_RES（用完必须 mysql_free_result）
> mysql_fetch_row(...)        → 迭代器，一行一行取（MYSQL_ROW）
> mysql_num_fields / mysql_fetch_field → 有几列、列是什么
> ```

—— §7.6

讲义 §7.6 用小标题「⚠️ store_result 返回空 ≠ 出错」提醒了一个常见误判：

> 最容易误判的一点：执行的是 insert / update 这类语句时，`mysql_store_result` 返回空是**正常的**——它们本来就没有结果集。判断成功与否要看错误码，不是看结果集空不空。

—— §7.6

## 关于索引部分（推演，不是讲义结论）

讲义 §7 讲的是**怎么连、怎么写、怎么查、怎么存**，**没有展开索引结构**
（§7.9 讲的是表结构与 protobuf 整列入库）。
本目录里 `01`~`03` 是在 §7.6"一次查询要读多少行"这条线上做的**推演**：
把"扫多少行"折算成"比多少次键"，好让同一张表的三种查法能横向比。
**这三个文件不代表讲义观点**，只代表一个可复算的代价模型。

同理，`04` 里"解析代价用 SQL 文本长度近似"是本实验的简化，
真实解析器的开销与文本长度不成正比，**只有比例关系可用，数字不可直接引用**。

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/mysql-query-cost
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-fullscan-sim.cpp        -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-hash-index-sim.cpp      -o /tmp/02 && /tmp/02
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03-sorted-index-sim.cpp     -o /tmp/03 && /tmp/03
g++ -std=c++20 -O2 -Wall -Wextra -pthread 04-prepared-stmt-sim.cpp    -o /tmp/04 && /tmp/04
```
