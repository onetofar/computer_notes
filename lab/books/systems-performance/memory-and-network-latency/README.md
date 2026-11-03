# memory-and-network-latency: 延迟还是带宽说了算？

> 来源讲义：`Chapter 7. Memory（讲义）.md` §7.3.1、`Chapter 10. Network（讲义）.md` §10.3.5

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-memory-sequential-vs-random.c` | 同一个 64MB 数组：**顺序**访问 vs **随机**访问 | 顺序约 1 ns/次（带宽数十 GB/s）；随机约 80 ns/次（带宽掉一个数量级） |
| `02-tcp-handshake-cost.cpp` | 回环上 500 次 `connect()`，量**连接建立**成本 | 每次握手几十 us；这是「连接建立延迟」，不是「连接寿命」 |
| `03-tcp-steady-state-throughput.cpp` | 复用**一条**连接连续传 64MB | 吞吐约 4 GB/s；一次握手被摊到整个传输上 |
| `04-latency-vs-bandwidth-model.py` | 把延迟与带宽分开建模，算并发度要多大才跑满带宽 | 小包被延迟支配；BDP 之上并发才有用；串行 8MB 比 64 并发慢 25 倍 |

## 教材原话

> The access time of main memory can be measured by the column address strobe (CAS) latency: the time between sending a memory module the desired address (column) and when the data is available to be read.

> CPU 1 can perform I/O to DRAM A directly, via its memory bus. This is referred to as local memory . CPU 1 performs I/O to DRAM B via CPU 2 and the CPU interconnect (two hops). This is referred to as remote memory and has a higher access latency.

> A common source of issues is the presence of latency outliers caused by TCP retransmits.

> Note that some sources of extra latency are transient and only occur during system load.

> Backlog drops and SYN retransmits are indicators of host overload.

## 跑法

```bash
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/m1 01-memory-sequential-vs-random.c && /tmp/m1
g++ -std=c++20 -O2 -Wall -Wextra -pthread -o /tmp/m2 02-tcp-handshake-cost.cpp && /tmp/m2
g++ -std=c++20 -O2 -Wall -Wextra -pthread -o /tmp/m3 03-tcp-steady-state-throughput.cpp && /tmp/m3
python3 04-latency-vs-bandwidth-model.py
```

两个 TCP 程序都用回环地址（127.0.0.1），不依赖外部网络。

## 结论

- **访问模式比容量更决定性能**：同样的 64MB，顺序和随机的每次访问成本差两个数量级。
- **延迟是往返的属性，带宽是流的属性**：小包被 RTT 卡住时，加并发流就能救回来 —— 这是延迟账没还清，不是带宽不够（`04` 的 depth 表）。
- **「连接延迟」和「连接寿命」只差一个词**：前者是握手（几十 us），后者是这条连接能活多久（分钟）。混用会得出完全相反的结论。
- **离群点是网络的头号敌人**：重传造成的离群点会被平均值藏起来，只能靠分布和逐条记录看见。
