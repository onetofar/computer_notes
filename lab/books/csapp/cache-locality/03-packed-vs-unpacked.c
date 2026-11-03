// @book: 深入理解计算机系统
// @topic: cache-locality
// @source: 第6章 存储器层次结构（讲义）.md §6.5
// @expect: 同样遍历一批记录, 分开存放(SoA)比结构体数组(AoS)少扫一半内存, 更快
#include <stdio.h>
#include <time.h>

#define N 8388608           /* 16 字节/条 -> 128 MB */

typedef struct {
    int id;
    double val;
} rec_t;

static rec_t recs[N];       /* 未打包: 数组里放结构体 */
static double vals[N];      /* 打包: 只把要用的字段拆出来单独存 */

int main(void) {
    for (int i = 0; i < N; i++) {
        recs[i].id = i;
        recs[i].val = (double)i;
        vals[i] = (double)i;
    }

    printf("记录数 %d\n", N);
    printf("  未打包 sizeof(rec_t) = %zu 字节, 取 val 要扫 %zu 字节\n",
           sizeof(rec_t), sizeof(rec_t) * N);
    printf("  打包   只存 val      = %zu 字节 (一半)\n", sizeof(double) * N);

    clock_t t0 = clock();
    double s1 = 0.0;
    for (int i = 0; i < N; i++) {
        s1 += recs[i].val;   /* 每 16 字节才有一个要用的 double */
    }
    clock_t t1 = clock();

    clock_t t2 = clock();
    double s2 = 0.0;
    for (int i = 0; i < N; i++) {
        s2 += vals[i];       /* 连续的 double, stride = 8 */
    }
    clock_t t3 = clock();

    double ms1 = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    double ms2 = 1000.0 * (double)(t3 - t2) / (double)CLOCKS_PER_SEC;
    printf("未打包 recs[i].val : %8.3f ms   sum = %.1f\n", ms1, s1);
    printf("打包   vals[i]     : %8.3f ms   sum = %.1f\n", ms2, s2);
    return 0;
}
