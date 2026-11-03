#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三本新书（EMC++ / CiA / SP）的总收尾体检。

跑法：cd /tmp/ostep && python3 -I final_check.py

依次做五件事：
  1. §-定位器核对（全书词表 + 区分标题/正文）
  2. 引文逐字复核
  3. 锚点校验（精简层）
  4. 全库体检（断链 / 孤儿 / 图片嵌入）
  5. 缺口清单（讲义 / 精简各还差哪些）
"""
import io, os, re, sys, glob, subprocess
sys.stdout.reconfigure(encoding="utf-8")   # Windows stdout 默认 GBK，会让中文输出变乱码

# ⚠️ 2026-11-03：路径从 __file__ 反推，不再写死 /mnt/kb（Windows 侧跑不了）。
from pathlib import Path
VAULT = Path(__file__).resolve().parents[3]   # <vault>/markdown/书籍/_workflow/final_check.py
VAULT = str(VAULT)
HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(VAULT, "markdown/书籍/教材原文")
LECT = os.path.join(VAULT, "markdown/书籍/教材讲义")
DIS = os.path.join(VAULT, "markdown/书籍/教材精简")
NODE = os.path.join(VAULT, "MOC/教材")
BOOKS = ["Effective Modern C++", "C++ Concurrency in Action", "Systems Performance",
         "Linux Kernel Programming"]

SEC = re.compile(r"§\s*(Item\s+\d+|Case\s+\d+|附录\s*([A-Z])|Appendix\s+([A-Z])|([0-9]+(?:\.[0-9]+)*))")

def norm(s):
    s = s.strip()
    if s.startswith("Item"):      return ("item", s.split()[-1])
    if s.startswith("Case"):      return ("case", s.split()[-1])
    if s.startswith("附录"):       return ("app", s.replace(" ", "")[-1])
    if s.startswith("Appendix"):  return ("app", s.split()[-1])
    return ("num", s)

def tokens_of(path):
    toks = set()
    try:
        lines = io.open(path, encoding="utf-8", errors="replace")
    except OSError:
        return toks
    for line in lines:
        m = re.match(r"^#{1,6}\s+(.*)$", line)
        if not m: continue
        h = m.group(1).strip()
        for pat, kind in ((r"^Item\s+(\d+)\b", "item"), (r"^Case\s+(\d+)\b", "case"),
                          (r"^(?:Appendix|附录)\s*([A-Z])\b", "app"),
                          (r"^(\d+(?:\.\d+)*)[\.\s]", "num")):
            m2 = re.match(pat, h)
            if m2:
                toks.add((kind, m2.group(1).upper() if kind == "app" else m2.group(1)))
                break
    return toks

def title_key(s):
    s = re.sub(r"[（(]讲义[）)]", "", s)
    s = re.sub(r"^\d+[a-z]?[-_]\s*", "", s)
    s = re.sub(r"^附录\s*[A-Za-z0-9一二三四五六七八九十]+\s*[-–—:：.．]?\s*", "", s)
    s = re.sub(r"^Appendix\s+[A-Za-z0-9]+\s*[-–—:：.．]?\s*", "", s)
    s = re.sub(r"^Chapter\s+\d+\s*[-–—.:：]?\s*", "", s)
    s = re.sub(r"^\d+\s*[-–—.:：]\s*", "", s)
    return re.sub(r"[\s.:：·—–\-_]+", "", s).lower()

def ls(d, book):
    p = os.path.join(d, book)
    if not os.path.isdir(p): return []
    return sorted(f[:-3] for f in os.listdir(p) if f.endswith(".md"))

FRONT = re.compile(r"(?i)^(cover page|about this ebook|half title page|title page|copyright page|"
                   r"dedication page|contents at a glance|contents|table of contents|brief table of contents|"
                   r"foreword|preface|acknowledgments|acknowledgements|about the author|about this book|"
                   r"about the cover illustration|index|list of figures|list of tables|list of listings|"
                   r"copyright|praise for the first edition|brief introduction|introduction|"
                   r"glossary|code snippets|from the publisher|dedication|"
                   "书名页、版权与目录|Packt Early Access 说明与全书目录)$")

def is_front(stem):
    s = re.sub(r"^\d+[a-z]?[-_]\s*", "", stem)
    s = re.sub(r"^Appendix\s+[A-Za-z0-9]+\s*[-–—:：.．]?\s*", "", s)
    s = re.sub(r"^Chapter\s+\d+\s*[-–—.:：]\s*", "", s)
    s = re.sub(r"^\d+\s*[-–—.:：]\s*", "", s)
    return bool(FRONT.match(s.strip()))

def chap_of(stem):
    s = re.sub(r"^\d+[a-z]?[-_]", "", stem)
    s = re.sub(r"^Appendix\s+", "附录 ", s)
    s = re.sub(r"^Chapter\s+(\d+)[\.\s]*", r"第 \1 章 ", s)
    return s

# ── 1. §-定位器 ─────────────────────────────────────
print("=" * 72)
print("【1】§-定位器核对（全书词表）")
print("=" * 72)
book_toks = {}
for b in BOOKS:
    t = set()
    for fn in sorted(os.listdir(os.path.join(RAW, b))):
        if fn.endswith(".md"):
            t |= tokens_of(os.path.join(RAW, b, fn))
    book_toks[b] = t

index = {}
for dp, dn, fns in os.walk(os.path.join(VAULT, "markdown/书籍")):
    dn[:] = [d for d in dn if d != ".git"]
    for fn in fns:
        if fn.endswith(".md"):
            index.setdefault(fn[:-3], os.path.join(dp, fn))

def find_raw(note_path, text):
    comp = re.search(r"companion:\s*\"?\[\[([^\]|]+)", text)
    if comp:
        p = index.get(comp.group(1).strip())
        if p and "/教材原文/" in p: return p
    base = re.sub(r"（讲义）$", "", os.path.basename(note_path)[:-3])
    c = glob.glob(os.path.join(RAW, "*", "*" + base + ".md"))
    return c[0] if c else None

fake = []; nf = 0; nhit = 0
for root in (LECT, DIS):
    for dp, dn, fns in os.walk(root):
        book = os.path.basename(dp)
        if book not in BOOKS: continue
        for fn in sorted(fns):
            if not fn.endswith(".md"): continue
            p = os.path.join(dp, fn)
            t = io.open(p, encoding="utf-8").read()
            rawp = find_raw(p, t)
            if not rawp: continue
            nf += 1
            own = tokens_of(rawp); bt = book_toks[book]
            for i, line in enumerate(t.split("\n"), 1):
                for m in SEC.finditer(line):
                    s = norm(m.group(1).strip())
                    if s in own:
                        nhit += 1; continue
                    if s not in bt:
                        fake.append((os.path.relpath(p, VAULT), i, m.group(1).strip(),
                                     line.strip()[:88], line.lstrip().startswith("#")))
print("笔记 %d 篇 · 命中本章标题 %d 个" % (nf, nhit))
if fake:
    print("⚠️ 全书里都查不到的 %d 处（其中标题行 %d 处 —— 标题行的是真错）:" % (
        len(fake), sum(1 for f in fake if f[4])))
    for f, i, s, line, inh in fake:
        print("  [%s] %s L%-4s §%-12s %s" % ("标题" if inh else "正文", os.path.basename(f)[:40], i, s, line))
else:
    print("✅ 0 处凭空编造")

# ── 2/3/4. 外部脚本 ────────────────────────────────
for label, cmd in (("【2】引文逐字复核", [sys.executable, "-I", os.path.join(HERE, "verify_quotes.py")]),
                   ("【3】锚点校验（精简层）", [sys.executable, "-I", os.path.join(VAULT, "markdown/_tools/kb_anchor_check.py")]),
                   ("【4】全库体检", [sys.executable, "-I", os.path.join(VAULT, "markdown/_tools/kb_health.py")])):
    print("\n" + "=" * 72)
    print(label)
    print("=" * 72)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=900, cwd=HERE)
        out = (r.stdout or "") + (r.stderr or "")
        print("\n".join(out.splitlines()[:40]))
        if r.returncode != 0:
            print("↳ 退出码 %d" % r.returncode)
    except subprocess.TimeoutExpired:
        print("⏱ 超时")

# ── 5. 缺口 ────────────────────────────────────────
print("\n" + "=" * 72)
print("【5】缺口清单")
print("=" * 72)
for book in BOOKS:
    raws = ls(RAW, book); lecs = ls(LECT, book); diss = ls(DIS, book)
    lec_by = {title_key(x) for x in lecs}; dis_by = {title_key(x) for x in diss}
    nl = []; nd = []
    for r in raws:
        if is_front(r): continue
        k = title_key(r)
        if k not in lec_by: nl.append(chap_of(r))
        if k not in dis_by: nd.append(chap_of(r))
    print("%-30s 讲义 %2d/%2d 篇 · 精简 %2d/%2d 篇" % (
        book, len(lecs), len(raws) - sum(1 for r in raws if is_front(r)),
        len(diss), len(raws) - sum(1 for r in raws if is_front(r))))
    if nl: print("    讲义缺: " + " | ".join(nl))
    if nd: print("    精简缺: " + " | ".join(nd))
