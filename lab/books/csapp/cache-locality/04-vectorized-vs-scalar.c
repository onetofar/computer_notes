// @book: 深入理解计算机系统
// @topic: cache-locality
// @source: 第6章 存储器层次结构（讲义）.md §6.5 §6.6.1
// @expect: 同一个求和循环, volatile 累加器阻止编译器向量化后明显变慢
#include <stdio.h>
#include <time.h>

#define N 4000000

static double a[N];

int main(void) {
    for (int i = 0; i < N; i++) {
        a[i] = (double)(i % 97);
    }

    /* 普通累加器: -O2 下编译器可以把它改写成 SIMD 指令 */
    clock_t t0 = clock();
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        sum += a[i];
    }
    clock_t t1 = clock();

    /* volatile 累加器: 每次循环都要真写回内存, 编译器不敢向量化 */
    clock_t t2 = clock();
    volatile double vsum = 0.0;
    for (int i = 0; i < N; i++) {
        vsum += a[i];
    }
    clock_t t3 = clock();

    double ms1 = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    double ms2 = 1000.0 * (double)(t3 - t2) / (double)CLOCKS_PER_SEC;
    printf("普通累加器 (可被向量化) : %8.3f ms   sum = %.1f\n", ms1, sum);
    printf("volatile 累加器 (不可)   : %8.3f ms   sum = %.1f\n", ms2, (double)vsum);
    printf("倍数 = %.2fx\n", ms2 / ms1);

    /* 想确认编译器到底做了什么, 直接看汇编 */
    printf("\n看汇编:\n  gcc -std=c11 -O2 -S -o - 04-vectorized-vs-scalar.c | grep -E 'xmm|ymm|movsd|addsd' | head -6\n");
    return 0;
}
