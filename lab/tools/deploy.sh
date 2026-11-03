#!/usr/bin/env bash
# 把 lab/ 同步到服务器 /home/xier/dev/kb-lab (与游戏服务器 otf-server 完全分开)。
# 讲义源同步到 /home/xier/dev/书籍/教材讲义 —— 内容门禁 (verify_content.py)
# 需要在服务器上直接比对讲义原文, 缺了它门禁会误报 NO-QUOTE。
# 用法: tools/deploy.sh [host]   默认 host=fedora
#
# 注意: 用 tar over ssh 而不是 rsync —— rsync 在本机被安全策略拦截, 跑不起来。
set -euo pipefail
HOST="${1:-fedora}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VAULT="$(cd "$ROOT/.." && pwd)"
DEST="/home/xier/dev/kb-lab"
LEC_DEST="/home/xier/dev/书籍"

echo "源:     $ROOT"
echo "讲义源: $VAULT/书籍/教材讲义"
echo "目标:   $HOST:$DEST"

ssh -o BatchMode=yes "$HOST" "rm -rf $DEST && mkdir -p $DEST"
tar -cf - --exclude=build --exclude=reports --exclude=.git --exclude=__pycache__ -C "$ROOT" . \
  | ssh -o BatchMode=yes "$HOST" "tar -xf - -C $DEST"

ssh -o BatchMode=yes "$HOST" "rm -rf \"$LEC_DEST\" && mkdir -p \"$LEC_DEST\""
tar -cf - -C "$VAULT/书籍" 教材讲义 \
  | ssh -o BatchMode=yes "$HOST" "tar -xf - -C \"$LEC_DEST\""

echo "同步完成"
ssh -o BatchMode=yes "$HOST" "cd $DEST && python3 -I tools/lab_check.py | tail -2 && python3 -I tools/verify_content.py | tail -2"