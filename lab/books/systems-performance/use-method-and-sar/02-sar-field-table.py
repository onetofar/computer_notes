#!/usr/bin/env python3
# @book: Systems Performance
# @topic: use-method-and-sar
# @source: Chapter 4. Observability Tools（讲义）.md §4.2.4 Monitoring：监控（字段表出自 附录 B. sar Summary（讲义）.md）
# @expect: 打印 sar 选项 -> 指标 -> 含义的对照表, 并给出"症状 -> 查哪一项"的映射; 无 sar 也可运行

import sys

# (option, metrics, description)  —— 按问题域重排
TABLE = [
    ("-u", "%user %nice %system %iowait %steal %idle", "CPU utilization"),
    ("-u ALL", "... %irq %soft %guest %gnice", "CPU utilization extended"),
    ("-u -P ALL", "同上 (按 CPU 逐个)", "Per-CPU utilization"),
    ("-m CPU -P ALL", "MHz", "Per-CPU frequency"),
    ("-q", "runq-sz plist-sz ldavg-1 ldavg-5 ldavg-15 blocked", "CPU run-queue size"),
    ("-w", "proc/s cswch/s", "CPU scheduler events"),
    ("-r", "kbmemfree kbavail kbmemused %memused kbbuffers kbcached kbcommit %commit kbactive kbinact kbdirty", "Memory utilization"),
    ("-S", "kbswpfree kbswused %swpused kbswpcad %swpcad", "Swap utilization"),
    ("-W", "pswpin/s pswpout/s", "Swapping statistics"),
    ("-B", "pgpgin/s pgpgout/s fault/s majflt/s pgfree/s pgscank/s pgscand/s pgsteal/s %vmeff", "Paging statistics"),
    ("-H", "kbhugfree kbhugused %hugused", "Huge pages"),
    ("-v", "dentunusd file-nr inode-nr pty-nr", "Kernel tables"),
    ("-d", "tps rkB/s wkB/s areq-sz aqu-sz await svctm %util", "Disk statistics"),
    ("-n DEV", "rxpck/s txpck/s rxkB/s txkB/s rxcmp/s txcmp/s rxmcst/s %ifutil", "Network interface statistics"),
    ("-n EDEV", "rxerr/s txerr/s coll/s rxdrop/s txdrop/s txcarr/s rxfram/s rxfifo/s txfifo/s", "Network interface errors"),
    ("-n IP", "irec/s fwddgm/s idel/s orq/s asmrq/s asmok/s fragok/s fragcrt/s", "IP statistics"),
    ("-n EIP", "ihdrerr/s iadrerr/s iukwnpr/s idisc/s odisc/s onort/s asmf/s fragf/s", "IP errors"),
    ("-n TCP", "active/s passive/s iseg/s oseg/s", "TCP statistics"),
    ("-n ETCP", "atmptf/s estres/s retrans/s isegerr/s orsts/s", "TCP errors"),
    ("-n SOCK", "totsck tcpsck udpsck rawsck ip-frag tcp-tw", "Socket statistics"),
]

# 症状 -> 先查哪个选项
SYMPTOMS = [
    ("CPU 不够", "-u", "%user/%system 高说明在算; %iowait 高说明不是 CPU 不够, 是 I/O 不够"),
    ("真的在排队了", "-q", "runq-sz > CPU 核数; blocked 大说明有进程卡在 D 状态"),
    ("上下文切换太多", "-w", "cswch/s 高说明 CPU 花在做切换上, 不是做正事"),
    ("内存吃紧", "-r", "%memused / kbavail; %commit 看是否要换出"),
    ("在换页", "-W", "pswpin/s pswpout/s 非零; 配合 -B 看页面扫描"),
    ("磁盘慢", "-d", "aqu-sz > 1 或 await 高 => 已经排队了"),
    ("网络错误", "-n EDEV / -n ETCP", "E 前缀的行是错误计数器, 平时恒为 0, 一有值就是事故"),
]


def main():
    print("=== sar 选项 -> 指标 -> 含义 ===")
    for opt, metrics, desc in TABLE:
        print("%-14s %-72s %s" % (opt, metrics, desc))

    print()
    print("=== 症状 -> 先查哪一项 ===")
    for symptom, opt, why in SYMPTOMS:
        print("%-18s -> %-20s %s" % (symptom, opt, why))

    print()
    print("note: 带 E 前缀的选项 (EDEV/EIP/ETCP) 是错误计数器, 平时恒为 0, 一有值就是事故。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
