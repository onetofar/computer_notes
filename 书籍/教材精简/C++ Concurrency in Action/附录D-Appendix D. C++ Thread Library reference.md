---
title: "附录D. C++ Thread Library reference（精读）"
category: 并发编程
tags:
  - chrono
  - 条件变量
  - 原子与内存序
  - future-promise
  - mutex与锁
  - 线程生命周期
difficulty: 高阶
source: "《C++ Concurrency in Action 第二版》（Anthony Williams）附录D C++ Thread Library reference"
source_section: "精读 §D.1–§D.7 全部七节；§D.1.1–§D.1.5 / §D.2.1–§D.2.2 / §D.3.1–§D.3.10 / §D.4.1–§D.4.5 / §D.5.1–§D.5.14 / §D.6.1–§D.6.11 / §D.7.1–§D.7.2 为各节内部子节，按主题重组讲解；数百条成员函数签名不逐条翻译，只留承重约束"
skills:
  - obsidian-markdown
---

# 附录D. C++ Thread Library reference

> **小节地图**：本篇是**参考手册**，不是教程——它按头文件分成七节（§D.1 `<chrono>`、§D.2 `<condition_variable>`、§D.3 `<atomic>`、§D.4 `<future>`、§D.5 `<mutex>`、§D.6 `<ratio>`、§D.7 `<thread>`），每个头文件下面又铺开几十条成员函数。
> 本篇的读法是**按主题重组**：每节先说「这一族 API 解决哪类并发问题」，再挑**承重的那一两条约束**逐字引出，最后说「什么时候用、什么时候别用」。
> 底层依赖：§D.1/§D.2 的等待语义接第4章（[[Chapter 4. Synchronizing concurrent operations|条件变量与 future]]），§D.3 接第5章（[[Chapter 5. The C++ memory model and operations on atomic types|内存模型与 atomic]]），§D.5 接第3章（[[Chapter 3. Sharing data between threads|共享数据与互斥量]]）与第6章（[[Chapter 6. Designing lock-based concurrent data structures|锁式并发数据结构]]），§D.7 接第2章（[[Chapter 2. Managing threads|管理线程]]）与第9章（[[Chapter 9. Advanced thread management|高级线程管理]]）。
> 本篇**不写实战锚点**——那是讲义层的事。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §D.1 时间与等待 · system_clock 不保证单调 | 附录D-Appendix D. C++ Thread Library reference.md L814-816 | The std::chrono::system_clock class provides a means of obtaining the current wall-clock time from the system-wide real-time clock. The current time can be obtained by calling std::chrono::system_clock::now() . Instances of std::chrono::system_clock::time_point can be converted to and from time_t with the std::chrono:: system_clock::to_time_t() and std::chrono::system_clock::to_time_point() functions. The system clock isn’t steady , so a subsequent call to std::chrono::system_clock::now() may return an earlier time than a previous call (for example, if the operating system clock is manually adjusted or synchronized with an external clock). |
| §D.1 时间与等待 · steady_clock 单调不倒退 | 附录D-Appendix D. C++ Thread Library reference.md L923-924 | The std::chrono::steady_clock class provides access to the system-wide steady clock. The current time can be obtained by calling std::chrono::steady_clock::now() . There is no fixed relationship between values returned by std::chrono::steady_clock::now() and wall-clock time. A steady clock can’t go backwards, so if one call to std::chrono::steady_clock::now() happens-before another call to std::chrono::steady_clock::now() , the second call must return a time point equal to or later than the first. The clock advances at a uniform rate as far as possible. |
| §D.1 时间与等待 · high_resolution_clock 的 now() 有开销 | 附录D-Appendix D. C++ Thread Library reference.md L1007 | Although std::chrono::high_resolution_clock has the highest resolution of all the library-supplied clocks, std::chrono::high_resolution_clock::now() still takes a finite amount of time. You must take care to account for the overhead of calling std::chrono::high_resolution_clock::now() when timing short operations. |
| §D.2 等待与唤醒 · 伪唤醒与谓词版 wait | 附录D-Appendix D. C++ Thread Library reference.md L1174 | The spurious wakeups mean that a thread calling wait() may wake even though no thread has called notify_one() or notify_all() . It’s therefore recommended that the overload of wait() that takes a predicate is used in preference where possible. Otherwise, it’s recommended that wait() be called in a loop that tests the predicate associated with the condition variable. |
| §D.2 等待与唤醒 · notify_all_at_thread_exit 持锁到线程退出 | 附录D-Appendix D. C++ Thread Library reference.md L1367-1368 | The lock is held until the thread exits, so care must be taken to avoid deadlock. It’s recommended that the calling thread should exit as soon as possible and that no blocking operations be performed on this thread. |
| §D.3 原子与内存序 · seq_cst 构成全局总序且是默认 | 附录D-Appendix D. C++ Thread Library reference.md L1864-1865 | The operation forms part of the single global total order of sequentially consistent operations. In addition, if it’s a store, it behaves like an std::memory_order_release operation; if it’s a load, it behaves like an std::memory_order_acquire operation; and if it’s a read-modify-write operation, it behaves as both std::memory_order_acquire and std::memory_order_release . This is the default for all operations. |
| §D.3 原子与内存序 · atomic_flag 是唯一保证无锁的类型 | 附录D-Appendix D. C++ Thread Library reference.md L1907-1908 | The std::atomic_flag class provides a simple bare-bones atomic flag. It’s the only data type that’s guaranteed to be lock-free by the C++11 Standard (although many atomic types will be lock-free in most implementations). |
| §D.3 原子与内存序 · atomic 不可拷贝/赋值 | 附录D-Appendix D. C++ Thread Library reference.md L2110 | Instances of std::atomic are not CopyConstructible or CopyAssignable , because these operations can’t be performed as a single atomic operation. |
| §D.4 异步结果 · 一个 future 只对应一个异步结果 | 附录D-Appendix D. C++ Thread Library reference.md L3907 | The std::future class template provides a means of waiting for an asynchronous result from another thread, in conjunction with the std::promise and std:: packaged_task class templates and the std::async function template, which can be used to provide that asynchronous result. Only one std::future instance references any given asynchronous result at any time. |
| §D.4 异步结果 · shared_future 共享状态才安全 | 附录D-Appendix D. C++ Thread Library reference.md L4138 | Accesses to a given instance of std::shared_future aren’t synchronized. It’s therefore not safe for multiple threads to access the same std::shared_future instance without external synchronization. But accesses to the associated state are synchronized, so it is safe for multiple threads to each access separate instances of std:: shared_future that share the same associated state without external synchronization. |
| §D.4 异步结果 · async 的启动策略决定任务跑在哪 | 附录D-Appendix D. C++ Thread Library reference.md L4853-4854 | std::async is a simple way of running self-contained asynchronous tasks to make use of the available hardware concurrency. A call to std::async returns a std::future that will contain the result of the task. Depending on the launch policy, the task is either run asynchronously on its own thread or synchronously on whichever thread calls the wait() or get() member functions on that future. |
| §D.5 互斥与锁 · mutex 的 lock/try_lock/unlock 契约 | 附录D-Appendix D. C++ Thread Library reference.md L4940-4941 | The std::mutex class provides a basic mutual exclusion and synchronization facility for threads that can be used to protect shared data. Prior to accessing the data protected by the mutex, the mutex must be locked by calling lock() or try_lock() . Only one thread may hold the lock at a time, so if another thread also tries to lock the mutex, it will fail ( try_lock() ) or block ( lock() ) as appropriate. Once a thread is done accessing the shared data, it then must call unlock() to release the lock and allow other threads to acquire it. |
| §D.5 互斥与锁 · lock_guard 构造加锁析构解锁 | 附录D-Appendix D. C++ Thread Library reference.md L6014-6016 | The std::lock_guard class template provides a basic lock ownership wrapper. The type of mutex being locked is specified by template parameter Mutex and must meet the Lockable requirements. The specified mutex is locked in the constructor and unlocked in the destructor. This provides a simple means of locking a mutex for a block of code and ensuring that the mutex is unlocked when the block is left, whether that’s by running off the end, by the use of a control flow statement such as break or return , or by throwing an exception. |
| §D.5 互斥与锁 · scoped_lock 用同一算法避免死锁 | 附录D-Appendix D. C++ Thread Library reference.md L6122 | - Effects Constructs an std::scoped_lock instance that references the supplied mutexes. Uses a combination of calls to m.lock() , m.try_lock() , and m.unlock() on each of the mutexes, in order to avoid deadlock, using the same algorithm as the std::lock() free function. |
| §D.5 互斥与锁 · call_once 建立 happens-before | 附录D-Appendix D. C++ Thread Library reference.md L7227 | - Synchronization The completion of an effective std::call_once invocation on an std::once_flag object happens-before all subsequent std::call_once invocations on the same std::once_flag object. |
| §D.6 编译期有理数 · ratio 约分且分母恒正 | 附录D-Appendix D. C++ Thread Library reference.md L7313 | - Description num and den are the numerator and denominator of the fraction N/D reduced to lowest terms. den is always positive. If N and D are the same sign, num is positive; otherwise num is negative. |
| §D.7 线程生命周期 · join 返回前线程已完成 | 附录D-Appendix D. C++ Thread Library reference.md L7989 | - Synchronization The completion of the thread of execution associated with *this prior to the call happens-before the call to join() returns. |
| §D.7 线程生命周期 · hardware_concurrency 可能返回 0 | 附录D-Appendix D. C++ Thread Library reference.md L8040-8041 | - Returns The number of threads that can run concurrently on the current hardware. This may be the number of processors in the system, for example. Where this information is not available or well-defined, this function returns 0 . |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> 「什么时候用 condition_variable、什么时候用 future」原文附录**没有**给判据——它只逐条描述 API 语义；本篇结尾的「章末选型判据」表是本篇整理的。
> 原文把「同一次 `std::async` 调用是否真的并行」交给实现选择（策略省略时 as-if 二者之一），本篇称其为「隐式策略可能不并行」是**基于原文这句 as-if 的解读**，原文没有用这个说法。
> 「`std::atomic` 不可拷贝 ⇒ 不能放进容器、不能按值传参」这一串推论，原文只说了不可拷贝这一条原因，其余是本篇推演。
> 附录没有讲「`std::this_thread::yield()` 代替 sleep 的收益」这类性能话题，只有 API 语义。

