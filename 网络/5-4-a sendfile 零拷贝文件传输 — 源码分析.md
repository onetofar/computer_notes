---
title: sendfile 零拷贝文件传输 — 源码分析
date: 2026-06-28
tags:
  - net
  - cpp
  - 操作系统
cppfile: "`sendfile.hpp`"
status: complete
---
# sendfile 零拷贝文件传输 — 源码分析

## 概述

一个极简的 TCP 文件传输服务器：监听端口 → 接受一个连接 → 用 `sendfile()` 把硬盘文件直接发给客户端 → 退出。

核心价值：**零拷贝**（zero-copy）—— 数据从磁盘到网卡全程在内核空间流动，不经过用户态内存。

---

## 代码总览

```cpp
open(file)  →  fstat() 获取大小
     ↓
bind + listen + accept (等一个客户端)
     ↓
sendfile(connfd, filefd, NULL, stat_buf.st_size)
     ↓
close(connfd) + close(sock)
```

**单连接、一发即退**，不是通用服务器，是 `sendfile` 的**最小可运行演示**。

---

## 分段详解

### 1. 打开文件

```cpp
int filefd = open(file_name, O_RDONLY);
assert(filefd > 0);
struct stat stat_buf;
fstat(filefd, &stat_buf);
```

`fstat` 获取文件大小（字节），后面告诉 `sendfile` 要发多少数据。

### 2. TCP 监听

```cpp
int sock = socket(PF_INET, SOCK_STREAM, 0);
bind(sock, ...);
listen(sock, 5);
int connfd = accept(sock, ...);
```

标准的 TCP 服务端模板，`PF_INET` = IPv4，`SOCK_STREAM` = TCP。

### 3. 零拷贝发送 — 核心

```cpp
sendfile(connfd, filefd, NULL, stat_buf.st_size);
```

| 参数 | 含义 |
|------|------|
| `connfd` | **输出**：TCP socket，数据从这里发出去 |
| `filefd` | **输入**：已打开的文件描述符，数据从这里读 |
| `offset` | `NULL` 表示从文件当前位置开始 |
| `count` | 要发送的字节数（即文件总大小） |

**注意**：
- `sendfile` 返回实际发送的字节数，可能小于 `count`（TCP 发送缓冲区满），**生产代码需要循环**直到发完或出错
- 本例简化了，一次性发送

---

## 传统方式 vs 零拷贝

### 传统 read + write（数据经用户态）

```
磁盘 ──DMA──→ 内核读缓冲区 ──CPU copy──→ 用户态缓冲区 ──CPU copy──→ 内核写缓冲区 ──DMA──→ 网卡
                ①                         ②                         ③
```

**4 次上下文切换**（2 次 read + 2 次 write 系统调用）
**2 次 DMA 拷贝**（磁盘→内核、内核→网卡）
**2 次 CPU 拷贝**（内核→用户态、用户态→内核）

### sendfile 零拷贝

```
磁盘 ──DMA──→ 内核读缓冲区 ──DMA copy──→ 网卡
                ①
```

**1 次上下文切换**（sendfile 系统调用进入内核，返回时一次）
**2 次 DMA 拷贝**（磁盘→内核、内核→网卡）
**0 次 CPU 拷贝** — 数据从未离开内核空间

### 对比图

```mermaid
flowchart LR
    subgraph 传统[传统 read + write]
        A1[磁盘] -->|DMA| B1[内核缓冲区]
        B1 -->|CPU copy| C1[用户缓冲区]
        C1 -->|CPU copy| D1[内核缓冲区]
        D1 -->|DMA| E1[网卡]
    end

    subgraph 零拷贝[sendfile 零拷贝]
        A2[磁盘] -->|DMA| B2[内核缓冲区]
        B2 -->|DMA| C2[网卡]
    end

    style 传统 fill:#ffe0e0
    style 零拷贝 fill:#e0ffe0
```

---

## sendfile 原型

```cpp
#include <sys/sendfile.h>

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```

| 返回 | 说明 |
|------|------|
| `> 0` | 实际发送的字节数 |
| `== 0` | 文件已读完或连接已关闭 |
| `== -1` | 出错（`errno` 见下文） |

**常见 errno**：

| errno | 原因 |
|-------|------|
| `EAGAIN` / `EWOULDBLOCK` | 非阻塞 socket 发送缓冲区满，需要重试 |
| `EINVAL` | `out_fd` 不是 socket，或 `in_fd` 不支持 mmap |
| `ENOSYS` | 内核不支持 sendfile（古老 Linux） |
| `EIO` | 读文件出错 |

---

## 限制与注意事项

### 1. 必须循环发送

```cpp
/* 正确的生产代码：不是一次性发完 */
off_t offset = 0;
ssize_t remaining = stat_buf.st_size;
while (remaining > 0) {
    ssize_t n = sendfile(connfd, filefd, &offset, remaining);
    if (n < 0) {
        if (errno == EAGAIN) continue;  /* 非阻塞重试 */
        perror("sendfile");
        break;
    }
    remaining -= n;
    /* offset 自动前移（传 &offset 时内核会更新） */
}
```

`sendfile` 带 `&offset` 和不带的区别：
- `NULL`：从文件当前偏移读，发送完成后偏移**不更新**
- `&offset`：从 `*offset` 开始读，发送完成后内核更新 `*offset`，**支持断点续传**

### 2. in_fd 的限制

- **必须是支持 mmap 的文件**（普通文件、某些设备文件），**不能是 socket 或 pipe**
- 为什么？sendfile 内部用 `mmap` + 检查页缓存来实现零拷贝
- `out_fd` **必须是 socket**（Linux 要求，FreeBSD 允许任意 fd）

### 3. 大文件问题

