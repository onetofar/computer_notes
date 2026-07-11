---
title: 升序链表的定时器服务器源码分析
date: 2026-06-28
tags:
  - net
  - cpp
  - pipe
cppfile: schedule_server.cpp
status: complete
link:
  - "[[markdown/csapp/第10章 系统级 IO | 系统级IO]]"
  - "[[5-3-b Linux 管道与信号处理核心笔记|管道与信号处理]]"
---

> 基于升序链表定时器服务器代码的学习笔记，记录关键概念和常见误区。

---

## 一、文件描述符 ≠ 数据容器

**误区**：`int pipefd[2]` 是 4 字节整数，所以管道只能存 4 字节。 **正确理解**：文件描述符只是**内核文件表的索引号**（门牌号），不是数据本身。

```
进程空间                        内核空间
──────────                     ──────────────
pipefd[0] = 3  ──────────→    文件表[3] → 管道缓冲区（约64KB）
pipefd[1] = 4  ──────────→    文件表[4] → 同一管道的另一端
```

`send(pipefd[1], data, len)` 的执行过程：

1. 内核根据 fd=4 查找文件表
2. 找到对应的管道对象（`struct pipe_inode_info`）
3. 将 `data` 复制到管道的 64KB 环形缓冲区

---

## 二、pipe() vs socketpair()

|特性|`pipe(fd)`|`socketpair(fd)`|
|---|---|---|
|方向|单向（`[0]`只读，`[1]`只写）|双向（两端均可读写）|
|操作函数|`read()`/`write()`|`recv()`/`send()`|
|适用场景|父子进程单向通信|双向通信（全双工）|

**本代码选择 `socketpair` 的原因**：统一使用 `send`/`recv` 风格，但实际只用了单向（`[1]`写、`[0]`读）。 **双向能力的典型用途**：

```
进程A: pipefd[0] 的 send() ──→ 进程B: pipefd[1] 的 recv()
进程A: pipefd[0] 的 recv() ←── 进程B: pipefd[1] 的 send()
```

---

## 三、异步信号安全 vs 互斥（两个独立层面）

### 层面一：异步信号安全（用户态）

**问题**：信号处理函数里能调用哪些函数？ **危险场景**：

```
主循环正在执行 malloc()
     ↓ SIGALRM 中断
sig_handler() 也调 malloc()
     ↓
两个 malloc 同时操作堆管理数据结构 → 崩溃
```

**规则**：

- ✅ 安全：`send()`, `recv()`, `write()`, `read()`, `kill()` — 直接系统调用，无用户态全局状态
- ❌ 不安全：`malloc()`, `printf()`, `std::cout`, `syslog()` — 有内部锁或全局缓冲区

**本代码**：`sig_handler` 里只调了 `send()`，安全。

### 层面二：管道并发写入（内核态）

**问题**：多个写入者同时写同一管道，数据会不会交错？ **答案**：内核保证 ≤ `PIPE_BUF`（Linux 上 4096 字节）的写入是**原子的**。

```
写入者A: send(fd, data_A, 100)  ──┐
写入者B: send(fd, data_B, 100)  ──┤→ 内核自旋锁排队
                                   ↓
                        管道缓冲区: [data_A][data_B]
                        保证不交错: 不会出现 A[..]B[..]A[..]
```

**关键**：这是内核层面的互斥（`pipe_lock` 自旋锁），用户代码无需加锁。

---

## 四、信号编号与字节传输

### 常见信号

|信号|编号|来源|用途|
|---|---|---|---|
|`SIGALRM`|14|`alarm()` 定时器到期|触发定时任务|
|`SIGTERM`|15|`kill` 命令|请求进程正常退出|
|`SIGINT`|2|Ctrl+C|中断进程|

### 为什么只写 1 字节？

`int sig = 14` 在内存中（x86 小端序）：

```
内存: [0x0E][0x00][0x00][0x00]
       ↑ 低地址（最低字节）
```

- `(char*)&sig` 取最低字节 `0x0E` = 14，信号编号都 < 64，1 字节够用
- 信号处理函数里做的事越少越安全

### 读端如何还原？

```cpp
char signals[1024];
recv(pipefd[0], signals, ...);   // signals[0] = 0x0E
switch (signals[i]) {
    case SIGALRM:  // char 0x0E 自动提升为 int 14，匹配
    case SIGTERM:  // char 0x0F 自动提升为 int 15，匹配
}
```

C++ 整型提升：`char` 和 `int` 比较时，`char` 自动转为 `int`。

---

## 五、统一事件源：信号 → 管道 → epoll

**核心问题**：信号处理函数里几乎什么都不能做，但需要在主循环中处理信号。 **解决方案**：

```
SIGALRM(14) 触发
     ↓
sig_handler(14):
  save errno → send(pipefd[1], 0x0E, 1) → restore errno   ← 只做这一件事
     ↓
管道缓冲区: [0x0E]
     ↓
epoll_wait 发现 pipefd[0] 可读    ← 信号变成了 I/O 事件
     ↓
主循环: recv(pipefd[0], ...) → signals[0] = 14
     ↓
switch(14): case SIGALRM → timeout = true → tick() 处理到期定时器
```

**优势**：

- 所有事件（I/O、信号）统一在 `epoll_wait` 中串行处理
- 无需加锁，避免信号处理函数与主循环的竞态条件
- 信号处理函数极小极安全

---

## 六、IPC 机制速查

|概念|Linux 实现|特点|
|---|---|---|
|管道|`pipe()` / `socketpair()`|内核缓冲区，单向或双向|
|信号|`kill()` / `sigaction()`|异步通知，仅传递信号编号|
|信号量|`semget()` / `semop()`|计数器，用于进程间同步|
|共享内存|`shmget()` / `mmap()`|最快，需自行同步|
|消息队列|`msgget()` / `msgsnd()`|结构化消息，有类型|
|套接字|`socket()` / `connect()`|可跨网络|

---

## 七、信号注册细节

```cpp
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;   // 指定处理函数
    sa.sa_flags |= SA_RESTART;     // 被中断的系统调用自动重启
    sigfillset(&sa.sa_mask);       // 执行期间屏蔽所有其他信号
    sigaction(sig, &sa, NULL);
}
```

|标志|作用|
|---|---|
|`SA_RESTART`|`epoll_wait` 被信号中断后自动重启，代码只需跳过 `EINTR`|
|`sigfillset`|信号处理函数执行期间屏蔽所有信号，防止嵌套|

---

## 关键总结

1. **文件描述符是门牌号**，管道是内核里 64KB 的房间
2. **异步信号安全**管的是「函数能不能在信号处理函数里调」，不是互斥
3. **管道并发写入**的互斥由内核自旋锁保证，用户无需操心
4. **信号编号是小整数**，1 字节足够传输
5. **统一事件源**把信号转为 I/O 事件，在 epoll 中串行安全处理