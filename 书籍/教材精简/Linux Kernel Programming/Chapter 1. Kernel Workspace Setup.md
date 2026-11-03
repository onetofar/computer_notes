---
title: Chapter 1. Kernel Workspace Setup（精读）
category: Linux内核
tags:
  - 内核开发环境
  - 虚拟机
  - 交叉编译
  - 内核文档
  - 静态分析
  - eBPF
difficulty: 入门
source: "《Linux Kernel Programming 第二版》（Kaiwan N. Billimoria，Packt 2022，英文原版）Chapter 1 Kernel Workspace Setup"
source_section: "精读 §Technical requirements / §Selecting a Linux distro and kernel / §Installing an x86_64 Linux guest / §Turn on your x86 system's virtualization extension support / §Allocate sufficient space to the disk / §Using pre-built Linux VM images / §Setting up OSBoxes Ubuntu 22.04 as a guest OS / §Installing QEMU and a cross toolchain / §Installing a cross compiler / §A few remaining tips when running the VM / §Experimenting with the Raspberry Pi / §Using the Linux man pages / §The tldr variant / §Locating and using the Linux kernel documentation / §Generating the kernel documentation from source / §Static analysis tools for the Linux kernel / §Linux Trace Toolkit next generation / §The procmap utility / §Simple Embedded ARM Linux System FOSS project / §Modern tracing and performance analysis with eBPF / §The LDV - Linux Driver Verification - project；§More Choices / §Step 1. Install the VirtualBox Guest Additions / §Step 2. Set up a new user account named c2kp / §Step 3. Install required software packages 是所属小节的内部步骤，随该小节立篇，不单独成节；§Additional useful projects 是纯过渡段，其下的工具子节各自立篇；§Join our book community on Discord 不立篇（书务推广页，无技术内容）；§Summary / §Questions / §Further reading 不立篇（复述与外部链接清单，内容已被各节覆盖）"
skills:
  - obsidian-markdown
---

# Chapter 1. Kernel Workspace Setup

