// @book: 深入理解计算机系统
// @topic: virtual-memory-alloc
// @source: 第9章 虚拟内存（讲义）.md §9.11.9
// @expect: free 之后再读这块内存是未定义行为, 值已被下一次 malloc 覆盖 (需 valgrind/ASan 检出)
// @ub: yes
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *x = malloc(sizeof(int) * 4);
    if (x == NULL) {
        return 1;
    }
    for (int i = 0; i < 4; i++) {
        x[i] = 100 + i;
    }
    printf("释放前 x[0..3] = %d %d %d %d\n", x[0], x[1], x[2], x[3]);

    free(x);                              /* 块已经回到空闲链表 */

    int *y = malloc(sizeof(int) * 4);    /* 很可能复用同一块 */
    if (y == NULL) {
        return 1;
    }
    for (int i = 0; i < 4; i++) {
        y[i] = -1;                       /* 把旧内容覆盖掉 */
    }
    printf("新的 y[0..3]    = %d %d %d %d\n", y[0], y[1], y[2], y[3]);

    /* 指针经过一个 volatile 变量: gcc 的 -Wuse-after-free 看不见它,
       但真正的读动作照样发生 —— 这正是要演示的未定义行为. */
    int *volatile dangling = x;
    printf("再读已释放的 x[0] = %d  <- 未定义行为, 读到的是别人的数据\n", dangling[0]);

    free(y);
    return 0;
}
