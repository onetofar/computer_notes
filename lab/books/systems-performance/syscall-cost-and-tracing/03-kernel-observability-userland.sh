#!/usr/bin/env bash
# @book: Systems Performance
# @topic: syscall-cost-and-tracing
# @source: Chapter 15. BPF（讲义）.md §15.1
# @expect: 打印 tracefs / bpftrace / perf 的可用性; 用户态只能从 /proc 看到累计计数, 看不到逐次事件

set -uo pipefail

echo "=== 内核态观测能力可用性 ==="
for t in perf bpftrace bcc; do
  if command -v "$t" >/dev/null 2>&1; then
    echo "$t: available ($(command -v "$t"))"
  else
    echo "$t: NOT available"
  fi
done

for d in /sys/kernel/debug/tracing /sys/kernel/tracing; do
  if [ -d "$d" ]; then
    if [ -w "$d" ]; then
      echo "tracefs $d: mounted and writable"
    else
      echo "tracefs $d: mounted, NOT writable (ftrace needs root)"
    fi
  else
    echo "tracefs $d: not mounted"
  fi
done

if [ -r /proc/sys/kernel/perf_event_paranoid ]; then
  echo "perf_event_paranoid: $(cat /proc/sys/kernel/perf_event_paranoid)"
fi

echo
echo "=== 用户态能拿到的近似证据 ==="
if [ -r /proc/self/stat ]; then
  sed 's/^.*) //' /proc/self/stat | awk '{
      printf "minflt=%s majflt=%s utime_ticks=%s stime_ticks=%s\n", $8, $10, $12, $13
  }'
fi
if [ -r /proc/self/status ]; then
  grep -E "^(voluntary_ctxt_switches|nonvoluntary_ctxt_switches)" /proc/self/status 2>/dev/null || true
fi

echo
echo "note: /proc exposes cumulative counters only."
echo "      Per-event detail needs kernel-state tracing (ftrace/BPF), which needs root."
exit 0
