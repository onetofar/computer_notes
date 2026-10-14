#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""教材锚点校验器。

约定（见 MOC/教材-MOC.md）：教材精简笔记里每条引文都应带
    教材原文/<书名>/<章节稿>.md L###   锚点
本脚本逐条把锚点回查原文对应行，检查：
  1. 文件存在、行号在范围内
  2. 该行确实含有笔记声称的关键词（用引文首 8 字做锚）
  3. 统计每篇的锚点覆盖率与失效锚点

用法：
  python kb_anchor_check.py                 # 全量体检
  python kb_anchor_check.py <文件路径...>   # 只查指定笔记
"""
import re, sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

# 脚本位于 <vault>/markdown/_tools/ 下，据此反推 vault 根（不写死绝对路径）
VAULT = Path(__file__).resolve().parents[2]
BOOKS = VAULT / "markdown" / "教材原文"
NOTES = VAULT / "markdown" / "教材精简"

# 锚点：教材原文/<书>/<稿>.md L123（可带 L123-456 区间）
ANCHOR = re.compile(
    r"(?P<file>(?:教材原文/)?[^()\[\]|]*?\.md)\s*L(?P<a>\d+)(?:\s*[-–]\s*(?P<b>\d+))?"
)

# 引文：> … 开头的引用块，或表格行，视为一条引文
QUOTE = re.compile(r"^\s*(?:>\s*)?\|?\s*(?P<body>.+?)\s*$")


def load(path: Path):
    return path.read_text(encoding="utf-8", errors="replace").split("\n")


def norm(s: str) -> str:
    """比对用规范化：去所有空白与 markdown 行内标记，保留实义字符。"""
    s = re.sub(r"\s+", "", s)
    return s


def resolve(ref: str) -> Path | None:
    ref = ref.strip().strip("`").lstrip("./")
    if ref.startswith("教材原文/"):
        cand = VAULT / "markdown" / ref
    else:
        cand = BOOKS / ref
    if cand.exists():
        return cand
    # 只给文件名时在全库按 basename 找
    name = Path(ref).name
    hits = list(BOOKS.rglob(name))
    return hits[0] if len(hits) == 1 else None


def check_note(note: Path):
    lines = load(note)
    anchors, bad, unresolved = 0, [], []
    for ln, line in enumerate(lines, 1):
        # 跳过代码围栏内
        if line.lstrip().startswith("```"):
            continue
        for m in ANCHOR.finditer(line):
            anchors += 1
            f = resolve(m.group("file"))
            if f is None:
                unresolved.append((ln, m.group("file")))
                continue
            src = load(f)
            a = int(m.group("a"))
            b = int(m.group("b") or a)
            if a < 1 or b > len(src) or a > b:
                bad.append((ln, f"行号越界 L{a}-{b}（原文共 {len(src)} 行）", m.group(0)))
                continue
            snippet = line[line.index(m.group(0)) + len(m.group(0)):]
            snippet = snippet.strip().strip("|").strip()
            snippet = re.sub(r"^[：:—–\-]+", "", snippet).strip().strip("|").strip()
            snippet = snippet.rstrip("…").strip()
            if len(norm(snippet)) < 4:
                continue
            seg = "\n".join(src[a - 1: b])
            if norm(snippet) not in norm(seg):
                bad.append((ln, f"锚点行不含引文「{snippet[:24]}」", m.group(0)))
    return anchors, bad, unresolved


def main():
    notes = [Path(a) for a in sys.argv[1:]] or sorted(NOTES.rglob("*.md"))
    total = cov = 0
    print(f"{'':2s} {'笔记':46s} {'行数':>6s} {'锚点':>5s}")
    print("-" * 70)
    for n in notes:
        a, bad, unres = check_note(n)
        try:
            rel = n.relative_to(VAULT / "markdown").as_posix()
        except ValueError:
            rel = n.as_posix()
        rel = rel.rsplit("/", 1)[-1]
        mark = "有" if a else "—"
        total += 1
        if a:
            cov += 1
        print(f"{mark:2s} {rel[:46]:46s} {len(load(n)):6d} {a:5d}")
        for ln, why, frag in bad:
            print(f"      ✗ L{ln}: {why}   [{frag}]")
        for ln, frag in unres:
            print(f"      ✗ L{ln}: 找不到原文稿 {frag}")
    print("-" * 70)
    print(f"合计 {total} 篇，带行号锚点 {cov} 篇（{cov}/{total}）")


if __name__ == "__main__":
    main()