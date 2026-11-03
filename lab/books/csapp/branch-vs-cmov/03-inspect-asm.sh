# @book: 深入理解计算机系统
# @topic: branch-vs-cmov
# @source: 第3章 程序的机器级表示（讲义）.md §3.6.5 §3.6.6
# @expect: 打印两个文件在 -O2 下生成的汇编, 前者是条件跳转(jge), 后者是条件传送(cmov)
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

CC="${CC:-gcc}"
TMP="$(mktemp -d)" || exit 1
trap 'rm -rf "$TMP"' EXIT

printf '%-30s %-6s %-10s %s\n' "文件" "cmov" "条件跳转" "出现的关键指令"
printf -- '----------------------------------------------------------------------\n'

for f in 01-absdiff-with-counters.c 02-absdiff-plain.c; do
    if ! "$CC" -std=c11 -O2 -S -o "$TMP/out.s" "$f" 2>"$TMP/err.txt"; then
        echo "编译失败: $f"
        head -5 "$TMP/err.txt"
        exit 1
    fi
    cmov=$(grep -c 'cmov' "$TMP/out.s" || true)
    jump=$(grep -cE '^[[:space:]]+j[a-z]+[[:space:]]' "$TMP/out.s" || true)
    key=$(grep -oE '\b(cmov[a-z]+|j[a-z]+)\b' "$TMP/out.s" | sort -u | tr '\n' ' ')
    printf '%-30s %-6s %-10s %s\n' "$f" "$cmov" "$jump" "$key"
done

echo
echo "把两个文件的关键片段贴出来对比:"
for f in 01-absdiff-with-counters.c 02-absdiff-plain.c; do
    echo "--- $f ---"
    "$CC" -std=c11 -O2 -S -o "$TMP/out.s" "$f" 2>/dev/null
    grep -E '^[[:space:]]+(cmov|j[a-z]+|cmp|ret)' "$TMP/out.s" | head -6
done
