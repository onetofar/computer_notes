#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""知识库体检：frontmatter 有效性 + 断链。临时诊断脚本。"""
import os, re, sys, json
from pathlib import Path

# 脚本位于 <vault>/markdown/_tools/ 下，据此反推 vault 根（不写死绝对路径）
VAULT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {".git", ".obsidian", ".trash", "Base", "node_modules"}
CODE_EXT = {".excalidraw.md"}

WIKILINK = re.compile(r"\[\[([^\]\|#]+?)(?:#[^\]\|]*?)?(?:\|[^\]]*?)?\]\]")


def iter_files(root: Path, all_ext=False):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for fn in filenames:
            p = Path(dirpath) / fn
            rel = p.relative_to(VAULT).as_posix()
            if all_ext:
                yield p, rel
                continue
            if not fn.endswith(".md"):
                continue
            yield p, rel


def strip_code_and_inline(text: str):
    """把代码围栏与行内反引号的内容替换成等长空白，避免误报。"""
    out = []
    in_fence = False
    fence = None
    for line in text.split("\n"):
        stripped = line.lstrip()
        if in_fence:
            out.append("")
            if stripped.startswith(fence):
                in_fence = False
                fence = None
            continue
        if stripped.startswith("```") or stripped.startswith("~~~"):
            in_fence = True
            fence = stripped[:3]
            out.append("")
            continue
        # 行内反引号
        out.append(re.sub(r"`[^`\n]*`", lambda m: " " * len(m.group(0)), line))
    return "\n".join(out)


def main():
    files = list(iter_files(VAULT))
    basenames = {}
    for _, rel in iter_files(VAULT, all_ext=True):
        basenames.setdefault(Path(rel).name, []).append(rel)
        basenames.setdefault(Path(rel).stem, []).append(rel)
    # .excalidraw.md 的 Obsidian basename 含 .excalidraw
    for p in Path(VAULT).rglob("*.excalidraw.md"):
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        basenames.setdefault(p.name[: -len(".md")], []).append(
            p.relative_to(VAULT).as_posix())

    broken = []
    dup_basename = {k: v for k, v in basenames.items() if len(v) > 1}

    for p, rel in files:
        text = p.read_text(encoding="utf-8", errors="replace")
        clean = strip_code_and_inline(text)
        for lineno, line in enumerate(clean.split("\n"), 1):
            for m in WIKILINK.finditer(line):
                target = m.group(1).strip()
                if not target:
                    continue
                # 纯 URL / 路径型显式路径
                stem = Path(target).name or target
                if stem not in basenames:
                    broken.append((rel, lineno, target))

    print(f"files={len(files)}  basenames={len(basenames)}")
    print(f"\n=== 断链 {len(broken)} 条 / {len({b[0] for b in broken})} 篇 ===")
    for rel, lineno, target in broken:
        print(f"{rel}:{lineno}  ->  [[{target}]]")
    print(f"\n=== 同名文件歧义 {len(dup_basename)} 组 ===")
    for k, v in sorted(dup_basename.items()):
        print(f"{k}: {v}")


if __name__ == "__main__":
    main()