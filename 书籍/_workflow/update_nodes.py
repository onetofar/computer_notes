#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""按磁盘现状刷新几本书的「书名节点 + 两张索引表」。

三层各有目录，节点里的目录表必须跟着磁盘走 ——
否则「表里说有讲义、点进去是断链」，或者「讲义写了、索引里没有」⇒ 变成孤儿笔记。
"""
import os, re, io, glob, sys
sys.stdout.reconfigure(encoding="utf-8")   # Windows stdout 默认 GBK，会让中文输出变乱码

# ⚠️ 2026-11-03：路径从 __file__ 反推，不再写死 /mnt/kb（Windows 侧跑不了）。
from pathlib import Path
VAULT = str(Path(__file__).resolve().parents[3])   # <vault>/markdown/书籍/_workflow/update_nodes.py
BOOKS = ["Effective Modern C++", "C++ Concurrency in Action", "Systems Performance",
         "Linux Kernel Programming"]
RAW = VAULT + "/markdown/书籍/教材原文"
LEC = VAULT + "/markdown/书籍/教材讲义"
DIS = VAULT + "/markdown/书籍/教材精简"
NODE = VAULT + "/MOC/教材"

def title_key(s):
    """把「01-Chapter 5. X」「5. X（讲义）」「附录A-Appendix A： X」统一成同一个键。"""
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

def chap_of(stem):
    s = re.sub(r"^\d+[a-z]?[-_]", "", stem)
    s = re.sub(r"^Appendix\s+", "附录 ", s)
    s = re.sub(r"^Chapter\s+(\d+)[\.\s]*", r"第 \1 章 ", s)
    return s

FRONT = re.compile(r"(?i)^(cover page|about this ebook|half title page|title page|copyright page|"
                   r"dedication page|contents at a glance|contents|table of contents|brief table of contents|"
                   r"foreword|preface|acknowledgments|acknowledgements|about the author|"
                   r"about the cover illustration|about this book|index|list of figures|list of tables|list of listings|"
                   r"copyright|praise for the first edition|brief introduction|introduction|"
                   r"glossary|code snippets|from the publisher|dedication|cover page|"
                   r"书名页、版权与目录|Packt Early Access 说明与全书目录)$")

def is_front(stem):
    s = re.sub(r"^\d+[a-z]?[-_]\s*", "", stem)
    s = re.sub(r"^Appendix\s+[A-Za-z0-9]+\s*[-–—:：.．]?\s*", "", s)
    s = re.sub(r"^Chapter\s+\d+\s*[-–—.:：]?\s*", "", s)
    s = re.sub(r"^\d+\s*[-–—.:：]\s*", "", s)
    return bool(FRONT.match(s.strip()))

def link(t): return "[[%s]]" % t

for book in BOOKS:
    node = os.path.join(NODE, book + ".md")
    if not os.path.exists(node):
        print("跳过（节点不存在）", book); continue
    text = io.open(node, encoding="utf-8").read()
    raws = ls(RAW, book); lecs = ls(LEC, book); diss = ls(DIS, book)
    lec_by = {title_key(x): x for x in lecs}
    dis_by = {title_key(x): x for x in diss}

    tbl = ["| 章 | 原文稿 | 讲义 | 精简 | 状态 |", "|---|---|---|---|---|"]
    for r in raws:
        k = title_key(r)
        lc, dc = lec_by.get(k), dis_by.get(k)
        if lc and dc:   st = "**已精读**"
        elif lc:        st = "**已沉淀**（讲义已就位 · 精简待补）"
        elif dc:        st = "**已沉淀**（精简已就位 · 讲义待补）"
        elif is_front(r): st = "不立篇"
        else:           st = "未净化"
        tbl.append("| %s | %s | %s | %s | %s |" % (
            chap_of(r), link(r), link(lc) if lc else "—", link(dc) if dc else "—", st))
    new_tbl = "\n".join(tbl)

    m = re.search(r"(\| 章 \| 原文稿 \| 讲义 \| 精简 \| 状态 \|.*?)(?=\n\n)", text, re.S)
    if not m:
        print("!! 目录表定位失败", book); continue
    text = text[:m.start(1)] + new_tbl + text[m.end(1):]

    text = re.sub(r"(\| 原文章节稿 \| `markdown/书籍/教材原文/" + re.escape(book) + r"/` \| )\d+(\|)",
                  lambda m: m.group(1) + str(len(raws)) + m.group(2), text)
    text = re.sub(r"(\| 讲义 \| `markdown/书籍/教材讲义/" + re.escape(book) + r"/` \| )[^|]*(\|)",
                  lambda m: m.group(1) + ("%d 篇 " % len(lecs) if lecs else "待写 ") + m.group(2), text)
    text = re.sub(r"(\| 精简 \| `markdown/书籍/教材精简/" + re.escape(book) + r"/` \| )[^|]*(\|)",
                  lambda m: m.group(1) + ("%d 篇 " % len(diss) if diss else "待写 ") + m.group(2), text)
    text = re.sub(r"\n> \*\*讲义标题是计划\*\*（还没写）.*?\n", "\n", text, flags=re.S)
    # 精简行的描述随状态变
    text = re.sub(r"(\| 精简 \| `markdown/书籍/教材精简/" + re.escape(book) + r"/` \| [^|]*\| )[^|]*(\|)",
                  lambda m: m.group(1) + (("**已开写**，逐条可回查原文行号" if diss else "尚未开始")) + m.group(2), text)
    io.open(node, "w", encoding="utf-8", newline="\n").write(text)   # newline="\n"：Windows 文本模式默认会把 LF 写成 CRLF
    print("节点已刷新 %-30s 原文稿 %2d / 讲义 %2d / 精简 %2d" % (book, len(raws), len(lecs), len(diss)))


# ── 两张索引表 ─────────────────────────────────────────
def counts(book):
    return len(ls(RAW, book)), len(ls(LEC, book)), len(ls(DIS, book))

# markdown/书籍/README.md 的「现有书目」表
rp = VAULT + "/markdown/书籍/README.md"
rt = io.open(rp, encoding="utf-8").read()
for book, label in (("Effective Modern C++", "《Effective Modern C++》(EMC++·Meyers·英文原版)"),
                    ("C++ Concurrency in Action", "《C++ Concurrency in Action 第二版》(CiA·Williams·英文原版)"),
                    ("Systems Performance", "《Systems Performance 第二版》(Brendan Gregg·英文原版)"),
                    ("Linux Kernel Programming",
                     "《Linux Kernel Programming 第二版》(Billimoria·英文原版·试读本)")):
    r, l, d = counts(book)
    pat = re.compile(r"^\| " + re.escape(label) + r" \| [^|]*\| [^|]*\| [^|]*\|.*$", re.M)
    new = "| %s | %d 篇 | %s | %s | [[%s]] |" % (
        label, r, ("%d 篇" % l) if l else "待写", ("%d 篇" % d) if d else "—", book)
    if pat.search(rt):
        rt = pat.sub(lambda _: new, rt)
    else:
        print("!! README 里没找到行：", label)
io.open(rp, "w", encoding="utf-8", newline="\n").write(rt)   # 同上，保住 LF
print("README 书目表已刷新")

# MOC/教材-MOC.md 的「书名节点一览」表
mp = VAULT + "/MOC/教材-MOC.md"
mt = io.open(mp, encoding="utf-8").read()
for book, author in (("Effective Modern C++", "Scott Meyers"),
                     ("C++ Concurrency in Action", "Anthony Williams"),
                     ("Systems Performance", "Brendan Gregg"),
                     ("Linux Kernel Programming", "Kaiwan N. Billimoria")):
    r, l, d = counts(book)
    tail = "原文稿 `markdown/书籍/教材原文/%s/`（%d 篇 · 英文原版）" % (book, r)
    tail += " · 讲义 %s" % (("%d 篇" % l) if l else "待写")
    tail += " · 精简 %s" % (("%d 篇" % d) if d else "待写")
    new = "| [[%s]] | %s | **正在读** | %s |" % (book, author, tail)
    pat = re.compile(r"^\| \[\[" + re.escape(book) + r"\]\] \|.*$", re.M)
    if pat.search(mt):
        mt = pat.sub(lambda _: new, mt)
    else:
        print("!! 教材-MOC 里没找到行：", book)
io.open(mp, "w", encoding="utf-8", newline="\n").write(mt)   # 同上，保住 LF
print("教材-MOC 书名节点表已刷新")
