# 等条件的三种姿势：condition_variable / condition_variable_any / 带超时

> 来源讲义：`C++ Concurrency in Action/Chapter 4. Synchronizing concurrent operations（讲义）.md` §4.1, §4.3
> `C++ Concurrency in Action/附录 D. C++ Thread Library reference（讲义）.md` §D.2

同一个问题：**「等一个条件成立」, 怎么等才既不漏唤醒、又不会无限等下去？**

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-cv-predicate.cpp` | 谓词式 `wait(lk, pred)`: 把循环收进标准库, 假醒也不出错; 生产者 `notify_one` | `cv: 收到 5 个, 和=15` |
| `02-cv-any-any-lock.cpp` | `condition_variable_any` 配 `recursive_mutex`（`condition_variable` 只吃 `unique_lock<mutex>`） | `wait_for 的谓词结果=true` / `ready=true (用 recursive_mutex 也能等条件)` |
| `03-cv-timed-wait.cpp` | 循环里用 `wait_for(duration)` 每次假醒都重新计时 ⇒ 总时长被拉长; 绝对 `time_point` 才有界 | `wait_for 循环共等了 234 ms, wait_until 绝对时间点共等了 31 ms` |

## 讨论点

1. **谓词式 wait 防不了假醒, 但省掉了「必须写在 while 里」这条纪律**：它不是更安全,
   是**把纪律藏进库里**。写法上仍然要假设「条件可能被反复检查」「可能被叫醒却什么都不变」。
2. **`cv_any` 存在的唯一理由是「我的锁不是 `std::mutex`」**：代价是慢——它得替你拿放**用户**的锁。
   讲义附录 D 的对照表说得很干脆：**默认选 `condition_variable`**。
3. **`03` 是本章最实用的一条工程结论**：**循环里用 `wait_for(duration)` ⇒ 总等待时间无上界**;
   要有界, 就用**一次算好的绝对 `time_point`**（或者只 `wait_for` 一次）。
   演示里 200ms 的假醒风暴把 20ms 的等待拉成了 200ms+。

## 教材原话

> Fundamentally, std::condition_variable::wait is an optimization over a busy-wait .

> Your code must be prepared to work with such a minimal implementation of wait() , as well as an implementation that only wakes up if notify_one() or notify_all() is called.

> it’s a bad idea to hold a lock on a mutex for longer than necessary.

> If you use wait_for() in a loop, you might end up waiting almost the full length of time before a spurious wake-up, and the next time through the wait time starts again.

> This may repeat any number of times, making the total wait time unbounded.

> This is the recommended way to wait for condition variables with a time limit if you’re not passing a predicate to wait .

> These are basic-level synchronization mechanisms that allow a thread to block until notified that some condition is true or a timeout period has elapsed.

> D.2 只有两小节：`std::condition_variable` 和 `std::condition_variable_any`。

## 跑法

```bash
cd <vault>/markdown/lab
bash tools/run_all.sh
# 单个文件:
bash tools/check_one.sh books/cpp-concurrency-in-action/condition-variable-family/03-cv-timed-wait.cpp
```
