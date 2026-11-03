// @book: 深入理解计算机系统
// @topic: integer-overflow
// @source: 第2章 信息的表示与处理（讲义）.md §2.3.1 §2.3.8
// @expect: 无符号加法按 2^32 取模回绕且行为有定义, UINT_MAX + 1 == 0
#include <limits.h>
#include <stdio.h>

int main(void) {
    unsigned int u = UINT_MAX;

    printf("UINT_MAX     = %u\n", u);
    printf("UINT_MAX + 1 = %u   <- 无符号: 定义良好的取模回绕\n", u + 1u);
    printf("UINT_MAX + 2 = %u\n", u + 2u);
    printf("2^32         = %llu\n", 1ULL << 32);

    /* 同一串位, 两种解释: 无符号看是 3410065408, 按补码看是负数 */
    unsigned int p = 200u * 300u * 400u * 500u;
    printf("\n200*300*400*500 :\n");
    printf("  按无符号解释 = %u\n", p);
    printf("  按补码解释   = %d\n", (int)p);

    /* 讲义 §2.3.2: 无符号加法与补码加法的位级表示完全相同, 差别只在溢出后怎么解释 */
    printf("\n位级对照: UINT_MAX + 1 的位模式 = 0x%08x\n", (unsigned)(u + 1u));
    printf("同一串位按补码解释 = %d\n", (int)(u + 1u));
    return 0;
}