## §D.1 时间与等待（一）：chrono 把「时间」变成有类型的量

**这一族解决哪类并发问题**：几乎所有超时机制都要回答两个问题——「等多久」和「什么时候算超时」。
裸整数回答不了第二个问题（毫秒还是微秒？和谁的时间轴比？），所以标准库把时间做成了**类型**。

### §D.1.1 duration 与 §D.1.2 time_point：一个量长度，一个量位置

`duration<Rep, Period>` 的两个模板参数各管一件事：`Rep` 是**存计数的数值类型**，`Period` 是**一个 tick 等于多少秒**（用 §D.6 的 `std::ratio` 表达，如 `std::milli` = `ratio<1,1000>`）。
`time_point<Clock, Duration>` 则把「时间点」定义为**某个特定时钟纪元以来的时长**：

> The std::chrono::time_point class template represents a point in time, as measured by a particular clock. It’s specified as a duration since the epoch of that particular clock.

`Clock` 模板参数标识时钟（每个不同的时钟必须有唯一的类型），`Duration` 决定测量精度并默认为该时钟的默认 duration 类型。

**关键约束在转换构造函数上**。从一个 duration 转成另一个 duration 时，只有「目标用浮点」或「源周期是目标周期的整数倍」才允许——否则就是悄悄截断：

> This avoids accidental truncation (and corresponding loss of precision) from storing a duration with small periods in a variable representing a duration with a longer period.

原文给的例子很直白：5 毫秒存进**整秒**是编译错误，存进**浮点秒**可以（`s2.count()==0.005`），存进**微秒**可以（`us.count()==5000`）。
需要显式跨尺度转换时用 `std::chrono::duration_cast`（§D.1.1 末尾），它的语义是「按尽量少丢精度的办法换算」。

