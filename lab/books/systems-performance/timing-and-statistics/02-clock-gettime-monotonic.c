// @book: Systems Performance
// @topic: timing-and-statistics
// @source: Chapter 3. Operating Systems（讲义）.md §3.2.2 Kernel and User Modes：模式切换与上下文切换
// @expect: CLOCK_MONOTONIC 分辨率 1ns; 与 MONOTONIC_RAW 的差只反映 NTP 平滑率(ppm 级), 两者都不会因步进而跳变

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

static long long delta_ns(const struct timespec *a, const struct timespec *b)
{
    return (long long)(b->tv_sec - a->tv_sec) * 1000000000LL
         + (long long)(b->tv_nsec - a->tv_nsec);
}

int main(void)
{
    struct timespec mono, raw, mono2, raw2, res;
    struct timespec nap = { 0, 200000000L };

    if (clock_getres(CLOCK_MONOTONIC, &res) == 0)
        printf("CLOCK_MONOTONIC resolution     : %ld ns\n", res.tv_nsec);
    if (clock_getres(CLOCK_MONOTONIC_RAW, &res) == 0)
        printf("CLOCK_MONOTONIC_RAW resolution : %ld ns\n", res.tv_nsec);
    else
        printf("CLOCK_MONOTONIC_RAW            : unavailable on this kernel\n");

    clock_gettime(CLOCK_MONOTONIC, &mono);
    clock_gettime(CLOCK_MONOTONIC_RAW, &raw);
    nanosleep(&nap, NULL);
    clock_gettime(CLOCK_MONOTONIC, &mono2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &raw2);

    long long dm = delta_ns(&mono, &mono2);
    long long dr = delta_ns(&raw, &raw2);
    printf("MONOTONIC over 200ms sleep     : %lld ns\n", dm);
    printf("MONOTONIC_RAW over 200ms sleep : %lld ns\n", dr);
    if (dm > 0)
        printf("difference (NTP slew rate)     : %lld ns (%.1f ppm)\n",
               dm - dr, (double)(dm - dr) / (double)dm * 1e6);
    printf("note: MONOTONIC is slewed by NTP, RAW is not; neither jumps on a step.\n");
    return 0;
}
