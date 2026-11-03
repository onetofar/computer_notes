#!/usr/bin/env bash
# @book: Systems Performance
# @topic: use-method-and-sar
# @source: Chapter 4. Observability Tools（讲义）.md §4.6 Observing Observability：观测的观测
# @expect: perf 报的上下文切换/缺页数与 /proc/self/stat 读到的累计计数同量级 —— 两个工具可以互相印证

set -uo pipefail

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

if ! command -v gcc >/dev/null 2>&1; then
  echo "SKIP: gcc not available"
  exit 0
fi

cat >"$WORKDIR/probe.c" <<'EOF'
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

if ! gcc -std=c11 -O2 -Wall -Wextra -o "$WORKDIR/probe" "$WORKDIR/probe.c" 2>/dev/null; then
  echo "SKIP: probe failed to build"
  exit 0
fi

echo "=== /proc/self/stat (用户态可见的累计计数) ==="
if [ -r /proc/self/stat ]; then
  sed 's/^.*) //' /proc/self/stat | awk '{
      printf "minflt=%s majflt=%s utime_ticks=%s stime_ticks=%s\n", $8, $10, $12, $13
  }'
fi

echo
echo "=== perf stat (内核计数器) ==="
if ! command -v perf >/dev/null 2>&1; then
  echo "SKIP: perf(1) not installed on this host"
  exit 0
fi
if ! perf stat -e task-clock,context-switches,page-faults -- "$WORKDIR/probe" 2>&1 >/dev/null | tee "$WORKDIR/stat.txt" | grep -q "task-clock"; then
  echo "SKIP: perf stat refused to run under the current perf_event_paranoid setting"
  echo "      (raise it with: sysctl -w kernel.perf_event_paranoid=N, needs root)"
  exit 0
fi

echo
echo "note: a man with one tracer knows what events happened;"
echo "      two tracers with overlapping coverage can cross-check each other."
exit 0
