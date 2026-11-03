// @book: 深入理解计算机系统
// @topic: integer-overflow
// @source: 第2章 信息的表示与处理（讲义）.md §2.2.5
// @expect: -1 < 0U 为假, 2147483647U > -2147483647-1 为假 —— 隐式转无符号导致比较结果反转
#include <stdio.h>

/* 下面这几行故意写成"一边有符号一边无符号"——这正是讲义 §2.2.5 要讲的坑。
   gcc 会用 -Wsign-compare 提醒"这里有隐式转换"，而我们要保留原样来复现讲义的求值表，
   所以就地关掉这一条警告(其余警告照常生效)。 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

int main(void) {
    printf("表达式                          类型      求值\n");
    printf("--------------------------------------------------------\n");
    printf("-1 < 0                          有符号     %d\n", -1 < 0);
    printf("-1 < 0U                         无符号     %d   <- 反直觉\n", -1 < 0u);
    printf("2147483647 > -2147483647-1      有符号     %d\n", 2147483647 > -2147483647 - 1);
    printf("2147483647U > -2147483647-1     无符号     %d   <- 反直觉\n", 2147483647u > -2147483647 - 1);
    printf("-1 > -2                        有符号     %d\n", -1 > -2);
    printf("(unsigned)-1 > -2              无符号     %d   <- 反直觉\n", (unsigned)-1 > -2);

    /* 讲义 §2.2.5 的两个换算: T2U_32(-1) = UMax_32, U2T_32(2^31) = TMin_32 */
    printf("\nT2U_32(-1)      = %u\n", (unsigned)-1);
    printf("U2T_32(2^31)    = %d\n", (int)2147483648u);
    printf("sizeof(int)=%zu sizeof(unsigned)=%zu\n", sizeof(int), sizeof(unsigned));
    return 0;
}

#pragma GCC diagnostic pop