`time_point` 的类定义（照抄原文）：

```cpp
template <class Clock,class Duration = typename Clock::duration>
class time_point
{
public:
    typedef Clock clock;
    typedef Duration duration;
    typedef typename duration::rep rep;
    typedef typename duration::period period;

    time_point();
    explicit time_point(const duration& d);

    template <class Duration2>
    time_point(const time_point<clock, Duration2>& t);

    duration time_since_epoch() const;

    time_point& operator+=(const duration& d);
    time_point& operator-=(const duration& d);

    static constexpr time_point min();
    static constexpr time_point max();
```

**陷阱**：`time_point` 只能和**同一个 `Clock`** 的 time_point 相减得到 duration；跨时钟比较没有任何意义——这也是为什么 §D.7 的 `thread::id` 要「每个时钟各自唯一」。

### §D.1.3–§D.1.5 三种时钟：选错就是并发 bug

**这是本节最承重的一条**，也是最容易被当成 API 细节跳过的。

- **`system_clock`（§D.1.3）= 墙上时间**，可以被操作系统手动调整或与外部时钟同步，**因此可能倒退**：

> The system clock isn’t steady , so a subsequent call to std::chrono::system_clock::now() may return an earlier time than a previous call (for example, if the operating system clock is manually adjusted or synchronized with an external clock).

- **`steady_clock`（§D.1.4）= 单调时钟**，保证不倒退，且与墙上时间没有固定关系：

> A steady clock can’t go backwards, so if one call to std::chrono::steady_clock::now() happens-before another call to std::chrono::steady_clock::now() , the second call must return a time point equal to or later than the first.

- **`high_resolution_clock`（§D.1.5）= 分辨率最高的那个**，但它可能只是 `system_clock` 或 `steady_clock` 的一个 typedef，也可能是独立的类型——**所以它到底是不是稳态的，标准不保证**。而且：

> Although std::chrono::high_resolution_clock has the highest resolution of all the library-supplied clocks, std::chrono::high_resolution_clock::now() still takes a finite amount of time. You must take care to account for the overhead of calling std::chrono::high_resolution_clock::now() when timing short operations.

**实践结论**（对应第1章「不要凭印象写并发代码」）：**测量时间间隔一律用 `steady_clock`**；只有需要「人类可读的绝对时刻」（写日志、算日期）才用 `system_clock`；`high_resolution_clock` 只在「要最细的刻度」时用，且必须把 `now()` 自身的开销算进去。
§D.1 的开头还点明：每个时钟都有 `is_steady` 静态数据成员，而 **`std::chrono::steady_clock` 是唯一保证 steady 的时钟**（L23）——这条约束后面 §D.2、§D.4 的超时语义全都依赖它。

## §D.2 等待与唤醒：condition_variable 的三族重载

**这一族解决哪类并发问题**：把「等某个条件成立」从**忙等**换成**阻塞**，并且在等待期间**释放锁**。

### §D.2.1 condition_variable：锁类型被写死了

`std::condition_variable` 的 wait 系列只接受 `std::unique_lock<std::mutex>`（这是它相对 §D.2.2 的最大区别）。

`wait(lock)` 的语义分三段：原子地解锁 → 阻塞到被唤醒 → 返回前重新加锁。

> Atomically unlocks the supplied lock object and block until the thread is woken by a call to notify_one() or notify_all() by another thread, or the thread is woken spuriously. The lock object is locked again before the call to wait() returns.

**坑一：伪唤醒（spurious wakeup）**。这是本节唯一必须记住的规则：

> The spurious wakeups mean that a thread calling wait() may wake even though no thread has called notify_one() or notify_all() . It’s therefore recommended that the overload of wait() that takes a predicate is used in preference where possible. Otherwise, it’s recommended that wait() be called in a loop that tests the predicate associated with the condition variable.

带谓词的重载在原文里被定义成 as-if 下面这段循环——**所以「谓词版 wait」不是语法糖，它就是那个循环**：

```cpp
while(!pred())
{
    wait(lock);
}
```

**坑二：超时版本不等于「到点就返回」**。原文对 `wait_for` 的 Note 说得很清楚：循环里用 `wait_for` 时要自己保证超时仍然有效，**很多情况下 `wait_until` 更合适**；而且「线程可能被阻塞得比指定时长更久」：

> The thread may be blocked for longer than the specified duration. Where possible, the elapsed time is determined by a steady clock.

这两句在 §D.4 的 `future::wait_for` 里**一字不差地重复出现**（L4074-4075、L4100）——不是巧合：**凡是「等一个时长」的接口，超时都只是下限，不是上限**。真正准时的只有「轮询检查条件」这一种写法。

**坑三：`notify_all_at_thread_exit` 会把锁一直持到线程退出**：

> The lock is held until the thread exits, so care must be taken to avoid deadlock. It’s recommended that the calling thread should exit as soon as possible and that no blocking operations be performed on this thread.

这个函数是给「线程结束时要唤醒所有等待者」用的，但它把锁的所有权转进了内部存储，于是**在退出前那段时间里锁是不释放的**——如果别的线程正等着拿这把锁，就死锁了。原文还补了一条：等待方不能因为被唤醒就假定线程已经退出，仍要用谓词判断。

类定义（照抄原文）：

```cpp
class condition_variable
{
public:
    condition_variable();
    ~condition_variable();

    condition_variable(condition_variable const& ) = delete;
    condition_variable& operator=(condition_variable const& ) = delete;

    void notify_one() noexcept;
    void notify_all() noexcept;

    void wait(std::unique_lock<std::mutex>& lock);

    template <typename Predicate>
    void wait(std::unique_lock<std::mutex>& lock,Predicate pred);

    template <typename Clock, typename Duration>
    cv_status wait_until(
        std::unique_lock<std::mutex>& lock,
        const std::chrono::time_point<Clock, Duration>& absolute_time);

    template <typename Clock, typename Duration, typename Predicate>
    bool wait_until(
        std::unique_lock<std::mutex>& lock,
        const std::chrono::time_point<Clock, Duration>& absolute_time,
        Predicate pred);

    template <typename Rep, typename Period>
    cv_status wait_for(
        std::unique_lock<std::mutex>& lock,
        const std::chrono::duration<Rep, Period>& relative_time);

    template <typename Rep, typename Period, typename Predicate>
    bool wait_for(
        std::unique_lock<std::mutex>& lock,
        const std::chrono::duration<Rep, Period>& relative_time,
        Predicate pred);
};

void notify_all_at_thread_exit(condition_variable&,unique_lock<mutex>);
```

