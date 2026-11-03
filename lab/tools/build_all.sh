#!/usr/bin/env bash
# 只做语法检查, 不生成可执行文件。用于快速发现语法错误。
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1
BUILD="${KB_LAB_BUILD:-/tmp/kb-lab-build}"
mkdir -p "$BUILD"
ERR="$BUILD/syntax-errors.txt"
: > "$ERR"
OK=0; BAD=0
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  case "$f" in
    *.cpp) if g++ -std=c++20 -O2 -Wall -Wextra -pthread -fsyntax-only "$f" 2>>"$ERR"; then OK=$((OK+1)); else echo "SYNTAX-FAIL $rel"; BAD=$((BAD+1)); fi ;;
    *.c)   if gcc -std=c11 -O2 -Wall -Wextra -fsyntax-only "$f" 2>>"$ERR"; then OK=$((OK+1)); else echo "SYNTAX-FAIL $rel"; BAD=$((BAD+1)); fi ;;
    *) OK=$((OK+1)) ;;
  esac
done < <(find books -type f \( -name "*.cpp" -o -name "*.c" \) | sort)
echo "SYNTAX-OK=$OK SYNTAX-FAIL=$BAD  错误详情: $ERR"
[ "$BAD" -eq 0 ]
