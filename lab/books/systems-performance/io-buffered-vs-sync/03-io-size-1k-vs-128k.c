// @book: Systems Performance
// @topic: io-buffered-vs-sync
// @source: Chapter 5. Applications（讲义）.md §5.2.1 Selecting an I/O Size / Caching / Buffering
// @expect: 同样的 8MB, 分成 1KB 写 8192 次与 128KB 写 64 次; 后者系统调用次数少 128 倍, 更快

#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statfs.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

/* 文件系统魔数: 这些常量不在 <linux/magic.h> 之外的公共头里, 直接写死更省事 */
#define SP_MAGIC_TMPFS 0x01021994UL
#define SP_MAGIC_BTRFS 0x9123683EUL
#define SP_MAGIC_EXT   0xEF53UL
#define SP_MAGIC_NFS   0x6969UL

/* tmpfs 上的 fsync 几乎不要钱, 缓冲写与同步写看不出差别 —— 必须先看清落在哪种文件系统上 */
static void report_fs(const char *dir)
{
    struct statfs fs;
    if (statfs(dir, &fs) != 0) {
        printf("fs_type         : unknown (%s)\n", dir);
        return;
    }
    if (fs.f_type == SP_MAGIC_TMPFS)
        printf("fs_type         : tmpfs  <- fsync is nearly free here, contrast is muted\n");
    else if (fs.f_type == SP_MAGIC_BTRFS)
        printf("fs_type         : btrfs\n");
    else if (fs.f_type == SP_MAGIC_EXT)
        printf("fs_type         : ext2/3/4\n");
    else if (fs.f_type == SP_MAGIC_NFS)
        printf("fs_type         : nfs\n");
    else
        printf("fs_type         : 0x%lx\n", (unsigned long)fs.f_type);
}

#define TOTAL_BYTES (8 * 1024 * 1024)

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

static long long run(int chunk_size, const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    char *buf = malloc((size_t)chunk_size);
    if (!buf) {
        close(fd);
        unlink(path);
        exit(1);
    }
    memset(buf, 'x', (size_t)chunk_size);

    long long t0 = now_ns();
    int calls = 0;
    for (int written = 0; written < TOTAL_BYTES; written += chunk_size) {
        if (write(fd, buf, (size_t)chunk_size) != chunk_size) {
            perror("write");
            free(buf);
            close(fd);
            unlink(path);
            exit(1);
        }
        calls++;
    }
    long long t1 = now_ns();

    free(buf);
    close(fd);
    unlink(path);

    printf("chunk_bytes    : %d\n", chunk_size);
    printf("syscalls       : %d\n", calls);
    printf("elapsed_ns     : %lld\n", t1 - t0);
    printf("throughput_MBs : %.1f\n", (double)TOTAL_BYTES / ((double)(t1 - t0) / 1e9) / 1e6);
    return t1 - t0;
}

int main(void)
{
    const char *dir = getenv("SP_IO_DIR");
    if (!dir || !*dir)
        dir = ".";

    char path[512];
    snprintf(path, sizeof path, "%s/sp-io-size.tmp", dir);
    printf("path           : %s\n", path);
    report_fs(dir);

    long long small = run(1024, path);
    long long large = run(128 * 1024, path);

    printf("\nratio_1k_over_128k : %.2fx\n", (double)small / (double)large);
    printf("note: same bytes moved; fewer, larger I/Os amortize the per-I/O fixed cost.\n");
    return 0;
}