### §D.2.2 condition_variable_any：锁类型放宽了，能力也收窄了

> std::condition_variable_any can be used with any type that meets the Lockable requirements.

换句话说：**任何满足 Lockable 的锁都能用**（比如 `std::recursive_mutex`、自定义锁），代价是它走的是模板路径、而且**没有 `notify_all_at_thread_exit`**（原文里这个自由函数只接受 `condition_variable&`）。
它同样不可拷贝、不可移动（L1377）。

## §D.3 原子与内存序：atomic 的能力边界

**这一族解决哪类并发问题**：单个数据对象的无竞争访问，以及**跨对象建立可见性顺序**。它不解决「多个变量一起改」的原子性——那是第6章无锁数据结构的事。

### §D.3.1–§D.3.3 三件配套：别名、是否无锁、初始化宏

- **§D.3.1 `atomic_xxx` 别名**：为兼容即将到来的 C 标准提供的一组 typedef，C++17 起必须是 `std::atomic<T>` 特化的别名。
- **§D.3.2 `ATOMIC_xxx_LOCK_FREE` 宏**：值只能是 0 / 1 / 2。原文定义得很死：

> The value of ATOMIC_xxx_LOCK_FREE is either 0, 1, or 2.

0 = 对应类型上的操作**永远不是**无锁的；1 = 某些实例无锁、某些不是；2 = 总是无锁。**这个宏是判断「我的原子操作会不会退化成加锁」的唯一手段**（第5章讲为什么这件事重要）。
- **§D.3.3 `ATOMIC_VAR_INIT`**：初始化宏。原文特别标注——

> This initialization is not atomic, and any access by another thread to the variable being initialized where the initialization doesn’t happen-before that access is a data race and thus undefined behavior.

即：**用宏初始化 ≠ 线程安全初始化**。要跨线程安全地发布，得靠 happens-before（第5章）。

### §D.3.4 memory_order：六个档位，一句默认

原文把内存序写成枚举：

```cpp
typedef enum memory_order
{
    memory_order_relaxed,memory_order_consume,
    memory_order_acquire,memory_order_release,
    memory_order_acq_rel,memory_order_seq_cst
} memory_order;
```

逐档语义（一句话版）：

| 档位 | 原文语义 |
|---|---|
| `relaxed` | 不提供任何额外的排序约束 |
| `consume` | 已被 C++17 标准判定「不应使用」 |
| `acquire` | 读操作；若读到的是 release 操作写的值，那次 store 与本操作 synchronizes-with |
| `release` | 写操作；与同地址上读到该值的 acquire 操作 synchronizes-with |
| `acq_rel` | 必须是读-改-写，同时具备 acquire 与 release 语义 |
| `seq_cst` | 构成所有顺序一致操作的**单一全局总序**，且是**所有操作的默认值** |

承重的是最后一条：

> The operation forms part of the single global total order of sequentially consistent operations. In addition, if it’s a store, it behaves like an std::memory_order_release operation; if it’s a load, it behaves like an std::memory_order_acquire operation; and if it’s a read-modify-write operation, it behaves as both std::memory_order_acquire and std::memory_order_release . This is the default for all operations.

**为什么是「承重」**：`seq_cst` 是默认，意味着**你不写内存序 = 承担全局总序的代价**；而 `consume` 在 C++17 被禁用（原文：「The C++17 Standard states that this memory ordering should not be used.」）——它曾经是「依赖式加载」的语法糖，实际实现几乎无法正确优化，所以标准直接劝退。

### §D.3.5–§D.3.7 栅栏与 atomic_flag

- **§D.3.5 `atomic_thread_fence`**：插入内存屏障。原文的规则写得很精确——release/acq_rel/seq_cst 的栅栏与「同地址上读到栅栏之后写入值的 acquire 操作」synchronizes-with；反过来也成立。
- **§D.3.6 `atomic_signal_fence`**：同样插栅栏，但**约束只作用于本线程与本线程的信号处理程序之间**（原文：This is equivalent to std::atomic_thread_fence(order) except that the constraints apply only between a thread and a signal handler on the same thread.）。
- **§D.3.7 `atomic_flag`**：只有一个 set / clear 状态的裸标志位。

> The std::atomic_flag class provides a simple bare-bones atomic flag. It’s the only data type that’s guaranteed to be lock-free by the C++11 Standard (although many atomic types will be lock-free in most implementations).

**这是自旋锁的地基**：要自己实现无锁自旋锁，`atomic_flag` 是唯一有「保证无锁」背书的选择。另有 `ATOMIC_FLAG_INIT` 把它初始化为 clear 状态（默认构造的状态是未指定的，别依赖）。

### §D.3.8–§D.3.10 `atomic<T>`：能包装什么

`std::atomic<BaseType>` 的 `BaseType` 必须满足四条硬性要求：

> - Have a trivial default constructor
> - Have a trivial copy-assignment operator
> - Have a trivial destructor
> - Be bitwise-equality comparable

**这四条实际等价于「必须是可以按位复制的简单类型」**——所以 `std::atomic<int>` 行，`std::atomic<some-simple-struct>` 行，`std::atomic<std::string>` **不行**（有非平凡构造/析构）。
另一个直接后果：

> Instances of std::atomic are not CopyConstructible or CopyAssignable , because these operations can’t be performed as a single atomic operation.

**推论**：`std::atomic` 不能放进 `std::vector`（容器要求可拷贝）、不能按值传参、不能放进需要拷贝的聚合结构。要在容器里放原子量，得用 `std::atomic<T*>`（§D.3.10 有指针特化）或用 `unique_ptr` 包。

