# USE Method 与 sar：先分类，再查表

> 来源讲义：`附录 A. USE Method： Linux（讲义）.md` §附录 A、`附录 B. sar Summary（讲义）.md` §附录 B、`Chapter 4. Observability Tools（讲义）.md` §4.6

## 对照关系

| 文件 | 机制 | 关键观察 |
| --- | --- | --- |
| `01-use-method-collect.sh` | USE 三分类采集 | 按 U/S/E 分别收集 CPU/内存/网络/磁盘指标 |
| `02-sar-field-table.py` | sar 字段表 | "症状 → 查哪一项"的对照映射 |
| `03-proc-vs-perf-crosscheck.sh` | 工具互证 | perf 的上下文切换/缺页数与 `/proc/self/stat` 同量级 |

## 讨论点

1. **USE 的分类本身就是诊断顺序**。
   U（Utilization）满了 → 看 S（Saturation）→ 再看 E（Errors）。
   直接跳到看错误日志，往往是在最深的枝上找。

2. **两个工具可以互相印证**。`03` 把 `perf stat` 报的上下文切换/缺页数
   与 `/proc/self/stat` 里的累计计数对比——同量级才说明两边都没看错。
   这比单看一个工具的输出可靠得多。

## 跑法

```bash
bash tools/run_all.sh
```

所有脚本在缺少 `sar`/`perf` 时打印说明并以 0 退出，不会让整轮失败。