> **小节地图**：本篇覆盖 §Technical requirements、§Selecting a Linux distro and kernel、
> §Installing an x86_64 Linux guest（含 §Turn on your x86 system's virtualization extension support、
> §Allocate sufficient space to the disk、§Using pre-built Linux VM images）、
> §Setting up OSBoxes Ubuntu 22.04 as a guest OS（含三个 Step）、§Installing QEMU and a cross toolchain
> （含 §Installing a cross compiler）、§A few remaining tips when running the VM、§Experimenting with the Raspberry Pi、
> §Using the Linux man pages、§The tldr variant、§Locating and using the Linux kernel documentation
> （含 §Generating the kernel documentation from source）、§Static analysis tools for the Linux kernel、
> §Linux Trace Toolkit next generation、§The procmap utility、§Simple Embedded ARM Linux System FOSS project、
> §Modern tracing and performance analysis with eBPF、§The LDV - Linux Driver Verification - project。
> §More Choices、三个 Step、§Installing a cross compiler、§Generating the kernel documentation from source
> 是所属小节的内部子节，随该小节立篇。
>
> **前提（必须先知道）**：本地这份 epub 是 **Packt Early Access 试读本**，目录列到第 13 章，
> **但只包含第 1 章**（见 [[00b-Packt Early Access 说明与全书目录]]）。所以本篇讲的是
> **环境搭建与工具地图**，不是内核机制本身——第 2、3 章才是「源码编译内核」，本书里并不存在。
>
> **本篇天然分两半**（这是读这一章最重要的一件事）：
> **前半**（§Technical requirements ~ §Experimenting with the Raspberry Pi）是**一次性**的装环境步骤，
> 记「2022 年怎么装 Ubuntu 22.04」，会随发行版版本过期；
> **后半**（§Additional useful projects 下的九个工具子节）是**长期有效**的工具地图，
> 工具会换名字，但「查文档 / 看 trace / 上静态分析 / 找真实违规案例」这四类需求永远在。
> 只想带走这一章 20% 的话，带走后半。
>
> 同章另有 [[Chapter 1. Kernel Workspace Setup（讲义）]]（解释层）与 [[01-Chapter 1 Kernel Workspace Setup]]（原文稿，可逐字回查）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §章首 Be Empirical | 01-Chapter 1 Kernel Workspace Setup.md L40 | the best way to learn something is to do so empirically – not taking anyone's word on anything at all, but trying it out and experiencing it for yourself. |
| §Technical requirements 宿主算力 | 01-Chapter 1 Kernel Workspace Setup.md L54 | This is as performing tasks such as building a Linux kernel from source is a very memory- and CPU-intensive process. |
| §Technical requirements 为何用 VM | 01-Chapter 1 Kernel Workspace Setup.md L56 | Working within a guest VM also adds an additional layer of isolation and thus safety. |
| §Technical requirements 代码库先建索引 | 01-Chapter 1 Kernel Workspace Setup.md L65 | For efficient code browsing, I would strongly recommend that you always index the code base(s) with ctags and/or cscope . |
| §Technical requirements 输出环境声明 | 01-Chapter 1 Kernel Workspace Setup.md L67 | Unless noted otherwise, the code output we show in the book is the output as seen on an x86_64 Ubuntu 22.04 LTS guest VM (running under Oracle VirtualBox 6.1). |
| §Selecting a Linux distro and kernel 版本判据 | 01-Chapter 1 Kernel Workspace Setup.md L83 | Our reasoning is simple: all of these are Open Source Software (OSS) with, as of this writing, sufficiently long end-of-life dates, ensuring their continued support and viability for a pretty long while. |
| §Selecting a Linux distro and kernel 崩是常态 | 01-Chapter 1 Kernel Workspace Setup.md L87 | The fact is when working at the level of the kernel, abruptly crashing the system (and the data loss risks that arise thereof) is actually a commonplace occurrence. |
| §Turn on your x86 system's virtualization extension support | 01-Chapter 1 Kernel Workspace Setup.md L140 | Installing a 64-bit Linux guest requires that CPU virtualization extension support (Intel VT-x or AMD-SV) be turned on within the host system's basic input/output system ( BIOS ) settings. |
| §Using pre-built Linux VM images | 01-Chapter 1 Kernel Workspace Setup.md L178 | The OSBoxes (OSB) project allows you to freely download and use prebuilt VirtualBox (as well as VMware) images for popular Linux distributions. |
| §Installing QEMU and a cross toolchain | 01-Chapter 1 Kernel Workspace Setup.md L321 | we need to install a cross toolchain – a set of tools allowing you to build software on one host CPU such that it executes on a different target CPU. |
| §A few remaining tips when running the VM | 01-Chapter 1 Kernel Workspace Setup.md L358 | Finally, to be safe, please do not keep any important data on the guest VM. |
| §Using the Linux man pages 分区约定 | 01-Chapter 1 Kernel Workspace Setup.md L417 | System calls with (2) – for example, fork(2) or fork().2 |
| §The tldr variant 内核 API 无 man page | 01-Chapter 1 Kernel Workspace Setup.md L487 | Well, it's fiction, really (at least on today's Linux): no man pages actually exist for kernel APIs! |
| §Locating and using the Linux kernel documentation | 01-Chapter 1 Kernel Workspace Setup.md L493 | the kernel documentation is always available for that kernel version within the kernel source tree itself, within the directory named Documentation/ . |
| §Static analysis tools for the Linux kernel | 01-Chapter 1 Kernel Workspace Setup.md L511 | They can be tremendously useful to you as the developer, though you must learn how to "tame" them – in the sense that they can result in false positives. |
| §Linux Trace Toolkit next generation | 01-Chapter 1 Kernel Workspace Setup.md L543 | LTTng allows you to trace both userspace (applications) and/or the kernel code paths in minute detail. |
| §The procmap utility | 01-Chapter 1 Kernel Workspace Setup.md L563 | Visualizing the complete memory map of the kernel Virtual Address Space ( VAS ) as well as any given process's user VAS is what the procmap utility is designed to do. |
| §Modern tracing and performance analysis with eBPF | 01-Chapter 1 Kernel Workspace Setup.md L587 | eBPF is essentially virtual machine technology, allowing one to write (small) programs and run them in a safe isolated environment within the kernel! |
| §Modern tracing and performance analysis with eBPF BCC 依赖 | 01-Chapter 1 Kernel Workspace Setup.md L598 | The reason: the installation of the BCC toolset includes (and depends upon) the installation of the linux-headers-$(uname -r) package; this linux-headers package exists only for distro kernels (and not for our custom 5.10 kernel that we shall often be running on the guest). |
| §The LDV - Linux Driver Verification - project | 01-Chapter 1 Kernel Workspace Setup.md L614 | By glancing through these rules, we'll be able to not only see the rule but also instances of actual cases where these rules were violated by driver/kernel code within the mainline kernel, thus introducing bugs. |

