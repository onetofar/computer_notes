# select / poll / epoll：注册一次还是每帧重报名单

> 来源讲义：`书籍/教材讲义/网络多人在线游戏开发实战教程/第2章 网络IO多路复用（讲义）.md` §2.1 / §2.2

三份文件跑**完全相同的负载**：64 个 socket、8 轮、每轮让 8 个 socket 就绪。
唯一变化的是"你把名单交给内核的方式"，所以三份输出可以直接横向比。

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-select-loop.cpp` | 每轮 `FD_ZERO` + `FD_SET` 重建全部集合，再 `select` | `select: sockets=64 rounds=8 select_calls=8 rebuild_fd_ops=512 returned_fd=64 FD_SETSIZE=1024` |
| `02-poll-array.cpp` | 数组只建一次不用重塞，但每次 `poll` 仍要把整个数组过一遍 | `poll: sockets=64 rounds=8 poll_calls=8 build_fd_ops=64 scanned_fd=512 returned_fd=64` |
| `03-epoll-loop.cpp` | 64 个 fd 只 `epoll_ctl` 注册一次，之后 `epoll_wait` 只返回就绪的那几个 | `epoll: sockets=64 rounds=8 epoll_ctl_add=64 epoll_wait_calls=8 returned_fd=64` |

三份输出放在一起看：**三者的"就绪 fd 总量"都是 64（活儿一样多），
但交给内核的名单成本差很多——select 每轮重塞 512 次、poll 每次扫描 512 个 fd、epoll 只注册 64 次。**

## 教材原话

讲义 §2.1 先把 `fd_set` 的四个宏点清楚：

> fd_set 是装 Socket 的容器，四个宏伺候它：`FD_ZERO`（清空）、`FD_SET`（加入）、`FD_CLR`（删除）、`FD_ISSET`（判断在不在）。

—— §2.1

讲义 §2.1 紧接着指出 select 最容易踩的坑：

> 正因为"返回时集合已被改掉"，**每次调用前都要重新 FD_ZERO + FD_SET 重建全部集合**。忘了重建，下一帧你"关心"的就只剩上一帧有事的那些 Socket 了。这也是 select 的效率硬伤之一：1000 个连接，每帧都要把 1000 个 fd 重塞一遍。

—— §2.1

讲义 §2.1 还写了 select 的硬上限：

> 另一个硬伤：**fd_set 大小有上限**（Linux 默认 `FD_SETSIZE` 1024）。书里实测 robots 线程开到 1100 就报错。

—— §2.1

讲义 §2.2 用一句话讲清 epoll 的换法：

> select 每帧重塞集合、逐个轮询，连接一多就累。epoll（仅 Linux）换了思路：**事件注册一次，底层一直帮你看，有事才叫你**。

—— §2.2

本章金句第 2 条把它归纳成三条：

> 2. **select 的三个原罪**——每帧重建集合、fd_set 上限 1024、返回后逐个 FD_ISSET 轮询；

—— §2 收尾

## 跑法

```bash
bash tools/run_all.sh

cd books/game-dev/io-multiplexing
g++ -std=c++20 -O2 -Wall -Wextra -pthread 01-select-loop.cpp -o /tmp/01 && /tmp/01
g++ -std=c++20 -O2 -Wall -Wextra -pthread 02-poll-array.cpp -o /tmp/02 && /tmp/02
g++ -std=c++20 -O2 -Wall -Wextra -pthread 03-epoll-loop.cpp -o /tmp/03 && /tmp/03
```

**实现说明（不是教材结论，是本实验的取舍）**：

- 用 `socketpair(AF_UNIX, SOCK_STREAM)` 造 64 对 socket，不占端口、不连网，
  跑在谁机器上结果都一样；每轮只往 8 个 peer 写 1 字节，读端读完即清空，
  所以"本轮就绪的 fd 数 = 8"是确定的，三份文件的数字可直接对比。
- `03-epoll-loop.cpp` 依赖 Linux 的 `epoll`，非 Linux 平台只打印提示就退出。
