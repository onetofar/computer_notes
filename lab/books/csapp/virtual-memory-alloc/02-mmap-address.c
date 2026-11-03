// @book: 深入理解计算机系统
// @topic: virtual-memory-alloc
// @source: 第9章 虚拟内存（讲义）.md §9.8.4 §9.9.1
// @expect: mmap 得到的地址页对齐(4096) 且远离堆; Windows 上用 VirtualAlloc 得到同样的页对齐特征
#define _DEFAULT_SOURCE 1        /* POSIX 扩展: MAP_ANONYMOUS 等 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static void *map_anon(size_t len) {
    return VirtualAlloc(NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void unmap_anon(void *p, size_t len) {
    (void)len;
    VirtualFree(p, 0, MEM_RELEASE);
}
static size_t page_size(void) {
    return 4096;
}
#else
#include <sys/mman.h>
#include <unistd.h>
static void *map_anon(size_t len) {
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
static void unmap_anon(void *p, size_t len) {
    (void)len;
    munmap(p, len);
}
static size_t page_size(void) {
    return (size_t)sysconf(_SC_PAGESIZE);
}
#endif

int main(void) {
    size_t ps = page_size();
    void *heap = malloc(4096);      /* 堆上的一块 */
    void *mapped = map_anon(4096);   /* 单独映射的一块 */

    if (heap == NULL || mapped == NULL) {
        printf("分配失败\n");
        return 1;
    }

    printf("页大小          = %zu 字节\n", ps);
    printf("malloc 返回     = %p   页对齐? %s\n", heap,
           ((uintptr_t)heap % ps) == 0 ? "是" : "否");
    printf("mmap  返回      = %p   页对齐? %s\n", mapped,
           ((uintptr_t)mapped % ps) == 0 ? "是" : "否");
    printf("两者相距        = %lld 字节 (mmap 另起一片区域, 不在堆上)\n",
           (long long)((char *)mapped - (char *)heap));

    *(int *)mapped = 42;
    printf("映射区里写的值  = %d (可读写)\n", *(int *)mapped);

    unmap_anon(mapped, 4096);
    free(heap);
    return 0;
}
