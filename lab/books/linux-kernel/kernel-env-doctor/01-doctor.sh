#!/usr/bin/env bash
# @book: Linux Kernel Programming
# @topic: kernel-env-doctor
# @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.7（另见讲义 1.2 / 2.1 / 2.2 节）
# @expect: 逐项打印 7 项前置条件的体检结论, 缺项只报不失败, 退出码恒为 0

set -uo pipefail

pass=0
miss=0

ok()   { pass=$((pass + 1)); printf '[ OK ] %-16s %s\n' "$1" "$2"; }
nope() { miss=$((miss + 1)); printf '[MISS] %-16s %s\n' "$1" "$2"; }

rel="$(uname -r 2>/dev/null || echo unknown)"
ok "内核版本" "$rel"

build_link="/lib/modules/$rel/build"
if [ -d "$build_link" ]; then
  ok "内核头文件" "$build_link"
elif [ -L "$build_link" ]; then
  nope "内核头文件" "$build_link 是悬空软链接(指向 $(readlink -f "$build_link" 2>/dev/null), 目标不存在)"
else
  nope "内核头文件" "$build_link 不存在 (装 kernel-devel 才能编译模块)"
fi

if [ -r /proc/version ]; then
  ok "proc/version" "$(head -1 /proc/version)"
else
  nope "proc/version" "读不到 /proc/version"
fi

if grep -qw 'vmx' /proc/cpuinfo 2>/dev/null; then
  ok "虚拟化扩展" "检测到 vmx"
elif grep -qw 'svm' /proc/cpuinfo 2>/dev/null; then
  ok "虚拟化扩展" "检测到 svm"
else
  nope "虚拟化扩展" "没有 vmx/svm (讲义: 装 64 位客机要在 BIOS 里打开虚拟化)"
fi

if command -v gcc >/dev/null 2>&1; then
  ok "编译器" "$(gcc --version 2>/dev/null | head -1)"
else
  nope "编译器" "gcc 不在 PATH"
fi

missing_tools=""
for t in make bc bison flex; do
  command -v "$t" >/dev/null 2>&1 || missing_tools="$missing_tools $t"
done
if [ -z "$missing_tools" ]; then
  ok "编译内核工具" "make/bc/bison/flex 齐了"
else
  nope "编译内核工具" "缺:$missing_tools"
fi

mem_kb="$(awk '/^MemTotal:/ {print $2; exit}' /proc/meminfo 2>/dev/null)"
if [ -n "${mem_kb:-}" ]; then
  ok "内存" "$((mem_kb / 1024)) MB 总内存"
else
  nope "内存" "读不到 /proc/meminfo"
fi

avail_kb="$(df -Pk / 2>/dev/null | awk 'NR==2 {print $4}')"
if [ -n "${avail_kb:-}" ]; then
  ok "磁盘" "$((avail_kb / 1024)) MB 可用于 /"
else
  nope "磁盘" "df 不可用"
fi

printf '%s\n' '----------------------------------------'
printf '体检结论: %d 项通过, %d 项缺失\n' "$pass" "$miss"
if [ "$miss" -eq 0 ]; then
  printf '%s\n' '结论    : 本机具备编译内核/内核模块的基本条件'
else
  printf '结论    : 有 %d 项前置条件缺失, 见上面 MISS 行\n' "$miss"
fi
printf '%s\n' 'note     : 报告你观察到的这个具体环境, 而不是理想化的通用结论'
exit 0
