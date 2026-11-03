// @book: Systems Performance
// @topic: io-buffered-vs-sync
// @source: Chapter 5. Applications（讲义）.md §5.2.1
// @expect: 2000 次 1KB write() 全部被 page cache 吸收, 吞吐极高; 数据此刻还在内存里, 未落盘
//          (落在 tmpfs 上 fsync 不要钱, 看不出同步代价; 用 SP_IO_DIR 指向真实磁盘)

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

#define CHUNKS 2000
#define CHUNK_SIZE 1024

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

int main(void)
{
    const char *dir = getenv("SP_IO_DIR");
    if (!dir || !*dir)
        dir = ".";

    char path[512];
    snprintf(path, sizeof path, "%s/sp-io-buffered.tmp", dir);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    char buf[CHUNK_SIZE];
    memset(buf, 'x', sizeof buf);

    long long t0 = now_ns();
    for (int i = 0; i < CHUNKS; i++) {
        if (write(fd, buf, CHUNK_SIZE) != CHUNK_SIZE) {
            perror("write");
            close(fd);
            unlink(path);
            return 1;
        }
    }
    long long t1 = now_ns();
    close(fd);
    unlink(path);

    double secs = (double)(t1 - t0) / 1e9;
    printf("path            : %s\n", path);
    report_fs(dir);
    printf("writes          : %d x %d B = %d B total\n", CHUNKS, CHUNK_SIZE, CHUNKS * CHUNK_SIZE);
    printf("elapsed_ns      : %lld\n", t1 - t0);
    printf("throughput_MBs  : %.1f\n", (double)(CHUNKS * CHUNK_SIZE) / secs / 1e6);
    printf("note: buffered write; the page cache absorbed it, nothing forced to disk.\n");
    return 0;
}