> **书里没说的**（本篇有几处推论/判断，原文没有，别当成原文观点）：
> 1. **「一次性 vs 长期有效」这个切分是本篇的说法**，原文只是把前半当安装步骤、后半叫 "additional and useful projects"，
>    没有把它当成论点讲，也没有说「后半更值钱」。
> 2. **「EOL 越晚越好是为了教材能被读者照着跑」是本篇的推演**：原文只给了 "Our reasoning is simple" 与三个 EOL 日期，
>    理由只说成「保证持续支持与可行性」。
> 3. **「工具假定你用发行版内核」是本篇从 BCC 一例抽出的通用教训**，原文只陈述了「自编译 5.10 内核上装不上 BCC」这一事实。
> 4. **「中断要按来源分桶才看得出谁在吃时间」是本篇的推演**，原文只描述了观察到的现象（网卡中断几乎不耗时、i8042 超 200 微秒）。
> 5. **「与目标无关的步骤一律给链接不展开」是对原文一句抱怨的概括**，原文没把它写成方法论。

## §Technical requirements 这一章要你准备什么

这一节只回答一个问题：**要开始动手写内核，机器、代码、习惯上要准备什么**。

### 机器：编译内核很吃资源

原文给的机器要求只有一条逻辑链：

> This is as performing tasks such as building a Linux kernel from source is a very memory- and CPU-intensive process.

⇒ 宿主的 RAM、CPU、磁盘越多越好，别卡在「推荐最低配置」上。

### 形态：原生 Linux 还是虚拟机

作者先承认**原生 Linux 最好**，但两个理由让他选虚拟机：书里不能假设你有一台专用的原生 Linux 机器；以及
虚拟机多一层隔离：

> Working within a guest VM also adds an additional layer of isolation and thus safety.

代价也写明了：**高配原生机器可比虚拟机快到两倍**。

⇒ **（推演）** 这不是「新手教程」，是**风险隔离**：内核层崩溃是家常便饭（见 §Selecting a Linux distro and kernel）。
动手改一个高风险子系统之前，先造一个「坏了也不心疼」的环境。

### 代码库：先 clone，再建索引

书里的完整源码在 GitHub，按章节目录组织（`ch1/` 是本章的代码），根目录是各章共用的代码（如 `convenient.h`、
`klib_llkd.c`）：

```bash
git clone https://github.com/PacktPublishing/Linux-Kernel-Programming_2E
```

原文对「怎么在大代码库里找东西」的答案只有一句，但很硬：

> For efficient code browsing, I would strongly recommend that you always index the code base(s) with ctags and/or cscope .

建索引的做法是进到源码树根目录执行：

```bash
ctags -R .
```

### 一条诚实声明：书里的输出来自哪个环境

> Unless noted otherwise, the code output we show in the book is the output as seen on an x86_64 Ubuntu 22.04 LTS guest VM (running under Oracle VirtualBox 6.1).

⇒ 后面所有输出都是**某一个具体环境**的观测值，别的发行版/版本可能对不上。
这正是本章开篇那句总纲的操作定义：报告你观察到的那个具体环境，而不是给通用结论。

## §Selecting a Linux distro and kernel 版本选择只看 EOL

作者给出三件套：

| 组件 | 选择 | 原文给的生命周期 |
|---|---|---|
| 内核 | 5.10.y LTS（longterm） | EOL 2026 年 12 月 |
| 发行版 | Ubuntu 22.04 LTS（Jammy Jellyfish） | 安全与维护更新保证到 2027 年 4 月，EOL 2032 年 4 月 |
| 虚拟化 | Oracle VirtualBox 6.1.x（跑在宿主上） | — |

判据只有一条，而且是关于**时间**的：

> Our reasoning is simple: all of these are Open Source Software (OSS) with, as of this writing, sufficiently long end-of-life dates, ensuring their continued support and viability for a pretty long while.

⇒ **（推演）** 这条判据其实是**教材可用性的前提**：书出版后读者要照着跑。
书里若写「Ubuntu 20.04」，三年后读者装不上，整本书就作废了。所以「选 LTS」不是保守，是刚需。
注意这条判据是**相对时间**：本书写于 2022 年，读者应按自己动手的日期重新核对 EOL。

### §More Choices 其它可选组合（内部小节）

原文给的替代方案：Ubuntu 20.04 / 18.04 Desktop 也行；CentOS 9（明确**不是** CentOS Stream）、Fedora Workstation；
作为**目标硬件**的 SBC：Raspberry Pi、BeagleBone Black（BBB）；以及「用 QEMU 模拟一台极简自定义 PC」。
作者同时声明：书的代码**只在 x86_64 + Ubuntu 22.04 上测过**，ARM-32/ARM-64 的 Raspberry Pi（Debian 11 bullseye）只是「lightly tested」。

### 为什么坚持用虚拟机：崩是常态

> The fact is when working at the level of the kernel, abruptly crashing the system (and the data loss risks that arise thereof) is actually a commonplace occurrence.

