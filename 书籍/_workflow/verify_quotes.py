#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""复核「讲义/精简」里的英文引文是否真的逐字出现在原文稿里。

判据（比 agent 自称的 grep 更严）：
  1. 整句命中 —— 引文去掉空白后必须**连续**出现在原文稿归一化文本里；
  2. 分段省略 —— 引文里有 `...` 时，**每一段**都必须单独命中；
  3. 首段命中 + 尾部有出入 —— 单独标出来人工看（可能是尾部省略号）。
"""
import re, os, glob, sys
sys.stdout.reconfigure(encoding="utf-8")   # Windows stdout 默认 GBK，会让中文输出变乱码

# ⚠️ 2026-11-03：路径从 __file__ 反推，不再写死 /mnt/kb ——
#    这台机器是 Windows，vault 在 iCloudDrive 下；写死 Linux 路径会让脚本静默扫不到任何笔记。
from pathlib import Path
VAULT = Path(__file__).resolve().parents[3]   # <vault>/markdown/书籍/_workflow/verify_quotes.py
BOOKS_DIR = VAULT / "markdown" / "书籍"
LECT = str(BOOKS_DIR / "教材讲义")
DIS = str(BOOKS_DIR / "教材精简")
RAW = str(BOOKS_DIR / "教材原文")
BOOKS = ["Effective Modern C++", "C++ Concurrency in Action", "Systems Performance",
         "Linux Kernel Programming"]

def norm(s):
    for a, b in [("’","'"),("‘","'"),("“",'"'),("”",'"'),
                 ("—","-"),("–","-"),("…","..."),(" "," ")]:
        s = s.replace(a, b)
    s = s.replace("`", "")      # 行内代码的反引号属排版，不是文字
    return re.sub(r"\s+", "", s).lower()

cache = {}
def rawtext(book):
    if book not in cache:
        cache[book] = norm("\n".join(
            open(f, encoding="utf-8", errors="replace").read()
            for f in glob.glob(os.path.join(RAW, book, "*.md"))))
    return cache[book]

def quotes_of(path, layer):
    t = open(path, encoding="utf-8").read()
    out = []
    if layer == "讲义":
        i = t.find("收尾：本章金句清单")
        if i < 0: return out
        seg = t[i:]
        out = [("金句", q) for q in re.findall(r"^\s*\d+\.\s+\*\*(.+?)\*\*", seg, re.M)]
    else:
        # 精简层：锚点表第三列（逐字摘录）
        i = t.find("原文锚点")
        if i < 0: return out
        seg = t[i:]
        j = seg.find("\n## ", 5)
        seg = seg[:j] if j > 0 else seg
        for line in seg.split("\n"):
            m = re.match(r"^\s*\|[^|]*\|[^|]*L(\d+)(?:[-–]\d+)?\s*\|(.*)$", line)
            if m:
                q = m.group(2).strip().strip("|").strip()
                if q: out.append(("锚点", q))
    return out

tot = 0; exact = 0; seg_ok = 0; suspicious = []; wrong = []
for book in BOOKS:
    T = rawtext(book)
    for layer, root in (("讲义", LECT), ("精简", DIS)):
        d = os.path.join(root, book)
        if not os.path.isdir(d): continue
        for f in sorted(glob.glob(os.path.join(d, "*.md"))):
            for kind, q in quotes_of(f, layer):
                tot += 1
                qn = norm(q).strip("\"' ")
                # 尾部省略号是「引文被截短」的合法记号，核前缀即可
                if qn.endswith("...") and qn[:-3] in T:
                    exact += 1; continue
                if qn in T:
                    exact += 1; continue
                parts = [x for x in re.split(r"\.\.\.+", qn) if len(x) >= 12]
                if parts and all(x in T for x in parts):
                    seg_ok += 1; continue
                if len(qn) >= 40 and qn[:40] in T:
                    suspicious.append((os.path.basename(f), q)); continue
                wrong.append((os.path.basename(f), q))

print("=== 引文复核：%d 条 ===" % tot)
print("逐字整句命中      : %d" % exact)
print("分段省略（每段中）: %d" % seg_ok)
print("首段命中/尾部有出入: %d  ← 人工看" % len(suspicious))
print("对不上            : %d" % len(wrong))
for n, q in suspicious:
    print("  ≈ %-46s | %s" % (n[:46], q[:88]))
for n, q in wrong:
    print("  ❌ %-46s | %s" % (n[:46], q[:88]))
sys.exit(1 if wrong else 0)
