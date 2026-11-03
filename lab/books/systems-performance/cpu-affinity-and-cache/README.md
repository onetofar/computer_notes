# cpu-affinity-and-cache: CPU 到底忙在哪？绑核与缓存行会怎么改变答案

> 来源讲义：`Chapter 6. CPUs（讲义）.md` §6.4.1–6.4.2、`Chapter 3. Operating Systems（讲义）.md` §3.2.9

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-pinned-vs-migrating.cpp` | 4 个线程**绑到 CPU 0** vs 允许调度器迁移 | 均值接近；绑核后**方差更小**（缓存一直是热的） |
| `02-false-sharing-padded.cpp` | 两个线程改**同一 cache line** 的相邻计数器 vs 用 padding 隔到不同 cache line | 相邻版本慢 1.2 倍左右（两个线程必须落在**不同物理核**上才有效果） |
| `03-cache-line-stride.c` | 以 64B 步长 vs 以 4096B 步长遍历 64MB 数组 | 64B 步长预取器跟得上（约 300 MB/s）；4096B 步长每次跨新页（TLB miss）且预取失效（约 70 MB/s） |

## 教材原话

> A typical cache line size for x86 processors is 64 bytes.

> Cache coherency is one of the greatest challenges in designing scalable multiprocessor systems, as memory can be modified rapidly.

> The Linux scheduler also uses logic to avoid migrations when the cost is expected to exceed the benefit, preferring to leave busy threads running on the same CPU where the CPU caches should still be warm (CPU affinity).

> The measure of CPU utilization spans all clock cycles for eligible activities, including memory stall cycles. This can be misleading: a CPU may be highly utilized because it is often stalled waiting for memory I/O, not just executing instructions, as described in the previous section.

## 跑法

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread -o /tmp/c1 01-pinned-vs-migrating.cpp && /tmp/c1
g++ -std=c++20 -O2 -Wall -Wextra -pthread -o /tmp/c2 02-false-sharing-padded.cpp && /tmp/c2
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/c3 03-cache-line-stride.c && /tmp/c3
```

## 结论

- **CPU 利用率高不等于在干活**：利用率把**内存停滞周期**也算进去了。`99% CPU` 既可能是算得快，也可能是卡在等内存上。
- **假共享是布局问题，不是逻辑问题**：两个逻辑上无关的计数器挨在一起，就会在两个核之间来回弹 cache line。padding 到 64 字节边界是最低成本的修法。
- **绑核换的是方差不是均值**：迁移的代价是缓存变冷。绑核让结果更可复现，而不是必然更快。
- **步长决定预取能不能工作**：`03` 里 4096B 步长每次都踩新页，TLB miss 加预取失效，是两个机制叠加。