⇒ 这一句是整章的**风险陈述**：内核层干活，系统突然崩掉是家常便饭，虚拟机隔离不是为了方便，是为了**敢崩**。

## §Installing an x86_64 Linux guest 作者为什么不给安装教程

原文明确拒绝展开 VirtualBox 安装细节，理由是「与内核开发没有直接关系」，只给三个链接。

⇒ **（推演）** 这是好教材的自觉：**跟目标无关的步骤一律给链接不展开**。读者要的是内核知识，不是 VirtualBox 安装教程。

但作者仍点出三件必须注意的事。

### §Turn on your x86 system's virtualization extension support

> Installing a 64-bit Linux guest requires that CPU virtualization extension support (Intel VT-x or AMD-SV) be turned on within the host system's basic input/output system ( BIOS ) settings.

这是**第一个坑**：BIOS 里的虚拟化开关。检查办法（Linux 宿主）：

```bash
egrep --color "vmx|svm" /proc/cpuinfo
```

Windows 宿主有两条路：任务管理器 → 性能 → CPU 图形下方有 `Virtualization: Enabled/Disabled`；
或在 cmd 里跑 `systeminfo`，看 "Virtualization Enabled In firmware" 行是 `Yes` 还是 `No`。

确认支持后要**开两次**：进宿主 BIOS（开机按 Del 或 F12，具体看 OEM）找 `Virtualization` / `Virtualization Technology (VT-x)` 设为 Enabled；
然后在 VirtualBox 的 **系统 → 加速** 里勾上硬件虚拟化选项。

⚠️[纠错] 原文同一段里把 AMD 的 flag 写成 `smv`（"smv is the check for AMD processors"），但下一句又写成正确的 `svm`；
`/proc/cpuinfo` 上 AMD 的 flag 实际叫 `svm`。详见文末「纠错补充」。

### §Allocate sufficient space to the disk

内存与 CPU：给 guest 分配 1GB RAM + 2 个 CPU，对多数桌面机就够。

磁盘则相反——**别照默认来**：默认给的是 8GB，原作者强烈建议 **50GB 甚至更多**；
关键是这份空间可以**动态分配 / 按需分配**，hypervisor 会自己「长大」虚拟磁盘，而不是一开始就占满。

⇒ **（推演）** 一个很实用的通用习惯：**把上限开大，把实际占用留给运行时**。

### §Using pre-built Linux VM images

> The OSBoxes (OSB) project allows you to freely download and use prebuilt VirtualBox (as well as VMware) images for popular Linux distributions.

OSBoxes 提供预制镜像，**自带 VirtualBox Guest Additions**；默认账号密码是 `osboxes` / `osboxes.org`。
体积代价：7z 压缩包 2.6GB，解压约 8.6GB，且可动态增长。

Guest Additions 是什么：作者转述为「para-virtualization accelerator software」，对**磁盘 / 网络 I/O 与图形**优化明显。

## §Setting up OSBoxes Ubuntu 22.04 as a guest OS 三步调优

原文列的三步：装 Guest Additions、建 `c2kp` 账号、装软件包。

### §Step 1. Install the VirtualBox Guest Additions

先装最小依赖集：

```bash
sudo apt update
sudo apt upgrade   // if required
sudo apt install gcc make perl build-essential dkms linux-headers-$(uname -r) ssh -y
```

然后 VirtualBox 菜单 **Devices | Insert Guest Additions CD image...**，记下挂载点（可用 `df` 查），执行安装脚本后重启：

```bash
sudo /media/osboxes/VBox_GAs_6.1.26/VBoxLinuxAdditions.run
[...]
```

要用共享文件夹，还需把账号加进 `vboxsf` 组（改完要重新登录或重启才生效）：

```bash
sudo usermod -G vboxsf -a ${USER}
```

### §Step 2. Set up a new user account named c2kp

`c2kp` = **C**ouch **T**o **K**ernel **P**rogrammer，作者玩 C25K（Couch To 5 Kilometers）的梗。

```bash
sudo useradd –m c2kp –s /bin/bash
sudo passwd c2kp
```

会顺带建出同名主组与骨架家目录。原文提醒：密码别学他写 `welcome`（会触发 BAD PASSWORD 警告）。

要让它能 `sudo`，得加入 `adm` 和 `sudo` 组：

```bash
sudo usermod -a -G adm,sudo c2kp
```

验证（注意 `egrep -w` 是按单词匹配）：

```bash
$ egrep -w "adm|sudo" /etc/group
adm:x:4:syslog,osboxes,test1,c2kp
sudo:x:27:osboxes,c2kp
```

### §Step 3. Install required software packages

