# 环境体检：这条机器能不能编译内核

> 对照问题：讲义说「编译内核很吃内存和 CPU」、客机要先在 BIOS 里打开虚拟化、
> eBPF 有四个前置条件 —— 这些**前置条件**怎么变成一条能跑的体检命令？

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| [01-doctor.sh](./01-doctor.sh) | **shell 版完整体检**（8 项）：内核版本 / 头文件 / proc-version / 虚拟化 / 编译器 / 编译工具 / 内存 / 磁盘 | `体检结论: 7 项通过, 1 项缺失` |
| [02-ebpf-preconditions.sh](./02-ebpf-preconditions.sh) | **eBPF 四个前置条件**逐条判定，外加 BCC 依赖的 `linux-headers-$(uname -r)` | `体检结论: 2 项通过, 3 项缺失` |
| [03-doctor.c](./03-doctor.c) | **C 版体检**（6 项内核侧）：与 01 的对应项**结论必须一致** | `体检结论: 5 项通过, 1 项缺失` |

三个文件都**只读**：不写 `/proc` `/sys`、不改系统状态、不需要 root、跑完自己退出。
缺项只报「MISS」，**不会**让程序失败退出（体检脚本的职责是给结论，不是当门禁）。

> 本机实测（fedora，2026-11-03）：01 与 03 的**重叠 6 项输出逐字节相同** ——
> 内核版本 OK、头文件 MISS、proc/version OK、虚拟化 vmx OK、编译器 OK、内存 OK。
> 01 多出的 2 项（编译工具 / 磁盘）只在 shell 版里。

## 教材原话

> **This is as performing tasks such as building a Linux kernel from source is a very memory- and CPU-intensive process.**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §1.2「为什么内核书要从搭环境写起」

> **Installing a 64-bit Linux guest requires that CPU virtualization extension support (Intel VT-x or AMD-SV) be turned on within the host system's basic input/output system ( BIOS ) settings.**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §2.1「BIOS 里的虚拟化开关（第一个坑）」

> **However, when allocating space for the guest's disk, please be generous. Instead of the usual/default 8 GB suggested, I strongly recommend you make it 50 GB or even more.**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §2.2「磁盘要「给大方点」」

> eBPF 的四个前置条件：
>
> - **Linux kernel 4.0 or later**
> - **Kernel support for BPF**
> - **The BCC or bpftrace frontends installed**
> - **Root access on the target system**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.7「eBPF：四个前置条件」

> **the installation of the BCC toolset includes (and depends upon) the installation of the linux-headers-$(uname -r) package; this linux-headers package exists only for distro kernels (and not for our custom 5.10 kernel that we shall often be running on the guest).**
>
> —— 《Linux Kernel Programming》第 1 章讲义 §3.7

## 跑法

```bash
cd markdown/lab
bash tools/check_one.sh books/linux-kernel/kernel-env-doctor/01-doctor.sh
bash tools/check_one.sh books/linux-kernel/kernel-env-doctor/02-ebpf-preconditions.sh
bash tools/check_one.sh books/linux-kernel/kernel-env-doctor/03-doctor.c
```

## 讨论点

1. **本机最值得记的一条**（两个版本都查出来了）：
   `/lib/modules/7.2.12-200.fc44.x86_64/build` **这个软链接是悬空的** ——
   它指向 `/usr/src/kernels/7.2.12-200.fc44.x86_64`，而那个目录并不存在。
   `ls -d` 看得见软链接本身，`[ -d ]` / `stat()` 才看得出目标没了 ——
   **这就是「表面在、实际不能用」的典型**，也正是体检脚本存在的意义。
   两条路径都判它 MISS，并说明原因。
2. **eBPF 那四条体检结论**（本机）：内核 ≥ 4.0 ✓、内核开了 BPF ✓、
   bpftrace/bcc ✗、root ✗ —— 和讲义 §4.3 里那张逐条对照表的结论一致。
3. 讲义 §3.7 那条「工具链假设」在体检里是可验证的：
   **BCC 依赖 `linux-headers-$(uname -r)`，而本机这包是缺的** ——
   自编译内核上装 BCC 会卡在这里（讲义说的「工具假定你用的是发行版内核」）。
4. 体检脚本**故意不因缺项而失败**：它的产物是「一份结论」，不是「一道门禁」。
   要做门禁，另说（可以看同仓 `static-analysis-report-vs-block` 主题的对照）。
