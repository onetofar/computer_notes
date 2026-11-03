// @book: Linux Kernel Programming
// @topic: static-analysis-report-vs-block
// @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.4
// @expect: 二次 free 同一指针, 正常运行会 abort/段错误, 交给 valgrind 才明确报出 Invalid free
// @ub: yes
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* volatile: 让编译器无法推断 saved 与 p 同值, 也就无法把两次 free 合并掉 */
    static void *volatile saved;
    int *p = malloc(sizeof(int) * 4);

    if (p == NULL)
        return 1;

    for (int i = 0; i < 4; i++)
        p[i] = i * 7;

    printf("第一次 free 之前: p[0]=%d p[3]=%d\n", p[0], p[3]);

    saved = p;
    free(p);

    /* R1 命中的就是这一行: 同一块内存被释放了第二次 */
    free(saved);

    printf("这一行不会打印: 第二次 free 已经触发未定义行为\n");
    return 0;
}
