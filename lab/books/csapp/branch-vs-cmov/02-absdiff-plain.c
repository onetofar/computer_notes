// @book: 深入理解计算机系统
// @topic: branch-vs-cmov
// @source: 第3章 程序的机器级表示（讲义）.md §3.6.6
// @expect: 同样的 absdiff, 去掉副作用后 gcc -O2 生成条件传送 cmov, 控制流与数据无关
#include <stdio.h>

/* 讲义 §3.6.6 的 absdiff: 两条路径都是纯计算, 编译器可以把它们都算出来再选一个 */
static long __attribute__((noinline)) absdiff(long x, long y) {
    long result;
    if (x < y) {
        result = y - x;
    } else {
        result = x - y;
    }
    return result;
}

int main(void) {
    unsigned long long total = 0;
    for (long i = -100000; i < 100000; i++) {
        total += absdiff(i, -i);
    }
    printf("total = %llu\n", total);
    return 0;
}
