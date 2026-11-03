// @book: 深入理解计算机系统
// @topic: float-precision
// @source: 第2章 信息的表示与处理（讲义）.md §2.4.6
// @expect: float 尾数只有 24 位, double 53 位; 0.1 在 float 里存不下
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("sizeof(float)=%zu  sizeof(double)=%zu\n", sizeof(float), sizeof(double));
    printf("FLT_MANT_DIG = %d  DBL_MANT_DIG = %d   (尾数位数)\n", FLT_MANT_DIG, DBL_MANT_DIG);
    printf("FLT_DIG = %d  DBL_DIG = %d   (十进制有效位数)\n", FLT_DIG, DBL_DIG);
    printf("FLT_EPSILON = %.9e   (float  在 1.0 附近的最小间隔)\n", (double)FLT_EPSILON);
    printf("DBL_EPSILON = %.9e   (double 在 1.0 附近的最小间隔)\n", DBL_EPSILON);

    float f = 0.1f;
    double d = 0.1;
    unsigned int fb;
    uint64_t db;
    memcpy(&fb, &f, sizeof fb);
    memcpy(&db, &d, sizeof db);

    printf("\n同一个 0.1 的两种存放:\n");
    printf("  float  位模式 = 0x%08x\n", fb);
    printf("  double 位模式 = 0x%016llx\n", (unsigned long long)db);
    printf("  float  0.1 还原出来 = %.17g\n", (double)f);
    printf("  double 0.1 还原出来 = %.17g\n", d);
    printf("  0.1f == 0.1 ? %s\n", (0.1f == 0.1) ? "true" : "false");

    printf("\n1/3:\n");
    printf("  float  = %.9g\n", (double)(1.0f / 3.0f));
    printf("  double = %.17g\n", 1.0 / 3.0);

    printf("\nfloat 的尾数只有 %d 位, 存不下 16777217 (= 2^24 + 1):\n", FLT_MANT_DIG);
    printf("  float  16777217 -> %.1f\n", (double)16777217.0f);
    printf("  double 16777217 -> %.1f\n", 16777217.0);
    return 0;
}