默认发行版已经带了「系统程序员最小集」（GCC + 头文件 + make）；写**内核空间**代码还要额外装。

编译内核用的：

```bash
sudo apt install -y \
bison build-essential flex libncurses5-dev ncurses-dev \
libelf-dev libssl-dev tar util-linux xz-utils
```

全书其它部分要用的——**这份清单本身就是一份工具地图**（cppcheck、flawfinder、sparse、cscope、exuberant-ctags、
bpfcc-tools、trace-cmd、tldr-py、perf-tools-unstable、smem、hwloc、numactl、tuna……）：

```bash
sudo apt install -y \
bc bpfcc-tools bsdextrautils \
  clang cppcheck cscope curl exuberant-ctags \
fakeroot flawfinder \
git gnome-system-monitor gnuplot hwloc indent \
libnuma-dev linux-headers-$(uname -r) linux-tools-$(uname -r) \
man-db net-tools numactl openjdk-18-jre  openssh-server \
perf-tools-unstable psmisc python3-distutils  \
rt-tests smem sparse stress sysfsutils \
tldr-py trace-cmd tree tuna virt-what
```

磁盘代价：约 1.75GB。原文另外给了一个一键脚本 `pkg_install4ubuntu_lkp.sh`（在书的 GitHub 仓库 `ch1/` 下）。

## §Installing QEMU and a cross toolchain 交叉编译的两条路

要在 x86_64 宿主上做 ARM 内核，缺的是交叉工具链：

> we need to install a cross toolchain – a set of tools allowing you to build software on one host CPU such that it executes on a different target CPU.

两条路：**物理 SBC**（Raspberry Pi 等）或 **QEMU 模拟**（省掉硬件）。

```bash
sudo apt install qemu-system-arm
```

（Fedora 上是 `sudo dnf install qemu-system-arm-<version#>`，按两次 Tab 补全版本号。）

### §Installing a cross compiler

原文说安装细节放到第 3 章（交叉编译内核与内核模块）再讲——本章只负责**把它装上**这个前置条件摆好。

## §A few remaining tips when running the VM 三条能省事的习惯

1. **GUI 开销太大时进控制台模式**：在 bootloader 的内核命令行追加 runlevel `3`。但 VirtualBox 里控制台体验差
   （剪贴板不可用、字号与分辨率不理想）⇒ 作者自己**最常用**的做法是从宿主用 `ssh` / `putty` 远程登录进 VM。
2. **定期更新**（原文称之为 essential security requirement）：

```bash
sudo /usr/bin/update-manager
```

3. **不要在 guest 上放重要数据**：

> Finally, to be safe, please do not keep any important data on the guest VM.

   理由与 §Selecting a Linux distro and kernel 那句是同一条：**崩是家常便饭**，你永远说不准。

## §Experimenting with the Raspberry Pi 第二个目标架构

Raspberry Pi 是信用卡大小的 SBC（USB、microSD、HDMI、音频、以太网、GPIO），SoC 是 Broadcom 的 ARM 核。
作者在 Pi Zero W 与 Pi 4 Model B 上跑书中代码；理由是**跨架构跑一遍能暴露缺陷**：

⇒ **（推演）** 「在第二种目标架构上跑一遍」是本章少数**能长期复用**的方法论之一，
它逼你面对「代码是不是偷偷假设了 x86」这类问题。

工作方式上作者给了两条：HDMI + 键鼠，或更常见的 `ssh`；但 **`ssh` 并不能覆盖所有场景**——
做内核 bring-up 与调试时，**串口控制台**不可替代（USB-to-RS232 TTL 串口线接 GPIO）。

版本信息查询（原文输出是 Debian GNU/Linux 11 bullseye + 5.15.32-v8+ aarch64）：

```bash
rpi $ lsb_release -a
rpi $ uname –a
```

另有硬件信息速查命令：`hostnamectl`、`ls{cpu|pci|usb}`，x86 专用 `hwinfo` / `lshw`，Raspberry Pi OS 专用 `raspinfo`。

作者也提醒：**专业产品开发上 Pi 未必是好选择**（他让读者自己 google 原因），但作为学习与原型环境，
社区与技术爱好者支持让它很难被替代。

## §Using the Linux man pages 分区约定：1/2/3 是什么

Linux/Unix 文献里的括号编号约定：

- 用户命令 `gcc(1)`
- 系统调用 `fork(2)`
- 库 API `pthread_create(3)`

> System calls with (2) – for example, fork(2) or fork().2

括号里的数字就是 man 手册的分区号。`man man` 自己会列出全部分区（原文照抄）：

