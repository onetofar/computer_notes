# io-buffered-vs-sync: 同样的字节, 写进去要花多久？

> 来源讲义：`Chapter 5. Applications（讲义）.md` §5.2.1–5.2.3

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-buffered-write.c` | 2000 次 1KB `write()`，**不等**落盘 | page cache 全吸收，吞吐约 300 MB/s；数据此刻还在内存里 |
| `02-sync-write-fsync.c` | 同样大小的写，但**每次都 `fsync()`** 等落盘 | 吞吐掉到约 0.2 MB/s，比 01 慢三个数量级（真实磁盘上） |
| `03-io-size-1k-vs-128k.c` | 同样 8MB：1KB×8192 次 vs 128KB×64 次 | 系统调用少 128 倍，大块写明显更快 |

三个文件都先打印目标落在**哪种文件系统**上 —— tmpfs 上 `fsync()` 几乎不要钱，缓冲写与同步写看不出差别，这个前提不说清楚，对比就是假的。

## 教材原话

> “Initialization tax” is paid for small and large I/O alike. For efficiency, the more data transferred by each I/O, the better.

> It’s usually much more efficient to transfer 128 Kbytes as a single I/O than as 128 × 1 Kbyte I/O

> A ring buffer (or circular buffer ) is a type of fixed buffer that can be used for continuous transfer between components, which act upon the buffer asynchronously.

## 跑法

```bash
gcc -std=c11 -O2 -Wall -Wextra -o /tmp/i1 01-buffered-write.c && /tmp/i1

# 落在真实磁盘上观察落盘代价（默认是当前目录）
SP_IO_DIR=/home/xier /tmp/i1
```

三个文件都支持 `SP_IO_DIR` 环境变量指定目标目录；不设时用当前工作目录。

## 结论

- **缓冲不是免费的午餐，是延迟**：`write()` 返回只说明进了 page cache。真正要求「写完即持久」时，每次 `fsync()` 的等待才是真实成本。
- **先问落在哪种文件系统上**：同一份代码在 tmpfs 和真磁盘上是两个系统。`03` 的 `fs_type` 输出就是提醒这件事。
- **I/O 大小是杠杆**：同样的字节数，1KB×8192 与 128KB×64 的差别不在带宽，在**每次 I/O 的固定成本**。选一个贴近应用真实请求的尺寸。
