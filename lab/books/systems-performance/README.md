# Systems Performance

> 讲义来源：`书籍/教材讲义/Systems Performance/`（Chapter 1~16 + 附录 A~E）
原书：Brendan Gregg, *Systems Performance: Enterprise and the Cloud*（第二版，Addison-Wesley，2020）

## 主题目录

| 目录 | 讨论的问题 | 文件数 |
| --- | --- | --- |
| `timing-and-statistics/` | 用什么钟测、统计量怎么选 | 4 |
| `syscall-cost-and-tracing/` | 系统调用多贵、观测多贵 | 4 |
| `cpu-affinity-and-cache/` | 绑核与缓存行怎么影响 | 3 |
| `memory-and-network-latency/` | 内存延迟与网络延迟的量级差 | 4 |
| `io-buffered-vs-sync/` | write() 返回时数据在哪 | 3 |
| `use-method-and-sar/` | 分类诊断与查表 | 3 |

## 本书约定

- 这本书讲的是**测量与观测**，所以主题天然是「同一件事，两种测法/两种写法，差在哪」
- 涉及 `perf`/`sar`/eBPF 的脚本必须在**受限环境下优雅降级**（检测不到就打印说明并 exit 0），
  不能让缺工具变成测试失败
- 文件系统相关对照必须先打印文件系统类型——`fsync()` 在 tmpfs 上几乎不要钱，
  在 ext4 上才真花钱，不分清楚就会得出错误结论
