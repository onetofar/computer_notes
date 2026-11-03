#!/usr/bin/env bash
# 编译并运行 books/ 下全部测试文件, 输出 PASS/FAIL 汇总。
# 用法: tools/run_all.sh [--ub]   --ub 额外跑「故意演示未定义行为」的那批。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1
WITH_UB=0
if [ "${1:-}" = "--ub" ]; then WITH_UB=1; fi

BUILD="${KB_LAB_BUILD:-/tmp/kb-lab-build}"
REPORT_DIR="${KB_LAB_REPORT:-/tmp/kb-lab-reports}"
mkdir -p "$BUILD" "$REPORT_DIR"
REPORT="$REPORT_DIR/last-run.txt"
: > "$REPORT"

PASS=0; FAIL=0; SKIP=0

while IFS= read -r f; do
  rel="${f#$ROOT/}"
  ub=0
  if head -30 "$f" | grep -Eq "^[[:space:]]*(//|#)[[:space:]]*@ub:[[:space:]]*yes"; then ub=1; fi
  if [ "$ub" -eq 1 ] && [ "$WITH_UB" -eq 0 ]; then
    echo "SKIP(ub) $rel" >> "$REPORT"; SKIP=$((SKIP+1)); continue
  fi
  if [ "$ub" -eq 1 ]; then
    out="$(bash tools/check_one.sh "$rel" --ub 2>&1)"
  else
    out="$(bash tools/check_one.sh "$rel" 2>&1)"
  fi
  printf "%s\n" "$out" >> "$REPORT"
  if printf "%s" "$out" | head -1 | grep -q "^PASS"; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
done < <(find books -type f \( -name "*.cpp" -o -name "*.c" -o -name "*.sh" -o -name "*.py" \) | sort)

echo "----------------------------------------------"
echo "PASS=$PASS  FAIL=$FAIL  SKIP(ub)=$SKIP"
echo "报告: $REPORT"
if [ "$FAIL" -gt 0 ]; then echo "---- 失败明细 ----"; grep -A20 "^FAIL" "$REPORT" | head -80; fi
[ "$FAIL" -eq 0 ]