```bash
$ man man
[...]
       The table below shows the section numbers of the manual followed by the types of pages they contain.
       1   Executable programs or shell commands
       2   System calls (functions provided by the kernel)
       3   Library calls (functions within program libraries)
       4   Special files (usually found in /dev)
       5   File formats and conventions eg /etc/passwd
       6   Games
       7   Miscellaneous (including macro packages and conventions), e.g. man(7), groff(7)
       8   System administration commands (usually only for root)
       9   Kernel routines [Non standard]
[...]
```

⇒ **速查**：`man 2 stat` 或 `man stat.2` 查系统调用；`man 3 stat` 查库函数。
（同一个 `stat` 在 2 区与 3 区都有：2 区是内核提供的系统调用本体，3 区是 libc 的封装。）

## §The tldr variant man 太长时看哪儿

原文的理由：`ps(1)` 这类命令选项极多，man page 很长，只想要一个快速答案时读不动。
tldr = **T**oo **L**ong; **D**idn't **R**ead，给的是「常见用法」页。
项目自述（原文引用）："The tldr pages are a community effort to simplify the beloved man pages with practical examples"。

⇒ **（推演）** tldr 给的是**社区维护的速查**，不是完整规格。
当速查用，别当权威；作者自己也在下一节把「内核 API 没有 man page」这个更大的坑点出来。

CLI 两步：

```bash
sudo apt install -y tldr
```

```bash
tldr --update
```

（`--update` 内部是 clone 一个 git 仓库并填充 `~/.local/share/tldr`。）之后 `tldr tar` 即可。
另有两个入口：主站 `tldr.sh` 与 web client `tldr.ostera.io`（URL 可以直接拼命令名，如 `tldr.ostera.io/tar`）。

**这一节末尾有一个关键补丁**——第 9 区（Kernel routines）其实是虚构的：

> Well, it's fiction, really (at least on today's Linux): no man pages actually exist for kernel APIs!

⇒ 内核 API **没有 man page** ⇒ 查内核 API 必须去**内核文档**，也就是下一节。

## §Locating and using the Linux kernel documentation 查内核 API 的正路

两条路径，含义完全不同：

- **在线最新版**：`https://www.kernel.org/doc/html/latest/`
- **你手上那一版**：源码树里的 `Documentation/` 目录

> the kernel documentation is always available for that kernel version within the kernel source tree itself, within the directory named Documentation/ .

⇒ **（推演）** 读文档要读**你手上那一版**，不是 `latest`。内核 API 半年一变，
按 `latest` 查到的函数抄进 5.10 内核里可能根本不存在。

原文还给了一个读文档的观察：现代内核文档相当完整，甚至会提示某个 API 已废弃
（例：`simple_strtoull()` 被标为 obsolete，建议改用 `kstrtoull()`）。

### §Generating the kernel documentation from source

可以**直接从源码树生成整份内核文档**，格式包括 PDF、HTML、LaTeX、**EPUB**、XML，风格类似 Javadoc / Doxygen；
内核内部用的文档系统叫 **Sphinx**。`make help` 里能看到 `htmldocs`、`pdfdocs` 等目标，例如：

```bash
make pdfdocs
```

产物 PDF 与部分 meta-docs 落在 `Documentation/output/latex`。第一次执行通常会被要求补装若干包与工具。

## §Static analysis tools for the Linux kernel 静态分析：内核专用 / 通用 / 商业

原文把「会误报」这件事放在工具介绍的**正前面**，不是附录：

> They can be tremendously useful to you as the developer, though you must learn how to "tame" them – in the sense that they can result in false positives.

工具分三类：

| 类别 | 工具 | 说明 |
|---|---|---|
| 内核专用 | Sparse | 内核自带的语义检查器 |
| 内核专用 | Coccinelle | 语义化补丁，需装 ocaml 包 |
| 内核专用 | Smatch | 基于 Sparse 的告警分析 |
| 通用 C/C++ | Flawfinder | 偏安全问题 |
| 通用 C/C++ | Cppcheck | 通用 C/C++ 检查 |
| 商业 | SonarQube / Coverity Scan / Klocwork | SonarQube 有免费社区版 |

**Sparse 怎么用**——关键是把它挂进内核的编译流程，这样**每次编译都跑一遍**：

```bash
sudo apt install sparse
cd <kernel-src-tree>
make C=1 CHECK="/usr/bin/sparse"
```

作者对「怎么驯误报」**没有给办法**，只给态度：

Static analysis tools can save the day. Time spent learning to use them effectively is time well spent!

⇒ **（推演）** 这是**诚实的留白**而非遗漏：误报规则是逐工具、逐规则的，全书各处零散给。
**「学会驯它们」本身就是这项技能的隐藏成本。**

