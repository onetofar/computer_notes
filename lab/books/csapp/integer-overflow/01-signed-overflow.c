// @book: 深入理解计算机系统
// @topic: integer-overflow
// @source: 第2章 信息的表示与处理（讲义）.md §2.3.2
// @expect: 200*300*400*500 打印出 -884901888, 与讲义给出的 mod 2^32 结果一致
#include <stdint.h>
#include <stdio.h>

int main(void) {
    /* volatile: 阻止编译器把常量折叠成一个已经算好的常量, 让溢出在运行时发生 */
    volatile int a = 200;
    volatile int b = 300;
    volatile int c = 400;
    volatile int d = 500;

    int product = a * b * c * d;   /* 超出 int 上界: 有符号溢出是未定义行为 */

    uint32_t bits = (uint32_t)product;
    printf("product = %d\n", product);
    printf("bits    = 0x%08x = %u\n", bits, (unsigned)bits);

    /* 讲义给出的算式: 先 mod 2^32, 再按补码解释 */
    unsigned long long exact = 12000000000ULL;
    unsigned long long wrapped = exact % 4294967296ULL;   /* 2^32 */
    printf("12000000000 mod 2^32 = %llu\n", wrapped);
    printf("%llu - 2^32         = %lld\n", wrapped, (long long)wrapped - 4294967296LL);

    printf("INT_MAX = %d\n", INT32_MAX);
    printf("INT_MIN = %d\n", INT32_MIN);
    return 0;
}
