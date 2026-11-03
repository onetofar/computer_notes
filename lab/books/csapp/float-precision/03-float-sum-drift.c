// @book: 深入理解计算机系统
// @topic: float-precision
// @source: 第2章 信息的表示与处理（讲义）.md §2.4.4 §2.4.5
// @expect: 累加 100 万次 0.1f, 朴素累加误差约 9.6e+02, 补偿求和把误差压到 float 舍入间隔的量级
#include <stdio.h>

#define N 1000000

int main(void) {
    float fs = 0.0f;          /* 朴素累加: 每加一次都舍入回 float */
    float ks = 0.0f;          /* 补偿求和: 额外记住被舍掉的那部分 */
    float c = 0.0f;
    double ds = 0.0;

    for (int i = 0; i < N; i++) {
        fs += 0.1f;
        ds += 0.1;

        float y = 0.1f - c;   /* 把上一轮丢掉的误差补回来 */
        float t = ks + y;
        c = (t - ks) - y;     /* 这一轮又舍掉多少 */
        ks = t;
    }

    /* 注意: 累加的是 0.1f 本身, 它的精确值不是 0.1 */
    double target = (double)0.1f * N;
    printf("累加 %d 次 0.1f\n", N);
    printf("  精确目标 (0.1f*N) = %.10f\n", target);
    printf("  float 朴素        = %.6f   误差 = %.6e\n", (double)fs, (double)fs - target);
    printf("  float 补偿求和    = %.6f   误差 = %.6e\n", (double)ks, (double)ks - target);
    printf("  double 朴素       = %.10f   相对 0.1*N 的误差 = %.6e\n", ds, ds - 0.1 * N);

    /* 1e5 附近 float 的间隔是 2^-7 = 0.0078125, 补偿求和也只能给到这个精度 */
    printf("\n1e5 附近 float 的最小间隔 = %.7f\n", 0.0078125);
    printf("0.1f 的精确值 = %.21f\n", (double)0.1f);
    return 0;
}
