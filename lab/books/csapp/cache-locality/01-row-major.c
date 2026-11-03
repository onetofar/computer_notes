// @book: 深入理解计算机系统
// @topic: cache-locality
// @source: 第6章 存储器层次结构（讲义）.md §6.5 §6.6.2
// @expect: 行序遍历 32MB 二维数组明显快于列序遍历, 两者求和结果完全相同
#include <stdio.h>
#include <time.h>

#define N 2048              /* 2048 x 2048 个 double = 32 MB, 装不进 L2 */

static double a[N][N];

int main(void) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            a[i][j] = (double)(i + j);
        }
    }

    /* sumarrayrows: 内层循环沿着内存里的连续块走, stride = 1 */
    clock_t t0 = clock();
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            sum += a[i][j];
        }
    }
    clock_t t1 = clock();

    double ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    printf("行序 sumarrayrows : %8.3f ms   sum = %.1f\n", ms, sum);
    printf("  每次迭代走 %zu 字节连续内存 (stride = 1)\n", sizeof(double));
    printf("  工作集 %zu 字节\n", sizeof(a));
    return 0;
}
