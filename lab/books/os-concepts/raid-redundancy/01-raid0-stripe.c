// @book: 操作系统导论
// @topic: raid-redundancy
// @source: 操作系统导论/第38章 廉价冗余磁盘阵列（RAID）（讲义）.md §38.4
// @expect: 5 盘条带化: 容量 5 块, 每个逻辑块 1 次物理写, 坏一块就永久丢数据
#include <stdio.h>

#define NDISK 5
#define NBLOCK 10                      /* 逻辑块数 */

static int g_disk_of[NBLOCK];          /* 逻辑块落在哪块盘 */
static int g_off_of[NBLOCK];           /* 落在盘上的第几块 */
static int g_alive[NDISK];             /* 盘是否还活着 */

int main(void)
{
    int writes = 0, lost = 0, recovered = 0;

    for (int i = 0; i < NDISK; i++)
        g_alive[i] = 1;

    printf("逻辑块 -> (盘号, 偏移) 映射: 盘号 = A %% %d, 偏移 = A / %d\n", NDISK, NDISK);
    for (int a = 0; a < NBLOCK; a++) {
        g_disk_of[a] = a % NDISK;       /* 条带化: 轮转摊到各盘 */
        g_off_of[a] = a / NDISK;
        writes++;                       /* 每个逻辑块一次物理写 */
        printf("  块 %2d -> 磁盘%d 偏移%d\n", a, g_disk_of[a], g_off_of[a]);
    }

    g_alive[2] = 0;                     /* 磁盘2 整盘死亡(fail-stop) */
    printf("\n磁盘2 故障后:\n");
    for (int a = 0; a < NBLOCK; a++) {
        if (!g_alive[g_disk_of[a]]) {
            lost++;
            printf("  块 %2d 位于已死的磁盘%d, 无法恢复\n", a, g_disk_of[a]);
        } else {
            recovered++;
        }
    }
    printf("RAID-0: 容量=%d 块 物理写次数=%d 可恢复=%d/%d 丢失=%d (冗余=0)\n",
           NDISK * 2, writes, recovered, NBLOCK, lost);
    return lost > 0 ? 0 : 1;
}
