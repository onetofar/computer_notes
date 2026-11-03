// @book: 操作系统导论
// @topic: raid-redundancy
// @source: 操作系统导论/第38章 廉价冗余磁盘阵列（RAID）（讲义）.md §38.5
// @expect: 4 盘两两镜像: 容量 2 块, 每个逻辑块 2 次物理写, 坏一块可从副本恢复
#include <stdio.h>

#define NDISK 4
#define NBLOCK 2                       /* 逻辑块数: 容量 = 盘数 / 2 */

static int g_copy1[NBLOCK];            /* 第一份副本所在盘 */
static int g_copy2[NBLOCK];            /* 第二份副本所在盘 */
static int g_alive[NDISK];

int main(void)
{
    int writes = 0, recovered = 0, lost = 0;

    for (int i = 0; i < NDISK; i++)
        g_alive[i] = 1;

    for (int a = 0; a < NBLOCK; a++) {
        g_copy1[a] = a * 2;            /* 镜像对: (0,1) (2,3) */
        g_copy2[a] = a * 2 + 1;
        writes += 2;                   /* 每个逻辑块写两份 */
        printf("  块 %d -> 磁盘%d + 磁盘%d\n", a, g_copy1[a], g_copy2[a]);
    }

    g_alive[1] = 0;                     /* 磁盘1 整盘死亡 */
    printf("\n磁盘1 故障后:\n");
    for (int a = 0; a < NBLOCK; a++) {
        int ok = g_alive[g_copy1[a]] || g_alive[g_copy2[a]];
        if (ok) {
            recovered++;
            printf("  块 %d 可从%s恢复\n", a,
                   g_alive[g_copy1[a]] ? "第一份副本" : "第二份副本");
        } else {
            lost++;
            printf("  块 %d 两份副本都死了, 无法恢复\n", a);
        }
    }
    printf("RAID-1: 容量=%d 块 物理写次数=%d 可恢复=%d/%d 丢失=%d (冗余=1)\n",
           NDISK / 2, writes, recovered, NBLOCK, lost);
    return lost == 0 && recovered == NBLOCK ? 0 : 1;
}
