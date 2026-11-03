# kb-lab: 教材讲义的可执行对照实验

每个子目录对应一本教材。每个主题目录是一个**对照讨论**：
同一问题的多种写法并列，各自独立编译、可独立运行、输出可直接横向对比。

## 书目索引

| slug | 教材 | 主题数 | 实现文件数 |
| --- | --- | --- | --- |
| `cpp-concurrency-in-action` | C++ Concurrency in Action | 8 | 27 |
| `csapp` | Computer Systems: A Programmer Perspective | 7 | 24 |
| `effective-cpp` | Effective C++ (3rd, Chinese) | 7 | 27 |
| `effective-modern-cpp` | Effective Modern C++ | 7 | 26 |
| `game-arch` | Networked Game Architecture | 7 | 24 |
| `game-dev` | Networked Game Development in Practice | 7 | 22 |
| `linux-kernel` | Linux Kernel Programming | 3 | 10 |
| `linux-multithread-server` | Linux Multi-thread Server Programming | 7 | 23 |
| `os-concepts` | Operating Systems: Three Easy Pieces | 7 | 18 |
| `systems-performance` | Systems Performance | 6 | 21 |

## 各书主题目录

### `cpp-concurrency-in-action` — C++ Concurrency in Action

- `condition-variable-family/` — 3 个实现文件
- `lock-granularity/` — 3 个实现文件
- `memory-ordering/` — 3 个实现文件
- `multi-lock-acquisition/` — 4 个实现文件
- `mutex-family/` — 3 个实现文件
- `protecting-shared-state/` — 4 个实现文件
- `thread-pool-vs-async/` — 3 个实现文件
- `thread-vs-async-future/` — 4 个实现文件

### `csapp` — Computer Systems: A Programmer Perspective

- `branch-vs-cmov/` — 3 个实现文件
- `cache-locality/` — 4 个实现文件
- `float-precision/` — 3 个实现文件
- `integer-overflow/` — 4 个实现文件
- `pass-by-value-vs-pointer/` — 3 个实现文件
- `thread-vs-process/` — 4 个实现文件
- `virtual-memory-alloc/` — 3 个实现文件

### `effective-cpp` — Effective C++ (3rd, Chinese)

- `compiler-generated-functions/` — 4 个实现文件
- `exception-safety-levels/` — 4 个实现文件
- `member-init-order/` — 3 个实现文件
- `polymorphic-base-dtor/` — 4 个实现文件
- `resource-management-raii/` — 4 个实现文件
- `return-object-vs-reference/` — 4 个实现文件
- `self-assignment/` — 4 个实现文件

### `effective-modern-cpp` — Effective Modern C++

- `auto-deduction-boundary/` — 4 个实现文件
- `condvar-vs-future/` — 4 个实现文件
- `emplace-vs-push-back/` — 3 个实现文件
- `lambda-capture-lifetime/` — 4 个实现文件
- `move-vs-copy-semantics/` — 4 个实现文件
- `perfect-forwarding/` — 4 个实现文件
- `shared-ptr-control-block/` — 3 个实现文件

### `game-arch` — Networked Game Architecture

- `authority-topology/` — 4 个实现文件
- `latency-smoothing/` — 3 个实现文件
- `object-replication-strategies/` — 4 个实现文件
- `simulation-timestep/` — 2 个实现文件
- `socket-io-multiplexing/` — 4 个实现文件
- `transport-reliability-model/` — 3 个实现文件
- `wire-format-encoding/` — 4 个实现文件

### `game-dev` — Networked Game Development in Practice

- `blocking-vs-nonblocking/` — 2 个实现文件
- `io-multiplexing/` — 3 个实现文件
- `mysql-query-cost/` — 4 个实现文件
- `object-pool-vs-new/` — 3 个实现文件
- `oop-vs-ecs/` — 3 个实现文件
- `tcp-stream-vs-udp-datagram/` — 3 个实现文件
- `thread-process-actor/` — 4 个实现文件

### `linux-kernel` — Linux Kernel Programming

- `kernel-env-doctor/` — 3 个实现文件
- `static-analysis-report-vs-block/` — 4 个实现文件
- `syscall-vs-library-api/` — 3 个实现文件

### `linux-multithread-server` — Linux Multi-thread Server Programming

- `condition-variable-predicate/` — 3 个实现文件
- `copy-on-write-shared-ptr/` — 4 个实现文件
- `deadlock-lock-order/` — 4 个实现文件
- `epoll-echo-server/` — 2 个实现文件
- `lock-primitives/` — 4 个实现文件
- `object-lifetime-shared-ptr/` — 3 个实现文件
- `reactor-models/` — 3 个实现文件

### `os-concepts` — Operating Systems: Three Easy Pieces

- `cache-locality/` — 2 个实现文件
- `concurrency-anomalies/` — 4 个实现文件
- `condvar-while-vs-if/` — 2 个实现文件
- `lock-granularity/` — 2 个实现文件
- `raid-redundancy/` — 3 个实现文件
- `semaphore-binary-vs-counting/` — 3 个实现文件
- `thread-api-create/` — 2 个实现文件

### `systems-performance` — Systems Performance

- `cpu-affinity-and-cache/` — 3 个实现文件
- `io-buffered-vs-sync/` — 3 个实现文件
- `memory-and-network-latency/` — 4 个实现文件
- `syscall-cost-and-tracing/` — 4 个实现文件
- `timing-and-statistics/` — 4 个实现文件
- `use-method-and-sar/` — 3 个实现文件

## 规则

1. 一个主题目录 = 一个对照讨论，≥2 个实现文件
2. 单文件 ≤ 120 行，只验证一个机制
3. 文件头必须有 `@book @topic @source @expect` 元信息
4. `@ub: yes` 标注故意演示未定义行为，默认跳过
5. 引文只放在主题 README，不放代码里

## 跑法

```bash
python3 tools/lab_check.py     # 守卫: 结构 / 元信息 / 颗粒度
bash tools/run_all.sh           # 编译并运行全部
bash tools/run_all.sh --ub      # 连 UB 演示一起跑
bash tools/build_all.sh         # 只做语法检查
```