## §Linux Trace Toolkit next generation LTTng + Trace Compass

> LTTng allows you to trace both userspace (applications) and/or the kernel code paths in minute detail.

LTTng 是 Linux Foundation 项目，覆盖面是**用户态 + 内核态一起追**——这是它相对 `strace` 之类工具的关键差别。
安装文档在 `lttng.org/docs`；配套 GUI 是 Eclipse 的 **Trace Compass**（负责把 trace 变成看得懂的东西），
它需要 JRE（原文装的是 `openjdk-18-jre`）。

作者的读图示例很值得学：Trace Compass 截图上部的**粉色 = 硬件中断**；
下方 **IRQ vs Time** 标签页给出中断的**分布**，y 轴是耗时——观察结果是
**网卡中断处理器几乎不耗时，而 i8042 键鼠控制器的处理器耗时更多，甚至超过 200 微秒**。

⇒ **（推演）** 这就是**按中断源分桶**的价值：一个总的「中断耗时」指标会把这个 200μs 淹掉，
分桶才看得出「是谁在吃时间」。

## §The procmap utility 把地址空间画出来

> Visualizing the complete memory map of the kernel Virtual Address Space ( VAS ) as well as any given process's user VAS is what the procmap utility is designed to do.

作者本人是 procmap 与 SEALS 的主作者之一（原文有免责声明）。它做的事：

- 输出进程完整内存映射的**可视化**，格式是**竖向平铺、按虚拟地址降序**
- 同时显示内核态与用户态映射，并**计算稀疏区**
- 每段按相对大小缩放并配色，便于阅读
- 64 位系统上显示所谓**非规范（non-canonical）稀疏区或「空洞」**——x86_64 上典型值约 **16,384 PB**

其它选项：只看内核空间 / 只看用户空间、verbose 与 debug 模式、导出 CSV。
带内核组件，自动识别 x86_64、AArch32、AArch64。作者注明**仍在开发中、有若干 caveat**，
并且在第 7 章（内存管理）会真正用到它。

## §Simple Embedded ARM Linux System FOSS project SEALS：一键骨架系统

SEALS（Simple Embedded ARM Linux System）是一个跑在**模拟 ARM 机器**上的极简「骨架」Linux 系统。
它的工作方式：一个 Bash 脚本用菜单问用户要什么功能，然后据此**交叉编译 ARM 内核**、
创建并初始化一个简单根文件系统，再调用 QEMU（`qemu-system-arm`，默认模拟 **Versatile Express CA-9** 板）把它跑起来。

产出物是**目标内核 + 根文件系统 + 根文件系统镜像文件**，并且**把启动流程一并配好**；
还带一个简单的 GUI（或控制台）前端。项目地址 `github.com/kaiwan/seals`，带 wiki。

⇒ **（推演）** 这是本章唯一一个「把整条链打包成一条命令」的工具：
内核、根文件系统、镜像、启动设置一次到位——比逐条手敲 `make` 强得多。

## §Modern tracing and performance analysis with eBPF eBPF 与它的四个前置条件

> eBPF is essentially virtual machine technology, allowing one to write (small) programs and run them in a safe isolated environment within the kernel!

背景：BPF 原本是内核里追网络包的基础设施；**eBPF 是它的扩展版**，从 **Linux 4.0** 起才有，
能追的不止网络栈。它的本质是**跑在内核里的、受限的沙箱虚拟机**，因此可用于生产环境的追踪与性能分析。

直接用 eBPF 内核特性很难，所以有前端：**BCC** 与 **bpftrace**。

**四个前置条件**（原文列出）：

- `Linux kernel 4.0 or later`
- 内核支持 BPF（需按 BCC 文档配置内核）
- 装好 BCC 或 bpftrace 前端
- 目标系统上有 root 权限

而这一节最实用的是**装不上 BCC 的原因**：

> The reason: the installation of the BCC toolset includes (and depends upon) the installation of the linux-headers-$(uname -r) package; this linux-headers package exists only for distro kernels (and not for our custom 5.10 kernel that we shall often be running on the guest).

⇒ **BCC 依赖 `linux-headers-$(uname -r)`，而这个包只对发行版内核存在**。
在自编译内核（本章正打算用的 5.10）上 `apt install bpfcc-tools` 就是装不上。

⇒ **（推演）** 这是「**工具链假设**」的教科书例子：工具假定你用的是发行版内核，
你一旦自编译，这个假定就断了。BCC 在**宿主发行版内核**上是可以装的——
读者要做 eBPF 观测，应该在宿主上装，而不是在 guest 的自编译内核上装。

