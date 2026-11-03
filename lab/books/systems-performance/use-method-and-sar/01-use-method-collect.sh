#!/usr/bin/env bash
# @book: Systems Performance
# @topic: use-method-and-sar
# @source: 附录 A. USE Method： Linux（讲义）.md §附录 A 方法本身（方法出自 §2.5.9）
# @expect: 按 U/S/E 三类分别采集 CPU/内存/网络/磁盘指标; 缺失的工具打印说明而不报错退出

set -uo pipefail

echo "=== Utilization / Saturation / Errors 三类指标采集 ==="
echo

echo "--- CPU ---"
if command -v vmstat >/dev/null 2>&1; then
  echo "[U] vmstat: 忙碌占比"
  vmstat 1 2 | tail -1
  echo "[S] vmstat: r (可运行队列长度) 与 CPU 数比较"
  echo "    cpu_count=$(nproc)"
  vmstat 1 2 | tail -1 | awk '{print "    run_queue_r=" $1 "  blocked_b=" $2}'
else
  echo "[U] vmstat not available -> skip"
fi

echo
echo "--- Memory ---"
if [ -r /proc/meminfo ]; then
  awk '/^(MemTotal|MemAvailable|SwapTotal|SwapFree):/ {printf "    %s %s kB\n", $1, $2}' /proc/meminfo
  echo "[U] 内存利用率 = (MemTotal - MemAvailable) / MemTotal"
  awk '/^MemTotal:/ {t=$2} /^MemAvailable:/ {a=$2} END {if (t>0) printf "    mem_used_pct=%.1f%%\n", (t-a)*100.0/t}' /proc/meminfo
else
  echo "SKIP: /proc/meminfo not readable"
fi
if [ -r /proc/vmstat ]; then
  echo "[S] 换页与页面扫描 (si/so/pgscank 持续非零 => 饱和)"
  awk '/^(pswpin|pswpout|pgscank|pgscand) / {printf "    %s %s\n", $1, $2}' /proc/vmstat
fi

echo
echo "--- Network ---"
if [ -r /proc/net/dev ]; then
  echo "[U] 收发字节数"
  awk 'NR>2 {rx+=$2; tx+=$10} END {printf "    rx_bytes=%d tx_bytes=%d\n", rx, tx}' /proc/net/dev
  echo "[S/E] 丢包与错误"
  awk 'NR>2 {d+=$4; e+=$12} END {printf "    rx_dropped=%d rx_errors=%d\n", d, e}' /proc/net/dev
else
  echo "SKIP: /proc/net/dev not readable"
fi

echo
echo "--- Storage ---"
if [ -r /proc/diskstats ]; then
  echo "[U] 扇区读写数"
  awk '{r+=$6; w+=$10} END {printf "    read_sectors=%d write_sectors=%d\n", r, w}' /proc/diskstats
  echo "[S] io_ticks (设备忙的时间占比)"
  awk '{t+=$13} END {printf "    io_ticks_total=%d\n", t}' /proc/diskstats
else
  echo "SKIP: /proc/diskstats not readable"
fi

echo
echo "note: load average is NOT a CPU metric; it counts tasks in uninterruptible I/O wait."
exit 0
