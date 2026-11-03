#!/usr/bin/env bash
# 编译并运行单个测试文件。
# 用法: tools/check_one.sh <src> [--ub]
#   --ub  该文件故意演示未定义行为: 用 valgrind 检出 (免特权); 期望「检测到 UB」才算通过。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${1:-}"
MODE="${2:-}"

if [ -z "$SRC" ]; then echo "usage: check_one.sh <src> [--ub]" >&2; exit 64; fi
case "$SRC" in /*) ;; *) SRC="$ROOT/$SRC" ;; esac
if [ ! -f "$SRC" ]; then echo "MISSING $SRC"; exit 66; fi

REL="${SRC#$ROOT/}"
SAFE="$(printf "%s" "$REL" | tr "/ " "__")"
EXT="${SRC##*.}"
BUILD="${KB_LAB_BUILD:-/tmp/kb-lab-build}"
OUT="$BUILD/${SAFE%.*}.bin"
mkdir -p "$BUILD"

CXXFLAGS="-std=c++20 -O1 -g -Wall -Wextra -pthread"
CCFLAGS="-std=c11 -O1 -g -Wall -Wextra"
ASAN=0
if [ -n "${KB_LAB_ASAN:-}" ]; then
  CXXFLAGS="$CXXFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer"
  CCFLAGS="$CCFLAGS -fsanitize=address,undefined -fno-omit-frame-pointer"
  ASAN=1
fi

case "$EXT" in
  cpp) if ! g++ $CXXFLAGS "$SRC" -o "$OUT" 2>"$BUILD/cc.err"; then echo "BUILD-FAIL $REL"; head -20 "$BUILD/cc.err"; exit 2; fi ;;
  c)   if ! gcc $CCFLAGS "$SRC" -o "$OUT" 2>"$BUILD/cc.err"; then echo "BUILD-FAIL $REL"; head -20 "$BUILD/cc.err"; exit 2; fi ;;
  sh)  : ;;
  py)  : ;;
  *)   echo "UNSUPPORTED-EXT $EXT ($REL)"; exit 65 ;;
esac

TMPOUT="$(mktemp)"
if [ "$EXT" = "cpp" ] || [ "$EXT" = "c" ]; then
  if [ "$MODE" = "--ub" ] && [ "$ASAN" -eq 0 ] && command -v valgrind >/dev/null 2>&1; then
    timeout 60 valgrind --error-exitcode=99 --errors-for-leak-kinds=definite --leak-check=full \
             --quiet "$OUT" >"$TMPOUT" 2>&1; RC=$?
  elif [ "$MODE" = "--ub" ]; then
    timeout 20 "$OUT" >"$TMPOUT" 2>&1; RC=$?
  else
    timeout 20 "$OUT" >"$TMPOUT" 2>&1; RC=$?
  fi
elif [ "$EXT" = "sh" ]; then
  timeout 20 bash "$SRC" >"$TMPOUT" 2>&1; RC=$?
else
  timeout 20 python3 "$SRC" >"$TMPOUT" 2>&1; RC=$?
fi
OUTTXT="$(cat "$TMPOUT")"

# valgrind / ASan / UBSan 的各种报错措辞都要能认出来
UB_PATTERN="Invalid free|Invalid read|Invalid write|Invalid pointer|double free|AddressSanitizer|runtime error:|LeakSanitizer|definitely lost|Conditional jump or move depends on uninitialised value|Use of uninitialised value|ERROR SUMMARY: [1-9]"

if [ "$MODE" = "--ub" ]; then
  if [ "$RC" -ne 0 ] && printf "%s" "$OUTTXT" | grep -Eq "$UB_PATTERN"; then
    echo "PASS(ub-detected) $REL"
    printf "%s\n" "$OUTTXT" | grep -E "$UB_PATTERN" | head -6
    rm -f "$TMPOUT"; exit 0
  fi
  echo "FAIL(no-ub-triggered) $REL rc=$RC"
  printf "%s\n" "$OUTTXT" | head -20
  rm -f "$TMPOUT"; exit 1
fi

if [ "$RC" -eq 0 ]; then
  echo "PASS $REL"
else
  echo "FAIL $REL rc=$RC"
  printf "%s\n" "$OUTTXT" | tail -20
fi
rm -f "$TMPOUT"
exit $RC
