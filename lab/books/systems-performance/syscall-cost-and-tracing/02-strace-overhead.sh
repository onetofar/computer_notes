#!/usr/bin/env bash
# @book: Systems Performance
# @topic: syscall-cost-and-tracing
# @source: Chapter 4. Observability Tools（讲义）.md §4.2.3
# @expect: 被 strace 跟踪的程序自身测得的耗时明显变长; 差值除以被追踪的系统调用次数, 就是每次 syscall 的观测开销

set -uo pipefail

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

if ! command -v gcc >/dev/null 2>&1; then
  echo "SKIP: gcc not available, cannot build the probe workload"
  exit 0
fi
if ! command -v strace >/dev/null 2>&1; then
  echo "SKIP: strace not available on this host"
  exit 0
fi

cat >"$WORKDIR/probe.c" <<'EOF'
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static long long now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000000LL + (long long)t.tv_nsec;
}

int main(void)
{
    long long t0 = now_ns();
    for (int i = 0; i < 2000; i++)
        getpid();
    long long t1 = now_ns();
    printf("%lld\n", t1 - t0);
    return 0;
}
EOF

if ! gcc -std=c11 -O2 -Wall -Wextra -o "$WORKDIR/probe" "$WORKDIR/probe.c" 2>"$WORKDIR/cc.log"; then
  echo "SKIP: probe failed to build"
  head -5 "$WORKDIR/cc.log"
  exit 0
fi

plain_ns="$("$WORKDIR/probe")"

strace -c -f -o "$WORKDIR/strace.out" "$WORKDIR/probe" >"$WORKDIR/traced.out" 2>/dev/null
traced_ns="$(tail -1 "$WORKDIR/traced.out" 2>/dev/null)"

calls=0
if [ -s "$WORKDIR/strace.out" ]; then
  calls="$(awk '!/^%/ && !/^-+/ && $NF != "total" {s += $4} END {print s + 0}' "$WORKDIR/strace.out")"
fi

echo "plain_internal_ns  : $plain_ns"
echo "traced_internal_ns : $traced_ns"
if [ -n "$plain_ns" ] && [ "$plain_ns" -gt 0 ] && [ -n "$traced_ns" ]; then
  echo "slowdown           : $(awk -v a="$traced_ns" -v b="$plain_ns" 'BEGIN{printf "%.1f", a/b}')x"
fi
echo "syscalls_traced    : $calls"
if [ "$calls" -gt 0 ] && [ -n "$traced_ns" ] && [ -n "$plain_ns" ]; then
  echo "observed_overhead_ns_per_syscall: $(awk -v t="$traced_ns" -v p="$plain_ns" -v c="$calls" 'BEGIN{printf "%.1f", (t-p)/c}')"
fi
echo "note: tracing perturbs the target; numbers taken under strace are not a baseline."
exit 0
