#!/usr/bin/env bash
# 把 lab/ 同步到服务器 /home/xier/dev/kb-lab (与游戏服务器 otf-server 完全分开)。
# 用法: tools/deploy.sh [host]   默认 host=fedora
set -euo pipefail
HOST="${1:-fedora}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="/home/xier/dev/kb-lab"

echo "源: $ROOT"
echo "目标: $HOST:$DEST"

ssh -o BatchMode=yes "$HOST" "mkdir -p $DEST"
rsync -a --delete --exclude build --exclude reports --exclude ".git" "$ROOT/" "$HOST:$DEST/"
echo "同步完成"
ssh -o BatchMode=yes "$HOST" "ls -1 $DEST"
