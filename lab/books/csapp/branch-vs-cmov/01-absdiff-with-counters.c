// @book: 深入理解计算机系统
// @topic: branch-vs-cmov
// @source: 第3章 程序的机器级表示（讲义）.md §3.6.5
// @expect: 两条路径各有副作用(volatile 计数器), 编译器只能生成条件跳转 jge, 不能用 cmov
#include <stdio.h>

/* volatile: 每次自增都要真的写内存, 编译器无法把两条路径合并成"算两个再选一个" */
static volatile long lt_cnt = 0;
static volatile long ge_cnt = 0;

/* 讲义 §3.6.5 的 absdiff_se: 两条路径各有各的副作用 */
static long __attribute__((noinline)) absdiff_se(long x, long y) {
    long result;
    if (x < y) {
        lt_cnt++;
        result = y - x;
    } else {
        ge_cnt++;
        result = x - y;
    }
    return result;
}

int main(void) {
    unsigned long long total = 0;
    for (long i = -100000; i < 100000; i++) {
        total += absdiff_se(i, -i);
    }
    printf("lt_cnt = %ld\n", lt_cnt);
    printf("ge_cnt = %ld\n", ge_cnt);
    printf("total  = %llu\n", total);
    return 0;
}
