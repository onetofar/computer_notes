# syscall-cost-and-tracing: 一次系统调用多贵？观测它要付出多少代价？

> 来源讲义：`Chapter 3. Operating Systems（讲义）.md` §3.2.2–3.2.3、`Chapter 4. Observability Tools（讲义）.md` §4.2.3/§4.6、`Chapter 13. perf（讲义）.md` §13.8、`Chapter 15. BPF（讲义）.md` §15.1

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-syscall-vs-vdso.c` | 直接测量：真系统调用（`getpid()`）vs vDSO 用户态读钟（`clock_gettime()`） | `getpid()` 约 110 ns/次，`clock_gettime()` 约 20 ns/次，差 5–6 倍 |
| `02-strace-overhead.sh` | 用 `strace -c` 追踪同一负载，看**观测本身**的代价 | 被追踪的程序自身耗时涨几十倍；差值 ÷ 被追踪的 syscall 数 = 每次 syscall 的观测开销（约 20 µs） |
| `03-kernel-observability-userland.sh` | 为什么需要内核态观测：打印 ftrace/BPF/perf 的可用性，再对比用户态能拿到什么 | `/proc` 只有累计计数（minflt/utime/上下文切换）；逐次事件必须靠内核态追踪 |
| `04-perf-stat-basics.sh` | 用最便宜的内核计数器摸事件速率 | `perf stat` 输出 task-clock / context-switches / page-faults；`perf_event_paranoid` 受限时打印说明并 exit 0 |

## 教材原话

> Switching between kernel and user modes is a mode switch.

> All system calls mode switch. Some system calls also context switch: those that are blocking, such as for disk and network I/O, will context switch so that another thread can run while the first is blocked.

> Kernel and user mode have their own software execution contexts, including a stack and registers.

> Tracing can incur higher CPU and storage overheads than profiling, which can slow the target of tracing.

> measured timestamps may also be skewed by the tracer.

> strace(1) also costs high overhead, so the benchmark results while using strace(1) should be discarded.

> perf stat is efficient: it counts software events in kernel context and hardware events using PMC registers.

> When multiple observability tools have overlapping coverage, you can use them to cross-check each other. Ideally, they will source different instrumentation frameworks to check for bugs in the frameworks as well.

## 跑法

```bash
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/s1 01-syscall-vs-vdso.c && /tmp/s1
bash 02-strace-overhead.sh
bash 03-kernel-observability-userland.sh
bash 04-perf-stat-basics.sh
```

所有脚本在缺工具（`strace` / `perf` 未安装）或权限不足（`perf_event_paranoid` 过高）时**打印说明并以 0 退出**，不会报错中断。

## 结论

- **模式切换 ≠ 上下文切换**：每次系统调用都切换特权级，但线程仍在同一个 CPU 上跑；只有阻塞型调用才会换线程。
- **观测有代价**：`02` 里被 `strace` 跟踪的程序慢了约 70 倍。**用 strace 看到的数字不能当基准** —— 这不是小偏差，是把被测对象换了一个。
- **用户态看不见逐次事件**：`/proc` 只给累计计数。要知道「哪一次调用慢多久」，必须在内核态观测（ftrace/BPF），而那些需要 root。
- **先用便宜的**：`perf stat` 数事件速率，确认开销可接受，再用贵的 `perf record` 采样。
