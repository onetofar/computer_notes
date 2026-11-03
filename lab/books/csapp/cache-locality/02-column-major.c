// @book: 深入理解计算机系统
// @topic: cache-locality
// @source: 第6章 存储器层次结构（讲义）.md §6.5 §6.6.2
// @expect: 列序遍历同一块数组明显慢于行序: 内层循环每次跨一整行, 每次访问都要重新取块
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

    /* sumarraycols: 内层循环每次跳过一整行, stride = N */
    clock_t t0 = clock();
    double sum = 0.0;
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            sum += a[i][j];
        }
    }
    clock_t t1 = clock();

    double ms = 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC;
    printf("列序 sumarraycols : %8.3f ms   sum = %.1f\n", ms, sum);
    printf("  每次迭代跨 %zu 字节 (stride = %d)\n", sizeof(double) * N, N);
    printf("  工作集 %zu 字节\n", sizeof(a));
    return 0;
}