sendfile 每次调用有数量限制（`0x7ffff000` 约 2GB，取决于实现），超大文件需要分多次。

### 4. 仅适用于"发文件"场景

sendfile 只能**从文件到 socket**。反向不行。如果需要从 socket 读数据写到文件，需要用 `splice()`（另一个零拷贝系统调用）。

---

## 与其他零拷贝 API 对比

| 系统调用 | 方向 | 特点 |
|---------|------|------|
| `sendfile` | 文件 → socket | 最常用，HTTP 静态文件服务器核心 |
| `splice` | fd → fd | 双向通用，但一端必须是 pipe |
| `tee` | pipe → pipe | 数据克隆，不消耗原数据 |
| `copy_file_range` | 文件 → 文件 | 内核内文件拷贝 |

---

## 在真实项目中的应用

Nginx、Lighttpd、Tomcat（通过 Java NIO `FileChannel.transferTo()`）等高性能 Web 服务器都在用 `sendfile` 发送静态文件。

**典型路径**：
```
HTTP 请求 → 解析 URL → 找到文件 → open() → sendfile() → close()
                                ↑
                         零拷贝，不用读进用户态
```

Nginx 配置中的 `sendfile on;` 就是开启这个功能。

---

## 测试方式

### 构建

```bash
cmake --build /workspace/build --target sendfile
```

### 运行

终端 1 — 启动服务器（发送本地文件）：

```bash
/workspace/build/sendfile 0.0.0.0 9999 /etc/passwd
```

终端 2 — 用 curl 接收：

```bash
curl -v http://127.0.0.1:9999
# 或者用 nc
nc 127.0.0.1 9999 > received_file
```

### strace 验证零拷贝

```bash
strace -e sendfile /workspace/build/sendfile 0.0.0.0 9999 /etc/passwd
```

输出类似：
```
sendfile(4, 3, NULL, 1742)              = 1742
```

只有一次系统调用就把文件发完了，对比 `read` + `write` 需要 `2 × (文件大小/缓冲区)` 次。

---

---

## 性能基准测试

使用 `sendfile_bench`（`/workspace/src/study/net/sendfile/sendfile_bench.cpp`）对 512 MB 随机数据文件通过 loopback socket 分别测试零拷贝与普通 `read+write` 的性能。测试文件使用 `/dev/urandom` 填充，避免零页优化，结果更有参考价值。

### 结果一览

| 模式 | 缓冲区 | 耗时(ms) | 吞吐量(MB/s) | 加速比 |
|------|--------|---------|-------------|-------|
| readwrite | 4 KB | 559.05 | 915.83 | 1.00 × — **基线** |
| readwrite | 16 KB | 218.56 | 2342.60 | 2.56 × |
| readwrite | 64 KB | 157.17 | 3257.68 | 3.56 × |
| readwrite | 256 KB | 130.28 | 3929.88 | 4.29 × |
| readwrite | 1024 KB | 93.94 | 5450.46 | 5.95 × |
| **sendfile** | — | **57.52** | **8901.25** | **9.72 ×** |

### 解读

```
吞吐量（MB/s）
9000 ┤                             ┌────  sendfile  8901 ★
     │                             │
8000 ┤                             │
     │                             │
7000 ┤                             │
     │                             │
6000 ┤                     ┌───────┤
     │                     │       └────  readwrite(1MB)  5450
5000 ┤             ┌───────┤
     │             │       └────  readwrite(256KB) 3930
4000 ┤     ┌───────┤
     │     │       └────  readwrite(64KB)  3258
3000 ┤     │
     │     │
2000 ┤ ┌───┘
     │ │   └────  readwrite(16KB)  2343
1000 ┤ │
     │ └────  readwrite(4KB)   916
     └────────────────────────────────────
       4K  16K  64K 256K  1M   sendfile
```

**关键结论：**

1. **随机数据下 sendfile 优势远超零填充数据** — 实测加速比 **9.72 ×**，因为随机数据破坏了 CPU 缓存，每次拷贝都是真实的内存操作
2. **缓冲区越小，sendfile 优势越大** — 4KB 时加速 **9.72 ×**，省掉了海量的 syscall 和上下文切换
3. **大缓冲区仍追不上 sendfile** — 即使 1MB 缓冲区，read+write 的吞吐量 5450 MB/s 仍远低于 sendfile 的 8901 MB/s，CPU 拷贝开销在大数据量下依然显著
4. **真实服务器场景** — 面对大并发、慢网卡、CPU 争抢时，sendfile 节省的每比特 CPU 拷贝会直接转化为更高的并发处理能力。Nginx / Lighttpd 等生产服务器默认开启 `sendfile`

### 复现

```bash
# 方式一：自动生成随机数据文件（512MB）
/workspace/build/sendfile_bench

# 方式二：手动指定文件
dd if=/dev/urandom of=/tmp/testfile_1g bs=1M count=1024
/workspace/build/sendfile_bench /tmp/testfile_1g
```

> **为什么用随机数据？** 全零文件在 Linux 内核中有零页（zero page）优化，read+write 的 CPU 拷贝几乎不消耗实际内存带宽，导致 sendfile 优势被低估。随机数据下 CPU 拷贝才是真正的瓶颈，sendfile 的零拷贝优势才能充分体现。

---

## 总结

```
┌─────────────────────────────────────────────────────────┐
│                    sendfile 一句话                        │
│  "数据从磁盘到网卡，全程在内核空间，用户态只看结果不看过程" │
└─────────────────────────────────────────────────────────┘
```

- **性能优势**：减少 2 次 CPU 拷贝 + 减少 3 次上下文切换
- **代码优势**：一行替代 read+write 循环
- **适用场景**：文件服务器、HTTP 静态资源、日志下载、图片传输
- **不适合**：动态生成的数据（需要先写到文件）、从 socket 读数据
