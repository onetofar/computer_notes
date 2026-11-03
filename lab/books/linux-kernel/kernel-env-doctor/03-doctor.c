// @book: Linux Kernel Programming
// @topic: kernel-env-doctor
// @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.7（另见讲义 1.2 / 2.1 节）
// @expect: 用 C 复刻 01-doctor.sh 的 6 项内核侧体检, 逐项结论与 shell 版一致
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

static int g_pass = 0, g_miss = 0;

static void report(int good, const char *name, const char *detail)
{
    g_pass += good;
    g_miss += !good;
    printf("%s %-16s %s\n", good ? "[ OK ]" : "[MISS]", name, detail);
}

/* 0 = 不存在, 1 = 目录, 2 = 悬空软链接 */
static int path_kind(const char *p)
{
    struct stat st;

    if (lstat(p, &st) != 0)
        return 0;
    if (S_ISLNK(st.st_mode))
        return 2;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/* /proc/cpuinfo 的 flags/features 行里有没有这个标记 */
static int cpuinfo_has(const char *key)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[4096];
    int found = 0;

    if (!f)
        return 0;
    while (fgets(line, sizeof line, f))
        if ((strstr(line, "flags") || strstr(line, "Features")) && strstr(line, key)) {
            found = 1;
            break;
        }
    fclose(f);
    return found;
}

int main(void)
{
    struct utsname uts;
    char path[256], buf[512], detail[1024], target[512];
    long mem_kb = 0;
    int got;
    FILE *f;

    if (uname(&uts) != 0) {
        perror("uname");
        return 1;
    }
    report(1, "内核版本", uts.release);

    snprintf(path, sizeof path, "/lib/modules/%s/build", uts.release);
    got = path_kind(path);
    if (got == 1) {
        report(1, "内核头文件", path);
    } else {
        ssize_t k = got == 2 ? readlink(path, target, sizeof target - 1) : -1;
        if (k > 0) {
            target[k] = '\0';
            snprintf(detail, sizeof detail, "%s 是悬空软链接(指向 %s, 目标不存在)", path, target);
        } else {
            snprintf(detail, sizeof detail, "%s 不存在 (装 kernel-devel 才能编译模块)", path);
        }
        report(0, "内核头文件", detail);
    }

    f = fopen("/proc/version", "r");
    got = (f && fgets(buf, sizeof buf, f)) ? 1 : 0;
    if (f)
        fclose(f);
    if (got) {
        buf[strcspn(buf, "\r\n")] = '\0';
        report(1, "proc/version", buf);
    } else {
        report(0, "proc/version", "读不到 /proc/version");
    }

    if (cpuinfo_has("vmx"))
        report(1, "虚拟化扩展", "检测到 vmx");
    else if (cpuinfo_has("svm"))
        report(1, "虚拟化扩展", "检测到 svm");
    else
        report(0, "虚拟化扩展", "没有 vmx/svm (装 64 位客机要在 BIOS 里打开虚拟化)");

    report(1, "编译器", "gcc (GCC) " __VERSION__);

    f = fopen("/proc/meminfo", "r");
    got = (f && fscanf(f, "MemTotal: %ld kB", &mem_kb) == 1);
    if (f)
        fclose(f);
    if (got) {
        snprintf(detail, sizeof detail, "%ld MB 总内存", mem_kb / 1024);
        report(1, "内存", detail);
    } else {
        report(0, "内存", "读不到 /proc/meminfo 的 MemTotal");
    }

    printf("%s\n", "----------------------------------------");
    printf("体检结论: %d 项通过, %d 项缺失\n", g_pass, g_miss);
    if (g_miss == 0)
        printf("%s\n", "结论    : 本机具备编译内核/内核模块的基本条件");
    else
        printf("结论    : 有 %d 项前置条件缺失, 见上面 MISS 行\n", g_miss);
    printf("%s\n", "note     : 报告你观察到的这个具体环境, 而不是理想化的通用结论");
    return 0;
}
