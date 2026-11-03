// @book: 深入理解计算机系统
// @topic: integer-overflow
// @source: 第2章 信息的表示与处理（讲义）.md §2.2.8
// @expect: length 为 0 时 length-1 下溢成 UINT_MAX, 循环越界读堆内存 (需 valgrind/ASan 检出)
// @ub: yes
#include <stdio.h>
#include <stdlib.h>

/* 讲义 §2.2.8 例一: length 是 unsigned, 循环条件写成 i <= length - 1 */
static float sum_elements(const float *a, unsigned length) {
    unsigned i;
    float result = 0.0f;
    for (i = 0; i <= length - 1; i++)   /* <- bug: length == 0 时下溢成 UINT_MAX */
        result += a[i];
    return result;
}

int main(void) {
    float *a = malloc(sizeof(float) * 4);
    if (a == NULL) {
        return 1;
    }
    for (int i = 0; i < 4; i++) {
        a[i] = 1.0f;
    }

    /* length = 4 时循环边界恰好正确 */
    printf("length=4 -> %g   (i 取 0..3, 正常)\n", sum_elements(a, 4));
    fflush(stdout);

    /* length = 0: length - 1 = UINT_MAX, 条件 i <= UINT_MAX 永远成立 */
    printf("length=0 -> 开始越界读, 第一次读到的就是堆块之外\n");
    fflush(stdout);
    printf("length=0 -> %g\n", sum_elements(a, 0));

    free(a);
    return 0;
}
