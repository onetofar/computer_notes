#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""教材引文/锚点取行工具 —— 把「手抄引文」这个错误源从流程里拿掉。

用法（路径请用 Windows 绝对路径，路径含中文时不要用 python -c，直接跑本脚本）：
  python3 quote.py heads "<原文稿绝对路径>"
      列出真实标题层级 + 行号（原文里没有的编号不许造 —— 先跑这个）

  python3 quote.py find "<原文稿绝对路径>" "<正则>"
      列出匹配行（行号 + 全文，超长截断）

  python3 quote.py grep "<原文稿绝对路径>" "<字面串>"
      字面查找（相当于 grep -F），返回行号 + 整行

  python3 quote.py rows "<原文稿绝对路径>" "<本篇小节>" <spec> [<spec> ...]
      spec 语法：
        N        第 N 行整行
        N-M      第 N..M 行（多行合并成一段）
        N:a-b    第 N 行的第 a..b 个字符（0 基、左闭右开）—— 用来切掉句中的脚注号
      直接打印可粘贴的 markdown 表格行：
        | <本篇小节> | <原文稿文件名>.md L<N> | <逐字引文> |
      引文含竖线或以省略号结尾时会告警（这两样会破表 / 破校验）。

约定：行号一律从磁盘取（python），**不要信 Read 工具显示的行号**。
"""
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")   # Windows stdout 默认 GBK，会让中文输出变乱码

sys.stdout.reconfigure(encoding="utf-8")

def load(path):
    return Path(path).read_text(encoding="utf-8", errors="replace").split("\n")

def cmd_heads(args):
    lines = load(args[0])
    for i, ln in enumerate(lines, 1):
        if re.match(r"^#{1,6}\s", ln):
            print("%5d: %s" % (i, ln.rstrip()))
    return 0

def cmd_find(args):
    lines = load(args[0])
    pat = re.compile(args[1])
    for i, ln in enumerate(lines, 1):
        if pat.search(ln):
            s = ln.rstrip()
            if len(s) > 300:
                s = s[:300] + "  ……(截断)"
            print("%5d| %s" % (i, s))
    return 0

def cmd_grep(args):
    lines = load(args[0])
    needle = args[1]
    hit = 0
    for i, ln in enumerate(lines, 1):
        if needle in ln:
            hit += 1
            print("%5d| %s" % (i, ln.rstrip()))
    if not hit:
        print("!! 字面串未命中：%r" % needle)
    return 0 if hit else 1

def cmd_rows(args):
    path, section = args[0], args[1]
    specs = args[2:]
    lines = load(path)
    fname = Path(path).name
    for spec in specs:
        frag = None
        if ":" in spec:
            spec, rng = spec.split(":", 1)
            a, b = rng.split("-", 1)
            frag = (int(a), int(b))
        if "-" in spec:
            a, b = spec.split("-", 1)
            a, b = int(a), int(b)
            seg = "\n".join(lines[a - 1:b])
            head = "L%d-%d" % (a, b)
        else:
            n = int(spec)
            if not (1 <= n <= len(lines)):
                print("!! 行号越界：%d（全文 %d 行）" % (n, len(lines)))
                return 2
            seg = lines[n - 1]
            if frag:
                seg = seg[frag[0]:frag[1]]
            head = "L%d" % n
        seg = seg.rstrip()
        warn = []
        if "|" in seg:
            warn.append("含竖线（会破表）")
        if seg.endswith("…"):
            warn.append("以省略号结尾")
        if frag and seg != seg.strip():
            warn.append("首尾有空白")
        print("| %s | %s %s | %s |" % (section, fname, head, seg))
        for w in warn:
            print("   !! %s —— 换一行或切片" % w)
    return 0

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    cmd, args = sys.argv[1], sys.argv[2:]
    fn = {"heads": cmd_heads, "find": cmd_find, "grep": cmd_grep, "rows": cmd_rows}.get(cmd)
    if not fn:
        print("未知命令：%s" % cmd)
        return 1
    return fn(args)

if __name__ == "__main__":
    sys.exit(main())
