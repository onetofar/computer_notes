#!/usr/bin/env bash
# @book: Linux Kernel Programming
# @topic: kernel-env-doctor
# @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.7
# @expect: 逐条判定 eBPF 的四个前置条件, 额外报告 BCC 依赖的 linux-headers 是否在位, 退出码恒为 0

set -uo pipefail

pass=0
miss=0

ok()   { pass=$((pass + 1)); printf '[ OK ] %-22s %s\n' "$1" "$2"; }
nope() { miss=$((miss + 1)); printf '[MISS] %-22s %s\n' "$1" "$2"; }

rel="$(uname -r 2>/dev/null || echo 0.0.0)"
major="${rel%%.*}"
case "$major" in ''|*[!0-9]*) major=0 ;; esac

printf '%s\n' '前置条件 1/4: 内核 >= 4.0'
if [ "$major" -ge 4 ]; then
  ok "内核 >= 4.0" "$rel"
else
  nope "内核 >= 4.0" "$rel"
fi

printf '%s\n' '前置条件 2/4: 内核开了 BPF'
signals=""
if [ -r /proc/sys/kernel/unprivileged_bpf_disabled ]; then
  signals="$signals unprivileged_bpf_disabled"
fi
if [ -d /sys/fs/bpf ]; then
  signals="$signals bpf-fs"
fi
if [ -r "/boot/config-$rel" ] && grep -q '^CONFIG_BPF=y' "/boot/config-$rel" 2>/dev/null; then
  signals="$signals CONFIG_BPF=y"
fi
if [ -n "$signals" ]; then
  ok "内核 BPF 支持" "命中:$signals"
else
  nope "内核 BPF 支持" "没找到 CONFIG_BPF / bpf-fs / bpf sysctl 的任何信号"
fi

printf '%s\n' '前置条件 3/4: 装了 BCC 或 bpftrace'
front=""
command -v bpftrace >/dev/null 2>&1 && front="$front bpftrace"
command -v bcc >/dev/null 2>&1 && front="$front bcc"
command -v bpftool >/dev/null 2>&1 && front="$front bpftool"
if [ -n "$front" ]; then
  ok "eBPF 前端" "$front"
else
  nope "eBPF 前端" "bpftrace / bcc / bpftool 都不在 PATH"
fi

printf '%s\n' '前置条件 4/4: root 权限'
uid="$(id -u 2>/dev/null || echo 1000)"
if [ "$uid" -eq 0 ]; then
  ok "root 权限" "uid=0"
else
  nope "root 权限" "uid=$uid (eBPF 加载需要 root)"
fi

printf '%s\n' '附加: BCC 依赖的 linux-headers-$(uname -r)'
if [ -d "/lib/modules/$rel/build" ] || [ -d "/usr/src/linux-headers-$rel" ]; then
  ok "linux-headers" "有发行版内核头文件, BCC 装得上"
else
  nope "linux-headers" "没有发行版头文件, 自编译内核上 BCC 装不上"
fi

printf '%s\n' '----------------------------------------'
printf '体检结论: %d 项通过, %d 项缺失\n' "$pass" "$miss"
printf '%s\n' 'note     : 讲义指出的工具链假设: 自编译内核上装 BCC 会卡在 linux-headers 包上'
exit 0
