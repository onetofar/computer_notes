#!/usr/bin/env python3
"""kb-lab 守卫: 目录结构 / 元信息 / 颗粒度 三项硬约束。

规则:
  1. books/<slug>/ 必须是已知书目 slug; 主题目录 slug 为 kebab-case
  2. 书目目录下**只能有主题目录和 README.md** —— 实现文件必须住在主题目录里
  3. 每个源文件首部必须有元信息块: @book @topic @source @expect
  4. 颗粒度: 单文件 <= MAX_LINES 行; 单主题目录 <= MAX_FILES 个实现文件
  5. 每个主题目录必须有 README.md; 每本书必须有 README.md
  6. 每个主题目录建议 >= 2 个实现文件 (对照讨论性质)
  7. 文件必须是纯文本 UTF-8, 不含二进制
"""
import os
import re
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent.parent
BOOKS = ROOT / "books"

KNOWN_BOOKS = {
    "effective-cpp",
    "effective-modern-cpp",
    "cpp-concurrency-in-action",
    "linux-multithread-server",
    "csapp",
    "os-concepts",
    "systems-performance",
    "game-arch",
    "game-dev",
    "linux-kernel",
}

IMPL_EXT = {".cpp", ".c", ".sh", ".py"}
HEADER_EXT = {".h", ".hpp"}
DOC_EXT = {".md"}
MAX_LINES = 120
MAX_FILES = 8
REQUIRED_KEYS = ("@book", "@topic", "@source", "@expect")

KEBAB = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")

errors = []
warnings = []
stats = {"books": 0, "topics": 0, "impl": 0, "lines": 0}


def rel(p):
    try:
        return str(p.relative_to(ROOT)).replace(os.sep, "/")
    except ValueError:
        return str(p)


def check_src(f):
    """检查源文件的元信息 / 编码 / 颗粒度。"""
    try:
        raw = f.read_bytes()
    except Exception as e:
        errors.append(f"[io] 读取失败: {rel(f)} {e}")
        return
    if b"\x00" in raw:
        errors.append(f"[bin] 含二进制内容: {rel(f)}")
        return
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        errors.append(f"[enc] 非 UTF-8: {rel(f)}")
        return
    lines = text.splitlines()
    stats["lines"] += len(lines)
    if len(lines) > MAX_LINES:
        errors.append(f"[grain] 文件过长 ({len(lines)} > {MAX_LINES}): {rel(f)}")
    head = "\n".join(lines[:20])
    for k in REQUIRED_KEYS:
        if k not in head:
            errors.append(f"[meta] 缺少 {k}: {rel(f)}")


if not BOOKS.is_dir():
    print("FAIL: books/ 目录不存在")
    sys.exit(1)

book_dirs = sorted([d for d in BOOKS.iterdir() if d.is_dir()])
stats["books"] = len(book_dirs)

for bd in book_dirs:
    if not KEBAB.match(bd.name):
        errors.append(f"[slug] 书目目录名非 kebab-case: {bd.name}")
    if bd.name not in KNOWN_BOOKS:
        errors.append(f"[slug] 未知书目 slug: {bd.name} (允许: {sorted(KNOWN_BOOKS)})")
    if not (bd / "README.md").is_file():
        errors.append(f"[readme] 书目缺少 README.md: {rel(bd)}")

    for entry in sorted(bd.iterdir()):
        if entry.name.startswith("."):
            continue
        if entry.is_file():
            suf = entry.suffix.lower()
            if suf in IMPL_EXT or suf in HEADER_EXT:
                errors.append(f"[layout] 源文件必须住在主题目录里: {rel(entry)}")
                check_src(entry)
            elif suf not in DOC_EXT:
                errors.append(f"[ext] 不支持的文件类型: {rel(entry)}")
            continue
        if not entry.is_dir():
            continue

        td = entry
        if not KEBAB.match(td.name):
            errors.append(f"[slug] 主题目录名非 kebab-case: {rel(td)}")
        if not (td / "README.md").is_file():
            errors.append(f"[readme] 主题目录缺少 README.md: {rel(td)}")
        stats["topics"] += 1

        impls = []
        for f in sorted(td.iterdir()):
            if not f.is_file():
                continue
            suf = f.suffix.lower()
            if suf in DOC_EXT:
                continue
            if suf in HEADER_EXT:
                check_src(f)
                continue
            if suf not in IMPL_EXT:
                errors.append(f"[ext] 不支持的文件类型: {rel(f)}")
                continue
            impls.append(f)

        if not impls:
            errors.append(f"[empty] 主题目录没有实现文件: {rel(td)}")
            continue
        if len(impls) > MAX_FILES:
            errors.append(f"[grain] 主题目录文件过多 ({len(impls)} > {MAX_FILES}): {rel(td)}")
        if len(impls) < 2:
            warnings.append(f"[grain] 主题目录只有 1 个实现文件, 对照性不足: {rel(td)}")

        for f in impls:
            stats["impl"] += 1
            check_src(f)


print("=" * 60)
print(f"书目={stats['books']} 主题={stats['topics']} 实现文件={stats['impl']} 总行数={stats['lines']}")
print("=" * 60)

if warnings:
    print(f"--- 警告 {len(warnings)} 条 ---")
    for w_ in warnings:
        print("  " + w_)
    print("")

if errors:
    print(f"--- 错误 {len(errors)} 条 ---")
    for e in errors:
        print("  " + e)
    sys.exit(1)

print("OK: 目录结构 / 元信息 / 颗粒度 全部通过")
sys.exit(0)
