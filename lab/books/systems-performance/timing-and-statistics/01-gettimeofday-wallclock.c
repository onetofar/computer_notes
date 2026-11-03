// @book: Systems Performance
// @topic: timing-and-statistics
// @source: Chapter 12. Benchmarking（讲义）.md §12.3.9
// @expect: gettimeofday 给出墙钟时间戳, 分辨率 1us; 当前与 CLOCK_MONOTONIC 的差值接近 0, 但墙钟会被 NTP 步进/校时改动

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static volatile unsigned long sink;

static void busy_work(long iters)
{
    unsigned long x = 0;
    for (long i = 0; i < iters; i++)
        x += i ^ (x >> 3);
    sink = x;
}

int main(void)
{
    struct timeval tv0, tv1;
    struct timespec mono0, mono1, res;

    if (clock_getres(CLOCK_REALTIME, &res) == 0)
        printf("gettimeofday resolution : %ld ns (接口是 us 粒度, 内核实际给到 ns)\n", res.tv_nsec);

    gettimeofday(&tv0, NULL);
    clock_gettime(CLOCK_MONOTONIC, &mono0);
    busy_work(2000000);
    clock_gettime(CLOCK_MONOTONIC, &mono1);
    gettimeofday(&tv1, NULL);

    long long wall_us = (long long)(tv1.tv_sec - tv0.tv_sec) * 1000000LL
                      + (long long)(tv1.tv_usec - tv0.tv_usec);
    long long mono_ns = (long long)(mono1.tv_sec - mono0.tv_sec) * 1000000000LL
                      + (long long)(mono1.tv_nsec - mono0.tv_nsec);

    printf("elapsed via gettimeofday   : %lld us\n", wall_us);
    printf("elapsed via CLOCK_MONOTONIC: %lld ns (%.3f us)\n", mono_ns, mono_ns / 1000.0);
    printf("difference wall - mono     : %lld ns\n", wall_us * 1000LL - mono_ns);

    time_t secs = tv1.tv_sec;
    struct tm tmv;
    char buf[64];
    localtime_r(&secs, &tmv);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tmv);
    printf("wall clock timestamp       : %s (epoch %lld)\n", buf, (long long)tv1.tv_sec);
    printf("note: a wall clock is not monotonic; an NTP step can move it backwards.\n");
    return 0;
}
