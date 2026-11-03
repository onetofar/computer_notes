#!/usr/bin/env bash
# @book: Systems Performance
# @topic: syscall-cost-and-tracing
# @source: Chapter 13. perf（讲义）.md §13.8
# @expect: perf stat 打印出软件事件计数; 若 perf_event_paranoid 限制导致不可用, 打印说明并以 0 退出

set -uo pipefail

if ! command -v perf >/dev/null 2>&1; then
  echo "SKIP: perf(1) not installed on this host"
  exit 0
fi

paranoid="$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo unknown)"
echo "perf_event_paranoid : $paranoid"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

cat >"$WORKDIR/work.c" <<'EOF'
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
int main(void)
{
    for (int i = 0; i < 20000; i++)
        getpid();
    return 0;
}
EOF

if ! command -v gcc >/dev/null 2>&1; then
  echo "SKIP: gcc not available, cannot build the workload"
  exit 0
fi
if ! gcc -std=c11 -O2 -Wall -Wextra -o "$WORKDIR/work" "$WORKDIR/work.c" 2>"$WORKDIR/cc.log"; then
  echo "SKIP: workload failed to build"
  head -5 "$WORKDIR/cc.log"
  exit 0
fi

if ! perf stat -e task-clock,context-switches,page-faults -- "$WORKDIR/work" >"$WORKDIR/out" 2>"$WORKDIR/stat.txt"; then
  echo "SKIP: perf stat refused to run under the current perf_event_paranoid setting"
  echo "      (raise it with: sysctl -w kernel.perf_event_paranoid=N, needs root)"
  head -5 "$WORKDIR/stat.txt"
  exit 0
fi

echo "--- perf stat output ---"
cat "$WORKDIR/stat.txt"
echo "--- events of interest ---"
grep -E "task-clock|context-switches|page-faults" "$WORKDIR/stat.txt" || echo "(no matching rows)"
echo "note: perf stat is the cheap step; check the event rate before paying for perf record."
exit 0
