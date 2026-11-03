# timing-and-statistics: 一次操作要多久, 取决于你拿哪个钟、测几次、报哪个数

> 来源讲义：`Chapter 2. Methodologies（讲义）.md` §2.8.2–2.8.6、`Chapter 3. Operating Systems（讲义）.md` §3.2.2、`Chapter 12. Benchmarking（讲义）.md` §12.3.6–12.3.9

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-gettimeofday-wallclock.c` | 用 `gettimeofday()` 取墙钟；接口是 us 粒度，值会被 NTP 步进/校时改动 | 打印墙钟时间戳 + 与 `CLOCK_MONOTONIC` 的差值（当前约几百 ns）；附一句「墙钟不单调」 |
| `02-clock-gettime-monotonic.c` | 用 `clock_gettime(CLOCK_MONOTONIC)`；并与 `CLOCK_MONOTONIC_RAW` 对比 | 分辨率 1 ns；两者差值即 NTP 平滑率（ppm 级）；两者都不因步进而跳变 |
| `03-repeat-percentiles.c` | 同一操作跑 1000 次，排序后取分位数 | `op_ns_first`（单次基准会报的数字）可能比中位数大两个数量级（实测 3701 ns vs 35 ns）；`p95`/`max` 把尾部暴露出来 |
| `04-outlier-stats.py` | 给定一组样本，比均值 / 中位数 / 截尾均值 / 变异系数 | 均值被 2% 的离群点拉高到 3.3，中位数只有 0.9；`mean - median` 的差就是离群点藏身处 |

## 教材原话

> Averages are a summary statistic that hides details. I’ve analyzed many cases of occasional disk I/O latency outliers exceeding 100 ms, while the average latency was close to 1 ms.

> Every time you see an average used as a performance metric, especially an average latency, ask: What is the distribution?

> A standard deviation of 50 alone tells us little. That plus a mean of 200 tells us a lot.

> As a rule, the duration of a benchmark should not be shorter than one second. Short tests might be unusually perturbed by device interrupts (pinning the thread while performing interrupt service routines), kernel CPU scheduling decisions (waiting before migrating queued threads to preserve CPU affinity), and CPU cache warmth effects. Try running the benchmark test several times and examining the standard deviation—this should be as small as possible.

> the compiler may elide simple benchmark routines if it thinks the output is unused and therefore unnecessary to calculate. Always check using other tools while the benchmark is running to confirm its operation.

## 跑法

```bash
# 单个文件
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/t1 01-gettimeofday-wallclock.c && /tmp/t1
python3 04-outlier-stats.py

# 全书一起跑
bash tools/run_all.sh
```

`04-outlier-stats.py` 也可以吃真实样本：把每行一个数字（或 `key: 123.4` 形式）喂给 stdin 或作为参数传入即可。

## 结论

- **测多久**：单次计时给出的是**分布里的一个点**，不是分布。`03` 里首次采样（3701 ns）比中位数（35 ns）大了两个数量级 —— 照着单次结果下结论，等于照着离群点下结论。
- **报什么**：均值会藏细节（`04` 里均值 3.33 vs 中位数 0.90）。要报就报分布，至少带上 `p95`/`max` 和标准差。
- **用哪个钟**：`gettimeofday()` 是**墙钟**，会被 NTP 步进改动；测时间间隔用 `CLOCK_MONOTONIC`。它的分辨率是 1 ns，`gettimeofday()` 的接口只有 us 粒度。
