#!/usr/bin/env bash
# kb-lab 完整门禁: 结构守卫 + 内容守卫 + 语法检查。
# 任一环失败 -> 退出码非 0。
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

FAIL=0

echo "=== 1/3 结构守卫 (目录 / 元信息 / 颗粒度) ==="
python3 tools/lab_check.py || FAIL=1

echo "=== 2/3 内容守卫 (引文逐字 / @source 可解析) ==="
python3 tools/verify_content.py || FAIL=1

echo "=== 3/3 语法检查 ==="
bash tools/build_all.sh || FAIL=1

echo "----------------------------------------------"
if [ "$FAIL" -eq 0 ]; then echo "GATE: PASS"; else echo "GATE: FAIL"; fi
exit $FAIL
