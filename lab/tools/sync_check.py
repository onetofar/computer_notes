#!/usr/bin/env python3
"""写后校验: 确认 manifest 里列出的每个文件都在盘上, 且行数/字节数一致。

背景: 曾出现过「写入工具返回成功, 但同级子目录整块消失」的情况。
这个脚本把「以为写进去了」变成「验证写进去了」, 作为批量产出的收尾闸门。

用法:
  python3 tools/sync_check.py manifest.txt
  manifest 每行: <相对路径>\t<期望字节数>\t<期望行数>
"""
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent.parent


def main():
    if len(sys.argv) < 2:
        print("usage: sync_check.py manifest.txt")
        return 64
    mf = Path(sys.argv[1])
    if not mf.is_absolute():
        mf = ROOT / mf
    if not mf.is_file():
        print(f"manifest 不存在: {mf}")
        return 66

    bad = []
    total = 0
    for lineno, raw in enumerate(mf.read_text(encoding="utf-8").splitlines(), 1):
        raw = raw.strip()
        if not raw or raw.startswith("#"):
            continue
        parts = raw.split("\t")
        if len(parts) != 3:
            bad.append(f"L{lineno} 格式错: {raw}")
            continue
        rel, want_b, want_l = parts[0], int(parts[1]), int(parts[2])
        p = ROOT / rel
        total += 1
        if not p.is_file():
            bad.append(f"缺失: {rel}")
            continue
        data = p.read_bytes()
        if len(data) != want_b:
            bad.append(f"字节数不符: {rel} 期望{want_b} 实际{len(data)}")
            continue
        n = len(data.decode("utf-8", errors="replace").splitlines())
        if n != want_l:
            bad.append(f"行数不符: {rel} 期望{want_l} 实际{n}")

    print(f"校验 {total} 个文件")
    if bad:
        print(f"--- 不一致 {len(bad)} 项 ---")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: 全部文件在盘且一致")
    return 0


if __name__ == "__main__":
    sys.exit(main())