**§D.3.9–§D.3.10 特化**：整数类型和指针类型有专门特化，整数特化额外提供 `fetch_add` / `fetch_sub` / `fetch_and` / `fetch_or` / `fetch_xor` 以及 `++` / `--` / `+=` 等**读-改-写**操作；指针特化额外提供原子指针算术。这才是第5章里那些「原子 RMW」能写出来的原因。

**§D.3 的一个暗坑**（原文 §D.3.8 的 Note）：默认构造的 `std::atomic` 对**非静态存储期**对象「不能依赖有可预期的值」；而 `std::atomic_init` 是**非原子存储**，并发访问它构成数据竞争（L2292）。

## §D.4 异步结果：future 家族把「结果」当成可传递对象

**这一族解决哪类并发问题**：把「某处算出来的一个值」安全地交给另一个线程，并且让**异常也能被传递**（这正是裸指针/裸引用做不到的）。

### §D.4.1 future：一次性的结果句柄

> The std::future class template provides a means of waiting for an asynchronous result from another thread, in conjunction with the std::promise and std:: packaged_task class templates and the std::async function template, which can be used to provide that asynchronous result. Only one std::future instance references any given asynchronous result at any time.

**「Only one」是承重约束**：`future` 独占结果，所以它可移动、不可拷贝（MoveConstructible / MoveAssignable，但 not CopyConstructible / CopyAssignable，L3909）。
`get()` 会取出结果或重抛存储的异常：

> If the associated state contains a deferred function from a call to std::async , invokes that function and returns the result; otherwise, waits until the asynchronous result associated with an instance of std::future is ready, and then returns the stored value or throws the stored exception.

`wait()` / `wait_for()` / `wait_until()` 则只是等结果就绪，返回 `future_status`（`ready` / `timeout` / `deferred`）。
注意 `future` 也没有 `notify_all_at_thread_exit` 那种「退出时发布」的机制——那是 §D.4.4 `promise` 的 `set_value_at_thread_exit`（§D.4.4）。

类定义（照抄原文）：

```cpp
template<typename ResultType>
class future
{
public:
    future() noexcept;
    future(future&&) noexcept;
    future& operator=(future&&) noexcept;
    ~future();

    future(future const&) = delete;
    future& operator=(future const&) = delete;

    shared_future<ResultType> share();

    bool valid() const noexcept;

    see description get();

    void wait();

    template<typename Rep,typename Period>
    future_status wait_for(
        std::chrono::duration<Rep,Period> const& relative_time);

    template<typename Clock,typename Duration>
    future_status wait_until(
        std::chrono::time_point<Clock,Duration> const& absolute_time);
};
```

### §D.4.2 shared_future：把独占改成共享，但共享的是「状态」不是「对象」

这是本节最容易记错的一条，原文写得很清楚：

> Accesses to a given instance of std::shared_future aren’t synchronized. It’s therefore not safe for multiple threads to access the same std::shared_future instance without external synchronization. But accesses to the associated state are synchronized, so it is safe for multiple threads to each access separate instances of std:: shared_future that share the same associated state without external synchronization.

**一句话**：`shared_future` **对象本身**不是线程安全的，**它指向的共享状态**才是。
⇒ 想让 N 个线程都拿到同一个结果，要 **每个线程各持一份 `shared_future` 副本**（拷贝），而不是让 N 个线程去访问同一个 `shared_future` 对象。
（`shared_future` 可拷贝可赋值，并且能从 `future` 移动构造出来，L4136。）

### §D.4.3–§D.4.4 packaged_task 与 promise：结果的生产端

- **§D.4.3 `packaged_task`**：把一个可调用对象包起来，**调用它的结果被存成异步结果**，供 `std::future` 取用。

> The std::packaged_task class template packages a function or other callable object so that when the function is invoked through the std::packaged_task instance, the result is stored as an asynchronous result for retrieval through an instance of std::future .

它是**可移动、不可拷贝**的（L4361）。另有 `make_ready_at_thread_exit()`：调用任务但**不立刻让结果就绪**，等到线程退出才发布。

- **§D.4.4 `promise`**：结果的手动写入端。原文的分工是：

> A std::future associated with the asynchronous result of a particular std::promise instance can be obtained by calling the get_future() member function. The asynchronous result is set either to a value of type ResultType with the set_value() member function or to an exception with the set_exception() member function.

三兄弟各管一段：`promise` 写、`future` 读一次、`packaged_task` 把「调用」和「写结果」绑在一起。
`promise` 也可移动不可拷贝（L4617），并且有 `set_value_at_thread_exit()` / `set_exception_at_thread_exit()`——**「值先存好，等线程退出再唤醒等待者」**，这正是「线程结束时发布最终状态」的机制。
反过来，`promise` 或 `packaged_task` 在结果未设置就被销毁时，等待方会拿到 `broken_promise` 错误（L4516、L4740）。

### §D.4.5 std::async：启动策略决定任务跑在哪

> std::async is a simple way of running self-contained asynchronous tasks to make use of the available hardware concurrency. A call to std::async returns a std::future that will contain the result of the task. Depending on the launch policy, the task is either run asynchronously on its own thread or synchronously on whichever thread calls the wait() or get() member functions on that future.

策略枚举与接口（照抄原文）：

```cpp
enum class launch
{
    async,deferred
};

template<typename Callable,typename ... Args>
future<result_of<Callable(Args...)>::type>
async(Callable&& func,Args&& ... args);

template<typename Callable,typename ... Args>
future<result_of<Callable(Args...)>::type>
async(launch policy,Callable&& func,Args&& ... args);
```

三个关键行为：

1. **`launch::deferred` = 任务根本没跑**，只是被存成「延迟调用」，**第一次 `wait()` 或 `get()` 时在调用线程上同步执行**：

> If policy is std::launch::deferred , fff and xyz... are stored in the returned std::future as a deferred function call. The first call to the wait() or get() member functions on a future that shares the same associated state will execute INVOKE (fff,xyz...) synchronously on the thread that called wait() or get() .

