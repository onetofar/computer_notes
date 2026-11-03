// @book: 深入理解计算机系统
// @topic: float-precision
// @source: 第2章 信息的表示与处理（讲义）.md §2.4.5
// @expect: 0.1+0.2 != 0.3, 换 epsilon 才判等; 括号位置不同结果不同(结合律不成立)
#include <float.h>
#include <stdio.h>

static double dabs(double v) {
    return v < 0.0 ? -v : v;
}

/* 绝对误差判据 + 相对误差判据 */
static int nearly_equal(double a, double b, double eps) {
    double diff = dabs(a - b);
    if (diff <= eps) {
        return 1;
    }
    double m = dabs(a) > dabs(b) ? dabs(a) : dabs(b);
    return diff <= eps * m;
}

int main(void) {
    double sum = 0.1 + 0.2;

    printf("0.1 + 0.2 = %.20f\n", sum);
    printf("0.3       = %.20f\n", 0.3);
    printf("逐位相同? %s\n", (sum == 0.3) ? "是" : "否");
    printf("差值 = %.6e\n", dabs(sum - 0.3));
    printf("DBL_EPSILON = %.6e\n", DBL_EPSILON);

    printf("\n换成 epsilon 判据:\n");
    printf("  eps = DBL_EPSILON (%.1e) -> %s\n", DBL_EPSILON,
           nearly_equal(sum, 0.3, DBL_EPSILON) ? "相等" : "不等");
    printf("  eps = 1e-9                  -> %s\n",
           nearly_equal(sum, 0.3, 1e-9) ? "相等" : "不等");
    printf("  eps = 1e-18                 -> %s   (epsilon 太小, 又判不出来)\n",
           nearly_equal(sum, 0.3, 1e-18) ? "相等" : "不等");

    /* 讲义 §2.4.5: 浮点加法不满足结合律 */
    printf("\n括号位置:\n");
    printf("  (3.14 + 1e20) - 1e20 = %g\n", (3.14 + 1e20) - 1e20);
    printf("  3.14 + (1e20 - 1e20) = %g\n", 3.14 + (1e20 - 1e20));
    return 0;
}
