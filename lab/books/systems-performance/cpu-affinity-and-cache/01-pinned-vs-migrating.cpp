// @book: Systems Performance
// @topic: cpu-affinity-and-cache
// @source: Chapter 6. CPUs（讲义）.md §6.4.2 Software：调度器与亲和性
// @expect: 绑核与不绑核的均值可能接近, 但绑核后运行间波动更小; 迁移的代价是缓存变冷

#include <pthread.h>
#include <sched.h>
#include <cstdio>
#include <ctime>
#include <vector>

namespace {

constexpr int kThreads = 4;
constexpr long kIters = 40000000L;

double now_seconds()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

long g_sink = 0;

void* worker(void*)
{
    long acc = 0;
    for (long i = 0; i < kIters; i++)
        acc += i ^ (acc >> 3);
    g_sink += acc;
    return nullptr;
}

double run_pinned()
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setaffinity_np(&attr, sizeof set, &set);

    pthread_t th[kThreads];
    double t0 = now_seconds();
    for (int i = 0; i < kThreads; i++)
        pthread_create(&th[i], &attr, worker, nullptr);
    for (int i = 0; i < kThreads; i++)
        pthread_join(th[i], nullptr);
    double t1 = now_seconds();

    pthread_attr_destroy(&attr);
    return t1 - t0;
}

double run_migrating()
{
    pthread_t th[kThreads];
    double t0 = now_seconds();
    for (int i = 0; i < kThreads; i++)
        pthread_create(&th[i], nullptr, worker, nullptr);
    for (int i = 0; i < kThreads; i++)
        pthread_join(th[i], nullptr);
    double t1 = now_seconds();
    return t1 - t0;
}

double mean_of(const std::vector<double>& v)
{
    double s = 0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}

double spread_of(const std::vector<double>& v)
{
    double m = mean_of(v), d = 0;
    for (double x : v) {
        double diff = x - m;
        d += diff * diff;
    }
    return d / static_cast<double>(v.size());
}

}  // namespace

int main()
{
    std::vector<double> pinned, migrating;
    for (int r = 0; r < 5; r++) {
        pinned.push_back(run_pinned());
        migrating.push_back(run_migrating());
    }

    std::printf("pinned_mean_s      : %.4f\n", mean_of(pinned));
    std::printf("pinned_variance    : %.6f\n", spread_of(pinned));
    std::printf("migrating_mean_s   : %.4f\n", mean_of(migrating));
    std::printf("migrating_variance : %.6f\n", spread_of(migrating));
    std::printf("note: migration is avoided when its cost exceeds its benefit, so caches stay warm.\n");
    return 0;
}