2. **策略省略时，实现会「按调用逐次选择」**——原文写的是 as-if `launch::async` 或 `launch::deferred` 之一，由实现为了「利用可用硬件并发又不至于过度订阅」来挑。
   ⇒ **推论**：`std::async(f)` 不保证真的并行。**要并行就显式写 `std::launch::async`。**
3. **最后一个 future 析构会阻塞**：

> The destructor of the last future object associated with the asynchronous state of the returned std::future blocks until the future is ready.

（原文还在 §D.4.1 的 `wait_for` 里说明：如果关联的是还没启动的 deferred 函数，`wait_for` **立即返回 `deferred` 状态、不阻塞**。）
另外 §D.4.5 的 Synchronization 段写明：函数调用的完成 happens-before 任何引用同一状态的 `wait()` / `get()` / `wait_for()` / `wait_until()` 成功返回。

## §D.5 互斥与锁：mutex 家族与四层 RAII 包装

**这一族解决哪类并发问题**：保护**一段代码**（而不是一个变量）的互斥访问，并保证异常路径下锁一定会被释放。

### §D.5.1–§D.5.4 互斥量家族：从最简到可重入、可超时

**§D.5.1 `std::mutex`** 的契约只有一句，但每个动词都是承重的：

> The std::mutex class provides a basic mutual exclusion and synchronization facility for threads that can be used to protect shared data. Prior to accessing the data protected by the mutex, the mutex must be locked by calling lock() or try_lock() . Only one thread may hold the lock at a time, so if another thread also tries to lock the mutex, it will fail ( try_lock() ) or block ( lock() ) as appropriate. Once a thread is done accessing the shared data, it then must call unlock() to release the lock and allow other threads to acquire it.

**注意 `try_lock()` 可以在没有任何人持锁时返回 false**（原文 §D.5.1 的 Note：The function may fail to acquire the lock (and return false ) even if no other thread holds a lock on *this.）——这是自旋/退避策略里最容易误判的一点。

其余三个变体是**在同一套契约上加能力**：

- **§D.5.2 `recursive_mutex`**：同一线程可重复加锁，计数递增：

> This mutex is recursive so a thread that holds a lock on a particular std::recursive_mutex instance may make further calls to lock() or try_lock() to increase the lock count. The mutex can’t be locked by another thread until the thread that acquired the locks has called unlock once for each successful call to lock() or try_lock() .

  ⇒ **每次成功的 `lock()` 必须配一次 `unlock()`**，否则这把锁永远放不掉。
- **§D.5.3 `timed_mutex`**：在 `mutex` 上加超时（`try_lock_for` / `try_lock_until`），并满足 TimedLockable。
- **§D.5.4 `recursive_timed_mutex`**：两者叠加。

类定义（照抄原文）：

```cpp
class mutex
{
public:
    mutex(mutex const&)=delete;
    mutex& operator=(mutex const&)=delete;

    constexpr mutex() noexcept;
    ~mutex();

    void lock();
    void unlock();
    bool try_lock();
};
```

### §D.5.5–§D.5.6 shared_mutex：读多写少的专用形态

`std::shared_mutex` 允许「**一个线程持独占锁，或多个线程持共享锁**」。原文的适用场景写得很明确：

> The std::shared_mutex class provides a mutual exclusion and synchronization facility for threads that can be used to protect shared data that is frequently read and rarely modified.

写者用 `lock()` / `try_lock()`（独占），读者用 `lock_shared()` / `try_lock_shared()`（共享）；**有线程想拿独占锁时，它必须等所有共享锁都放掉**。
§D.5.5 的 `unlock_shared()` 还写明：**释放最后一个共享锁时，会唤醒一个等独占锁的线程，或若干个等共享锁的线程**（L5701）。
（§D.5.6 `shared_timed_mutex` 是它的带超时版本。）

### §D.5.7–§D.5.10 四个锁包装：选哪个取决于「要不要手动控制锁」

这四个都是 RAII 包装，差别在**控制粒度**：

| 包装 | 构造时 | 何时用 |
|---|---|---|
| **§D.5.7 `lock_guard`** | 必加锁 | 全程都要锁，一句「进去就锁、出来就解」 |
| **§D.5.8 `scoped_lock`** | 必加锁（多把） | 同时锁多把，且想避免死锁 |
| **§D.5.9 `unique_lock`** | 可选（defer / try / adopt / 超时） | 需要**延迟加锁、尝试加锁、手动解锁**，或要和条件变量配合 |
| **§D.5.10 `shared_lock`** | 同 unique_lock，但取共享锁 | 配 `shared_mutex` 的读者 |

**`lock_guard`（§D.5.7）** 的承重点是「作用域结束必解锁」：

> The std::lock_guard class template provides a basic lock ownership wrapper. The type of mutex being locked is specified by template parameter Mutex and must meet the Lockable requirements. The specified mutex is locked in the constructor and unlocked in the destructor. This provides a simple means of locking a mutex for a block of code and ensuring that the mutex is unlocked when the block is left, whether that’s by running off the end, by the use of a control flow statement such as break or return , or by throwing an exception.

⇒ **正常结束、`break`、`return`、抛异常，四条路径都解锁**——这就是第1章讲过的 RAII 惯例在标准库里的落地。

**`scoped_lock`（§D.5.8）** 解决的是**多锁死锁**：

> Uses a combination of calls to m.lock() , m.try_lock() , and m.unlock() on each of the mutexes, in order to avoid deadlock, using the same algorithm as the std::lock() free function.

**注意它用的就是 §D.5.11 `std::lock` 的算法**——这是 C++17 之后多锁的正解（见下）。

**`unique_lock`（§D.5.9）** 是最通用的那个：

> The std::unique_lock class template provides a more general lock ownership wrapper than std::lock_guard .

它多出来的能力对应 §D.2 的条件变量——原文在同一段里写明：

> all instantiations of std::unique_lock are suitable for use with the Lockable parameter for the std::condition_variable_any wait functions.

