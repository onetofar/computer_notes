// @book: 深入理解计算机系统
// @topic: virtual-memory-alloc
// @source: 第9章 虚拟内存（讲义）.md §9.9.1
// @expect: malloc 返回的地址只保证 16 字节对齐(64 位模式), 两次连续 malloc 紧挨着, 说明来自同一块堆
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    void *a = malloc(4096);
    void *b = malloc(4096);
    void *c = malloc(64);
    if (a == NULL || b == NULL || c == NULL) {
        return 1;
    }

    printf("malloc#1 (4096B) = %p\n", a);
    printf("malloc#2 (4096B) = %p\n", b);
    printf("malloc#3 (64B)   = %p\n", c);

    printf("\n16 字节对齐?  %d %d %d\n",
           (int)(((uintptr_t)a % 16) == 0), (int)(((uintptr_t)b % 16) == 0),
           (int)(((uintptr_t)c % 16) == 0));
    printf("页对齐(4096)?  %d %d %d\n",
           (int)(((uintptr_t)a % 4096) == 0), (int)(((uintptr_t)b % 4096) == 0),
           (int)(((uintptr_t)c % 4096) == 0));

    printf("\n#2 - #1 = %lld 字节   (从同一块堆上往前切)\n",
           (long long)((char *)b - (char *)a));
    printf("#3 - #2 = %lld 字节\n", (long long)((char *)c - (char *)b));

    printf("\nmalloc 不初始化内存, 刚拿到的块里是旧数据\n");
    free(a);
    free(b);
    free(c);
    return 0;
}
