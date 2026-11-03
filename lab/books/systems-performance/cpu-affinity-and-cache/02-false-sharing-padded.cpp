// @book: Systems Performance
// @topic: cpu-affinity-and-cache
// @source: Chapter 6. CPUs（讲义）.md §6.4.1 Cache Coherency
// @expect: 两个线程改同一 cache line 的相邻计数器明显更慢; 隔到不同 cache line 后恢复
//          (两个线程必须落在**不同物理核**上才有效果, 同核共享 L1 就没有一致性流量)

#include "pick_cores.h"

#include <cstdio>

namespace {

constexpr long kIters = 20000000L;

// 两个 hot 计数器挤在同一条 cache line -> 每次写都触发跨核一致性流量
struct Shared {
    volatile long a;
    volatile long b;
};

// 按 64B (x86 cache line) 对齐隔开 -> 各自独占一条 line
struct alignas(64) Padded {
    volatile long a;
    char pad[64 - sizeof(long)];
    volatile long b;
};

long g_sink = 0;

// job 里带上"改哪个字段", 一个 bump 函数覆盖两种布局
template <typename T>
struct Job {
    T* s;
    int which;
};

template <typename T>
void* bump(void* arg)
{
    const Job<T>& j = *static_cast<const Job<T>*>(arg);
    volatile long* target = (j.which == 0) ? &j.s->a : &j.s->b;
    for (long i = 0; i < kIters; i++)
        *target += 1;
    return nullptr;
}

template <typename T>
double bench(int cpu_a, int cpu_b)
{
    T s{};
    Job<T> job_a{&s, 0};
    Job<T> job_b{&s, 1};
    pthread_attr_t attr_a, attr_b;
    pthread_attr_init(&attr_a);
    pthread_attr_init(&attr_b);
    pin(&attr_a, cpu_a);
    pin(&attr_b, cpu_b);

    pthread_t t1, t2;
    double t0 = now_seconds();
    pthread_create(&t1, &attr_a, bump<T>, &job_a);
    pthread_create(&t2, &attr_b, bump<T>, &job_b);
    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);
    double t1end = now_seconds();

    pthread_attr_destroy(&attr_a);
    pthread_attr_destroy(&attr_b);
    g_sink += s.a + s.b;
    return t1end - t0;
}

}  // namespace

int main()
{
    std::vector<int> cpus = pick_two_cores();
    std::printf("cpu_a           : %d\n", cpus[0]);
    std::printf("cpu_b           : %d  (must be a different physical core)\n", cpus[1]);

    double shared = 0, padded = 0;
    for (int r = 0; r < 3; r++) {
        shared += bench<Shared>(cpus[0], cpus[1]);
        padded += bench<Padded>(cpus[0], cpus[1]);
    }
    shared /= 3.0;
    padded /= 3.0;

    std::printf("adjacent_counters_s : %.4f\n", shared);
    std::printf("padded_counters_s   : %.4f\n", padded);
    std::printf("ratio_adjacent_over_padded : %.2fx\n", shared / padded);
    std::printf("note: a cache line is 64 bytes; two hot counters in one line bounce between cores.\n");
    return 0;
}