## §The LDV - Linux Driver Verification - project LDV：规则 + 真实违规案例

LDV 是俄罗斯 Linux Verification Center 的开源项目（2005 年创立），做复杂软件项目的自动化测试：
完整测试套件、框架、以及对**内核核心与设备驱动**的静态与动态分析。
它的独门功夫是**把重心放在内核模块的测试与验证上**，而同类项目往往略过这一块。

最有价值的设计在这里：

> By glancing through these rules, we'll be able to not only see the rule but also instances of actual cases where these rules were violated by driver/kernel code within the mainline kernel, thus introducing bugs.

⇒ **「规则 + 真实违规案例」**：一条抽象规则（「引用计数必须平衡」）谁都能写；
**「mainline 里哪些驱动违反了它」**才是教材。LDV 的 Problems 页列出**400 多个已发现（且大多已修）的问题**。

作者说后面几章会提到内存泄漏、Use After Free (UAF)、锁违规等实例。

## ⚠️ 纠错补充

1. **AMD 虚拟化 flag 名**：原文 L148 同一句的括号里同时给出了两个 flag 的判读方式
   （"vmx is the check for Intel processors, smv is the check for AMD processors"），
   其中 **AMD 一侧把 flag 名写错成了 `smv`**。
   `/proc/cpuinfo` 上 AMD 的 flag 实际叫 **`svm`**（Secure Virtual Machine），不存在 `smv`。
   正确拼写 `svm` 出现在紧接其后的代码块命令（原文 L151 `egrep --color "vmx|svm" /proc/cpuinfo`）
   与随后的说明句（原文 L153 "In the case of AMD CPUs, svm will show up"）里——
   即原文自身前后不一致，错的只是括号里 AMD 侧这一处拼写。
2. **"Intel VT-x or AMD-SV"**：原文该句把 AMD 一侧写成 "AMD-SV"，规范叫法是 **AMD-V / SVM**。
3. **epub 排版产物（命令照抄会跑不动）**：原文的 `sudo useradd –m c2kp –s /bin/bash` 与 `rpi $ uname –a`
   里的连字符是 **en dash（U+2013）** 而非 ASCII 连字符；`sudo apt upgrade   // if required` 里的 `//`
   也不是合法的 shell 注释符（应为 `#`）。本篇在代码围栏里**照抄原文**，实际执行时请自行改成 `-m` / `-s` / `-a` 与 `#`。

## 章末选型判据

这一章的 80% 是「2022 年怎么装 Ubuntu 22.04」，会过期；剩下 20% 是工具地图，不会。
按场景取舍：

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 要在这台机器上动内核 | 虚拟机 + 足够磁盘 + 宿主算力 | 在生产机或唯一数据所在的机器上直接改 |
| 给 guest 分磁盘 | 50GB 上限、动态/按需分配 | 接受默认 8GB（源码树 + 编译产物 + 镜像很容易撑爆） |
| 查用户态系统调用 | `man 2 <syscall>` | `man 9 <kernel API>`（第 9 区今天没有内核 API 的 man page） |
| 只想要常见用法 | tldr（CLI 或 web client） | 把 tldr 当权威规格 |
| 查内核 API | 手上那一版的 `Documentation/`（`make htmldocs` / `pdfdocs` 自建） | `kernel.org/doc/html/latest/`（版本可能对不上你的内核） |
| 提交内核代码前 | Sparse 挂进编译：`make C=1 CHECK="/usr/bin/sparse"` | 只靠人肉 review；但也别指望零误报——误报要自己「驯」 |
| 追用户态 + 内核态全链路 | LTTng + Trace Compass | 只用 strace（看不到内核栈） |
| 生产环境动态观测 | eBPF（BCC / bpftrace），四个前置条件齐了再用 | 在自编译内核上指望 `apt install bpfcc-tools` 成功 |
| 看地址空间布局（含 64 位空洞） | procmap | 靠 `/proc/<pid>/maps` 手工推算非规范空洞 |
| 学「规则怎么被违反」 | LDV 的 Rules 页 + Problems 页（400+ 真实案例） | 只看抽象规则，不看真实违规 |
| 拿一套能跑的 ARM 目标环境 | SEALS 一条脚本出内核 + 根文件系统 + 镜像 + 启动 | 手工逐条搭交叉编译链 |

**动手顺序建议**（按投入产出排）：先把 §Technical requirements 的 `ctags` / `cscope` 索引做掉（一次投入永久收益），
再按 §A few remaining tips 把 `ssh` 远程工作流搭好（省掉剪贴板与分辨率的折磨），
最后把静态分析 / LTTng / eBPF 三件套按需装——**这三样才是这一章真正留给你的东西**。
