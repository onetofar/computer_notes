// @book: 深入理解计算机系统
// @topic: pass-by-value-vs-pointer
// @source: 第3章 程序的机器级表示（讲义）.md §3.7.3
// @expect: 前 6 个整型参数走寄存器, 第 7、8 个落在栈上且相隔 8 字节, 位置在调用者栈帧之上
#include <stdio.h>

/* 6 个参数: 按 ABI 全走寄存器 %rdi %rsi %rdx %rcx %r8 %r9 */
static long long six(long long a1, long long a2, long long a3,
                     long long a4, long long a5, long long a6) {
    printf("  six(): 6 个参数, 按 ABI 都在寄存器里\n");
    return a1 + a2 + a3 + a4 + a5 + a6;
}

/* 8 个参数: 第 7、8 个走栈, 每个占一个 8 字节的槽 */
static long long eight(long long a1, long long a2, long long a3, long long a4,
                       long long a5, long long a6, long long a7, long long a8) {
    long long gap = (long long)((char *)&a8 - (char *)&a7);
    if (gap < 0) {
        gap = -gap;
    }
    printf("  eight(): 第 7 个参数在 %p\n", (void *)&a7);
    printf("  eight(): 第 8 个参数在 %p\n", (void *)&a8);
    printf("  eight(): 两者相隔 %lld 字节 (每个栈参数占 8 字节的槽)\n", gap);
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}

int main(void) {
    long long local = 0;

    printf("main() 的局部变量在 %p\n", (void *)&local);
    printf("x86-64 的栈向低地址增长, 所以被调用者的参数区在调用者栈帧之上(地址更高)\n\n");

    six(1, 2, 3, 4, 5, 6);
    printf("\n");
    eight(1, 2, 3, 4, 5, 6, 7, 8);

    printf("\n讲义 §3.7.3 按 SysV ABI 给出的偏移: 第 7 个在 %%rsp+8, 第 8 个在 %%rsp+16\n");
    printf("  %%rsp+0 留给返回地址 —— call 指令把它压进去的\n");
    printf("sizeof(long long) = %zu (Windows 与 Linux 都是 8), 栈槽也是 8 字节\n", sizeof(long long));
    return 0;
}
