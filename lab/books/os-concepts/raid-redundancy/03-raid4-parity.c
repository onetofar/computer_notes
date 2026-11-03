// @book: 操作系统导论
// @topic: raid-redundancy
// @source: 操作系统导论/第38章 廉价冗余磁盘阵列（RAID）（讲义）.md §38.6
// @expect: 4 数据盘 + 1 校验盘: 容量 4 块, 坏一块可用 XOR 恢复, 小写要 4 个物理 I/O
#include <stdio.h>

#define NDATA 4
#define NDISK (NDATA + 1)              /* 4 块数据盘 + 1 块校验盘 */
#define NBLOCK NDATA                   /* 一个条带 = 4 个逻辑块 */
#define BSIZE 4                        /* 每块 4 字节 */

static unsigned char g_data[NDATA][BSIZE];
static unsigned char g_parity[BSIZE];  /* 校验块: 每行 1 的个数必须是偶数 */
static int g_alive[NDISK];

/* 某一字节列的数据异或, 跳过坏掉的盘 */
static unsigned char xor_col(int skip, int k)
{
    unsigned char p = 0;
    for (int i = 0; i < NDATA; i++) {
        if (i == skip)
            continue;
        p ^= g_data[i][k];
    }
    return p;
}

/* 恢复坏盘: 校验位 ^ 其余所有数据位 */
static unsigned char recover_col(int skip, int k)
{
    return (unsigned char)(g_parity[k] ^ xor_col(skip, k));
}

int main(void)
{
    int writes = 0, small_write_io = 0, ok = 1;

    for (int i = 0; i < NDISK; i++)
        g_alive[i] = 1;

    /* 全条带写入: 4 个数据块 + 1 个校验块, 5 次物理写 */
    for (int a = 0; a < NBLOCK; a++) {
        for (int k = 0; k < BSIZE; k++)
            g_data[a][k] = (unsigned char)(a * BSIZE + k);
        writes++;
    }
    for (int k = 0; k < BSIZE; k++)
        g_parity[k] = xor_col(-1, k);
    writes++;
    printf("全条带写入: 逻辑块=%d 物理写次数=%d (4 数据 + 1 校验)\n", NBLOCK, writes);

    /* 小写: 只改块1 —— 读旧数据 + 读旧校验 + 写新数据 + 写新校验 = 4 个物理 I/O */
    {
        unsigned char old_data = g_data[1][0];
        unsigned char old_parity = g_parity[0];
        unsigned char new_data = 0xFF;
        small_write_io = 4;
        g_data[1][0] = new_data;
        g_parity[0] = (unsigned char)(old_parity ^ old_data ^ new_data);
        printf("小写(改块1): 物理 I/O=%d (2 读 + 2 写), 新校验位=%02x\n",
               small_write_io, g_parity[0]);
    }

    g_alive[2] = 0;                     /* 磁盘2 整盘死亡 */
    printf("\n磁盘2 故障后用 XOR 恢复(校验位 ^ 其余数据位):\n");
    for (int k = 0; k < BSIZE; k++) {
        unsigned char rec = recover_col(2, k);
        g_data[2][k] = rec;
        printf("  恢复 磁盘2 的第 %d 字节 = %02x\n", k, rec);
    }
    for (int k = 0; k < BSIZE; k++) {
        unsigned char p = 0;
        for (int i = 0; i < NDATA; i++)
            p ^= g_data[i][k];
        if (p != g_parity[k])
            ok = 0;
    }
    printf("RAID-4: 容量=%d 块 全条带写=%d 次 小写=%d 次 I/O 恢复校验=%s (冗余=1)\n",
           NDISK - 1, writes, small_write_io, ok ? "通过" : "失败");
    return ok ? 0 : 1;
}