四种构造策略：**`lock_guard` 没有、`unique_lock` 全有**——`defer_lock`（构造时不加锁，事后自己 `lock()`）、`try_to_lock`（试一下，失败则不持有）、`adopt_lock`（我已经锁了，你接管）、以及带 duration / time_point 的超时构造。
两个要小心的成员函数：
- **析构**：只在**自己持有锁时**才解锁（Destroys an std::unique_lock instance and unlocks the corresponding mutex if it’s owned by the destroyed instance.）——所以「defer 后忘了 lock」不会误解锁别人的锁。
- **`release()`**：解除关联但**不解锁**：

> Breaks the association of the mutex with *this without unlocking any locks held.

  ⇒ 调用 `release()` 之后，**锁的所有权转给了调用者**，得自己负责解锁。忘了就是永久死锁。

类定义（照抄原文）：

```cpp
template <class Mutex>
class unique_lock
{
public:
    typedef Mutex mutex_type;

    unique_lock() noexcept;
    explicit unique_lock(mutex_type& m);
    unique_lock(mutex_type& m, adopt_lock_t);
    unique_lock(mutex_type& m, defer_lock_t) noexcept;
    unique_lock(mutex_type& m, try_to_lock_t);

    template<typename Clock,typename Duration>
    unique_lock(
        mutex_type& m,
        std::chrono::time_point<Clock,Duration> const& absolute_time);

    template<typename Rep,typename Period>
    unique_lock(
        mutex_type& m,
        std::chrono::duration<Rep,Period> const& relative_time);

    ~unique_lock();

    unique_lock(unique_lock const& ) = delete;
    unique_lock& operator=(unique_lock const& ) = delete;

    unique_lock(unique_lock&& );
    unique_lock& operator=(unique_lock&& );

    void swap(unique_lock& other) noexcept;

    void lock();
    bool try_lock();
    template<typename Rep, typename Period>
    bool try_lock_for(
        std::chrono::duration<Rep,Period> const& relative_time);
    template<typename Clock, typename Duration>
    bool try_lock_until(
        std::chrono::time_point<Clock,Duration> const& absolute_time);
    void unlock();

    explicit operator bool() const noexcept;
    bool owns_lock() const noexcept;
    Mutex* mutex() const noexcept;
    Mutex* release() noexcept;
};
```

### §D.5.11–§D.5.14 多锁、一次执行

**§D.5.11 `std::lock`**：一次锁多把，且**不会因为加锁顺序不一致而死锁**：

> The std::lock function template provides a means of locking more than one mutex at the same time, without risk of deadlock resulting from inconsistent lock orders.

原文的 Note 还补了一条异常安全：若异常从 `std::lock` 里逃出去，**已经拿到的那些锁都会被解锁**。

**§D.5.12 `std::try_lock`**：全拿或全不拿：

> The std::try_lock function template allows you to try to lock a set of lockable objects in one go, so either they are all locked or none are locked.

返回值有约定：全成功返回 `-1`，否则返回**失败的第几个（0 基）**。异常逃出时同样会释放已获取的锁。

**§D.5.13–§D.5.14 `once_flag` + `call_once`：恰好一次执行**。

> Invocations of std::call_once on the same std::once_flag object are serialized. If there has been no prior effective std::call_once invocation on the same std::once_flag object, the argument func (or a copy thereof) is called as-if by INVOKE(func,args) , and the invocation of std::call_once is effective if and only if the invocation of func returns without throwing an exception. If an exception is thrown, the exception is propagated to the caller.

**「effective」的定义是关键**：只有 `func` **没抛异常**才算「已执行」。抛了异常 → 异常传播给调用者，且这次调用**不算数**（下次还会再调一遍）。
它建立的顺序保证是：

> The completion of an effective std::call_once invocation on an std::once_flag object happens-before all subsequent std::call_once invocations on the same std::once_flag object.

⇒ **`call_once` 不只是「避免重复执行」，它还是一条 happens-before 边**——这正是「多线程初始化单例」的标准做法（第9章的线程安全单例）。

## §D.6 编译期有理数：ratio 是 chrono 的分母

**这一族解决哪类并发问题**：它本身不解决并发问题，它是 §D.1 的**地基**——`duration` 的 `Period` 参数就是 `std::ratio` 的实例（`std::milli` = `ratio<1,1000>`）。

`ratio<N, D>` 把分数**约分到最简**，并且**分母恒为正**：

> num and den are the numerator and denominator of the fraction N/D reduced to lowest terms. den is always positive. If N and D are the same sign, num is positive; otherwise num is negative.

（另外 `D` 不得为零——原文 Requirements：D may not be zero.）
类定义（照抄原文）：

```cpp
template <intmax_t N, intmax_t D = 1>
class ratio
{
public:
    typedef ratio<num, den> type;
    static constexpr intmax_t num= see below;
    static constexpr intmax_t den= see below;
};
```

**§D.6.2–§D.6.11 是成套的编译期运算别名**：`ratio_add` / `ratio_subtract` / `ratio_multiply` / `ratio_divide` 做有理数四则运算，`ratio_equal` / `ratio_not_equal` / `ratio_less` / `ratio_greater` / `ratio_less_equal` / `ratio_greater_equal` 做比较。
**坑**：四则运算的中间结果**溢出时程序是 ill-formed**（不是运行时错误，是编译不过）——比如把 `ratio<1,3>` 加 `ratio<2,5>` 时分母要乘，分子分母都用 `intmax_t`，量级一大就溢出。

## §D.7 线程生命周期：thread 与 this_thread

**这一族解决哪类并发问题**：把「一个线程」变成**一个可移动的资源对象**，让「谁负责等它、谁负责收它」在类型上就说清楚。

### §D.7.1 std::thread：启动、等待、分离、终止

> The std::thread class is used to manage a thread of execution. It provides a means of starting a new thread of execution and waiting for the completion of a thread of execution. It also provides a means for identifying and provides other functions for managing threads of execution.

类定义（照抄原文）：

