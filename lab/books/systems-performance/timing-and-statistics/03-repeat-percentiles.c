// @book: Systems Performance
// @topic: timing-and-statistics
// @source: Chapter 2. Methodologies（讲义）.md §2.8.3 Standard Deviation, Percentiles, Median
// @expect: 1000 次采样后, 首次单次值(first)与中位数可能差两个数量级(实测 3701 ns vs 35 ns); p95 与 max 把尾部暴露出来

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 1000

static volatile long sink;
static long long samples[N];

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

static int cmp_ll(const void *a, const void *b)
{
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

/* 位运算整数平方根, 避免依赖 libm */
static unsigned long long isqrt(unsigned long long x)
{
    unsigned long long res = 0, bit = 1ULL << 62;
    while (bit > x)
        bit >>= 2;
    while (bit) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

int main(void)
{
    long long first = 0;

    for (int i = 0; i < N; i++) {
        long long t0 = now_ns();
        long long acc = 0;
        for (int j = 0; j < 65536; j++)
            acc += (long long)j * 3;
        sink += acc;
        samples[i] = now_ns() - t0;
        if (i == 0)
            first = samples[i];
    }

    qsort(samples, N, sizeof samples[0], cmp_ll);

    unsigned long long sum = 0;
    for (int i = 0; i < N; i++)
        sum += (unsigned long long)samples[i];
    unsigned long long mean = sum / N;

    unsigned long long dev = 0;
    for (int i = 0; i < N; i++) {
        long long d = samples[i] - (long long)mean;
        dev += (unsigned long long)(d * d);
    }

    printf("iterations   : %d\n", N);
    printf("op_ns_first  : %lld   <- what a single-shot benchmark would report\n", first);
    printf("op_ns_min    : %lld\n", samples[0]);
    printf("op_ns_median : %lld\n", samples[N / 2]);
    printf("op_ns_p95    : %lld\n", samples[(int)(N * 0.95)]);
    printf("op_ns_p99    : %lld\n", samples[(int)(N * 0.99)]);
    printf("op_ns_max    : %lld\n", samples[N - 1]);
    printf("op_ns_mean   : %llu\n", mean);
    printf("op_ns_stddev : %llu\n", isqrt(dev / N));
    return 0;
}
