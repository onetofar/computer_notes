// @book: 深入理解计算机系统
// @topic: pass-by-value-vs-pointer
// @source: 第3章 程序的机器级表示（讲义）.md §3.7.3 §3.7.4
// @expect: 按值传 64KB 结构体时, 被调用者栈帧比调用者低约 64KB; 按指针传时两者只差几十字节
#include <stdio.h>
#include <time.h>

#define NDOUBLE 8192        /* 64 KB */
#define ROUNDS  2000

typedef struct {
    double v[NDOUBLE];
    double scratch;         /* 只用来观察"改的是副本还是原对象", 不参与求和 */
} blob;

static blob g_blob;          /* 放在数据区, 不占栈 */
static double *callee_slot;  /* 记下被调用者栈帧的位置 */

/* 按值传: 调用者必须把整个结构体复制一份, 放在被调用者栈帧上面的参数区.
   callee 写了参数, 这份副本必须是它自己的, 编译器没法用指针顶替. */
static double __attribute__((noinline)) by_value(blob b) {
    double probe = 0.0;      /* 取地址, 逼它落在栈上, 用来量栈帧位置 */
    callee_slot = &probe;
    b.scratch += 1.0;        /* 改的是副本, 调用者看不到 */
    double s = 0.0;
    for (int i = 0; i < NDOUBLE; i++) {
        s += b.v[i];
    }
    return s;
}

int main(void) {
    double caller_probe = 0.0;
    for (int i = 0; i < NDOUBLE; i++) {
        g_blob.v[i] = (double)i;
    }

    printf("结构体大小 = %zu 字节, 调用 %d 次\n", sizeof(blob), ROUNDS);

    clock_t t0 = clock();
    double sum = 0.0;
    for (int i = 0; i < ROUNDS; i++) {
        sum += by_value(g_blob);
        g_blob.v[0] += 1.0;  /* 每次调用都改输入, 防止编译器把整个循环优化掉 */
    }
    clock_t t1 = clock();

    double ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    printf("按值传递: CPU 时间 = %.3f ms, 每次 %.4f ms\n", ms, ms / ROUNDS);
    printf("每次调用额外拷贝了 %zu 字节\n", sizeof(blob));
    printf("  调用者栈帧   %p\n", (void *)&caller_probe);
    printf("  被调用者栈帧 %p\n", (void *)callee_slot);
    printf("  两者相差     %lld 字节 (参数区就搭在调用者栈帧下面)\n",
           (long long)((char *)&caller_probe - (char *)callee_slot));
    printf("校验和 = %.1f\n", sum);
    printf("g_blob.scratch = %.1f  <- 副本里的改动传不回来\n", g_blob.scratch);
    return 0;
}