```cpp
class thread
{
public:
    // Types
    class id;
    typedef implementation-defined native_handle_type; // optional

    // Construction and Destruction
    thread() noexcept;

    ~thread();

    template<typename Callable,typename Args...>
    explicit thread(Callable&& func,Args&&... args);

    // Copying and Moving
    thread(thread const& other) = delete;
    thread(thread&& other) noexcept;

    thread& operator=(thread const& other) = delete;
    thread& operator=(thread&& other) noexcept;

    void swap(thread& other) noexcept;

    void join();
    void detach();
    bool joinable() const noexcept;

    id get_id() const noexcept;

    native_handle_type native_handle();

    static unsigned hardware_concurrency() noexcept;
};

void swap(thread& lhs,thread& rhs);
```

**承重的四条约束**：

1. **不可拷贝，只能移动**。原文 Notes：std::thread objects are not CopyConstructible , so there’s no copy constructor, only this move constructor.（移动赋值同理，L7923。）⇒ 线程对象只能被 move，**不能放进容器按值存储**。
2. **析构时若还 joinable，直接 `std::terminate()`**（L7901）。这是 §D.7 里最容易踩的坑：`std::thread t(f);` 忘了 `join()`/`detach()`，程序在**析构那一刻**就 abort 了——不是在 `main` 结束时，也不是「可能有问题」。
3. **`join()` 建立 happens-before**：

> The completion of the thread of execution associated with *this prior to the call happens-before the call to join() returns.

   ⇒ **线程里写的数据，`join()` 返回后主线程一定看得见**——这是第2章「线程结束读数据要 join」的规范依据。
4. **`detach()` 让线程与 `std::thread` 对象脱钩**——此后没有任何对象负责等它：

> The thread of execution associated with *this prior to the call is detached and no longer has an associated std::thread object.

**另外两个容易误用的**：

- **`hardware_concurrency()` 是「提示」，而且可能返回 0**：

> The number of threads that can run concurrently on the current hardware. This may be the number of processors in the system, for example. Where this information is not available or well-defined, this function returns 0 .

  ⇒ **拿它做除数或线程池初始容量会出事**（0），而且它给的是处理器数、**不是硬件线程数**（第1章 §1.1.1 说过，超线程会让硬件线程数多于核数）。
- **`thread::id`** 的值不可预测、每次运行可能不同；默认构造的 id 统一表示「不代表任何线程」，且所有默认构造的 id **相同**（L7672）。它构成一个**全序**，可以放进 `std::map` 当键。

### §D.7.2 std::this_thread：在当前线程上做点事

> The functions in the std::this_thread namespace operate on the calling thread.

- **`get_id()`**：拿当前线程的 id。
- **`yield()`**：告诉库「我现在不想跑」：

> Used to inform the library that the thread that invoked the function doesn’t need to run at the point of the call. Commonly used in tight loops to avoid consuming excessive CPU time.

  ⇒ 用途明确：**紧循环里让出 CPU**，不是「让出时间片给别人」这么含糊。
- **`sleep_for` / `sleep_until`**：挂起当前线程。同样有那条「可能睡得更久」的 Note（L8093-8094）——和 §D.2、§D.4 的超时接口是同一套语义。

## 章末选型判据

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 测量一段耗时 | `steady_clock` | `system_clock`（会被校时调倒退）、`high_resolution_clock`（不保证稳态、`now()` 有开销） |
| 要人类可读的绝对时刻 | `system_clock` | —— |
| 要最细刻度 | `high_resolution_clock` + 把 `now()` 开销算进结果 | 拿它当稳态时钟 |
| 跨尺度换算时长 | `std::chrono::duration_cast` | 隐式转换（周期不整除时直接编译不过，这是保护不是限制） |
| 等条件成立 | `condition_variable::wait(lock, pred)` | 只有 `wait(lock)`（伪唤醒）+ 手写 if 判断 |
| 等一个时长 | `wait_for` / `wait_until` + 谓词 | 以为「到点就一定返回」——超时只是下限，线程可能被阻塞更久 |
| 循环里反复等超时 | `wait_until`（每次用同一个绝对时刻） | `wait_for`（每轮重新计时 → 总时长会被无限拉长） |
| 退出时唤醒所有等待者 | `notify_all_at_thread_exit`（并确保线程尽快退出、不做阻塞操作） | 手动 `notify_all` + 让锁持到退出（易死锁） |
| 锁类型不是 `unique_lock<std::mutex>` | `condition_variable_any` | `condition_variable`（不接受） |
| 多线程取同一个结果 | `shared_future`，**每个线程各拷一份** | 多个线程访问同一个 `shared_future` 对象（对象本身不同步） |
| 真正要并行的异步任务 | `std::async(std::launch::async, f)` | `std::async(f)`（省略策略时实现可能选 `deferred`，根本不并行） |
| 延迟执行 | `std::launch::deferred`（并知道它第一次 `get()` 时才在调用线程上跑） | 误以为它会「先跑着」 |
| 只在主线程执行一次的初始化 | `std::call_once` + `once_flag` | 裸 `std::once_flag` + 手写 `if`（没有 happens-before） |
| 多把锁、顺序可能不一致 | `std::scoped_lock` 或 `std::lock` | 手动按相反顺序 `lock()` 两把（死锁） |
| 锁要延迟加 / 试加 / 手动解 | `std::unique_lock`（`defer_lock` / `try_to_lock` / `adopt_lock`） | `lock_guard`（它不给你这些选项） |
| 忘了 `join()` 就析构 `std::thread` | 用 RAII 包装（第2章的 `thread_guard` 思路） | 裸 `std::thread`（析构直接 `std::terminate()`） |
| 想知道能开几个线程 | `hardware_concurrency()` 并**处理返回 0** | 直接拿它做除数 / 当上限（且它给的是处理器数，不是硬件线程数） |
| 自旋锁 | `std::atomic_flag`（唯一保证无锁的原子类型） | 随便挑个 `std::atomic<int>`（是否无锁要查 `ATOMIC_xxx_LOCK_FREE`） |
| 原子量要放进容器 | `std::atomic<T*>` 或 `unique_ptr` 包 | 直接放 `std::atomic<T>`（不可拷贝，容器放不下） |
