# man 2 与 man 3 的界线：库 API 和系统调用差多少

> 对照问题：讲义把 man page 分成 1/2/3 区，说「系统调用在第 2 区，库 API 在第 3 区」。
> 这条界线在用户态到底意味着什么？——**同一个操作，走库封装和走真系统调用，差几倍？**

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| [01-libc-clock-gettime.c](./01-libc-clock-gettime.c) | man 3 库 API：glibc 的 `clock_gettime()` 走 vDSO，**不进内核** | `路径: glibc clock_gettime (man 3 库 API -> vDSO, 不进内核)` / `每次开销: 31~34 ns` |
| [02-syscall-clock-gettime.c](./02-syscall-clock-gettime.c) | man 2 系统调用：`syscall(SYS_clock_gettime, …)` 绕过 glibc，**每次进内核** | `路径: syscall(SYS_clock_gettime) (man 2 系统调用, 每次进内核)` / `每次开销: 188~203 ns` |
| [03-libc-getpid.c](./03-libc-getpid.c) | man 3 库 API，但**没有** vDSO 快路径：glibc 的 `getpid()` 照样转发进内核 | `路径: glibc getpid (man 3 库 API, 但无 vDSO 快路径, 转发进内核)` / `每次开销: 133~146 ns` |

三个文件做的是**同一个动作**（读一次时钟 / 取一次 pid），调用次数都是 500000，
输出的「每次开销」可以直接横向比；「有效读数」都等于调用次数，说明读到的值都是合法的。

> 本机实测（fedora，内核 7.2.12-200.fc44.x86_64，gcc 16.2.1，2026-11-03，各跑 3 次）：
> **01 ≈ 31~34 ns，02 ≈ 188~203 ns，03 ≈ 133~146 ns**。
> 即 **走 vDSO 的库 API 比真系统调用快约 6 倍**；
> 而 **03 说明「man 3」并不等于「不进内核」** —— 分区号说的是 man page 放在哪，不是说调用走到哪。

## 教材原话

> **System calls with (2) – for example, fork(2) or fork().2**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.1「man page 的分区约定」（原文照录）

> **Library APIs with (3) – for example, pthread_create(3) or pthread_create().3**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.1

> **Well, it's fiction, really (at least on today's Linux): no man pages actually exist for kernel APIs!**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.1（作者当场补的补丁：内核 API 没有 man page，查内核 API 得去内核文档）

> **the best way to learn something is to do so empirically – not taking anyone's word on anything at all, but trying it out and experiencing it for yourself.**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §0（作者把这四个字单独列成一行大字：**Be Empirical!**）

## 跑法

```bash
cd markdown/lab
bash tools/check_one.sh books/linux-kernel/syscall-vs-library-api/01-libc-clock-gettime.c
bash tools/check_one.sh books/linux-kernel/syscall-vs-library-api/02-syscall-clock-gettime.c
bash tools/check_one.sh books/linux-kernel/syscall-vs-library-api/03-libc-getpid.c
```

手动跑：

```bash
cd books/linux-kernel/syscall-vs-library-api
gcc -std=c11 -O2 -Wall -Wextra 01-libc-clock-gettime.c -o /tmp/01 && /tmp/01
gcc -std=c11 -O2 -Wall -Wextra 02-syscall-clock-gettime.c -o /tmp/02 && /tmp/02
gcc -std=c11 -O2 -Wall -Wextra 03-libc-getpid.c -o /tmp/03 && /tmp/03
```

## 讨论点

1. **01 和 02 是同一件事的两条路**：x86_64 上 `clock_gettime` 有 vDSO 快路径，
   glibc 直接在用户态读时钟源，不惊动内核；用 `syscall()` 强制走系统调用，
   每次都要 `syscall` 指令 + 陷入 + 返回。
2. **03 才是本主题的题眼**：`getpid` 是标准的 man 3 库 API，但**没有** vDSO 路径，
   glibc 只能转发给内核。所以「man 3」只说明**文档在哪一区**，不说明**调用不进内核**。
3. 讲义 §3.1 说内核 API 没有 man page、要查内核文档 —— 那正是这条界线的另一头：
   man 2 / man 3 讲的是**你从用户态怎么调**，内核文档讲的是**内核里到底发生了什么**。
4. 绝对值依机器而定（CPU 频率、投机执行缓解措施、是否开 KPTI 都会影响系统调用开销），
   **能横向比的是比值**。
5. 本批**没有**做「内联汇编直接写 `syscall` 指令」那一档 —— 讲义里没有这一层，
   不做无出处的内容。
