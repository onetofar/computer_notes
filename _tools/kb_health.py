#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""知识库体检：断链 + 同名歧义 + 孤儿检测。临时诊断脚本。"""
import os, re, sys, json
from pathlib import Path

# 脚本位于 <vault>/markdown/_tools/ 下，据此反推 vault 根（不写死绝对路径）
VAULT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {".git", ".obsidian", ".trash", "Base", "node_modules"}
CODE_EXT = {".excalidraw.md"}

# ── 孤儿检测豁免名单 ─────────────────────────────────────────────
# 豁免 = 「不被 wikilink 指向」是这些位置的**正常状态**，不是漏回挂。
# 名单必须显式维护：多豁免一个区，绊线就瞎一格；少豁免一个区，全是误报没人信。
ORPHAN_EXEMPT_DIRS = (
    "MOC/",         # 聚合入口：被读、不被链（MOC 链别人，没人链 MOC）
    "教材原文/",     # 锚点层：消费方是 kb_anchor_check 的「行号 + 引文」，不是 wikilink
    "Excalidraw/",  # 画板资源目录
    "md_pict/",     # 图片附件
    "_tools/",      # 脚本互提全在代码围栏里，会被 strip_code_and_inline 抹掉，永远测不出入链
)
ORPHAN_EXEMPT_NAMES = ("README", "AGENTS")  # 入口说明类文件，按 stem 豁免
PROJECT_AREA_PREFIX = "markdown/个人项目/"    # 孤儿检测的合格线只管这块；其余领域信息性报告

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


def orphan_exempt(rel: str) -> bool:
    """该笔记是否豁免孤儿检测（「无入链」对它是正常状态）。"""
    if "/" not in rel:                       # vault 根的散件（.gitignore 等）本就不是笔记
        return True
    if rel.endswith(".excalidraw.md"):       # 画板散落在各目录，按后缀豁免而不是按目录
        return True
    if any(d in rel for d in ORPHAN_EXEMPT_DIRS):
        return True
    return Path(rel).stem in ORPHAN_EXEMPT_NAMES


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
    used_keys = set()   # 被（有效）wikilink 指到的 key 集合，供孤儿检测用

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
                else:
                    used_keys.add(stem)

    print(f"files={len(files)}  basenames={len(basenames)}")
    print(f"\n=== 断链 {len(broken)} 条 / {len({b[0] for b in broken})} 篇 ===")
    for rel, lineno, target in broken:
        print(f"{rel}:{lineno}  ->  [[{target}]]")
    print(f"\n=== 同名文件歧义 {len(dup_basename)} 组 ===")
    for k, v in sorted(dup_basename.items()):
        print(f"{k}: {v}")

    # ── 孤儿检测：存在但没有任何有效入链的笔记 ─────────────────────
    # 合格线只管项目区（新笔记回挂漏做会在这里当场暴露）；
    # 其余领域的孤儿是信息性报告，由人决定要不要补豁免或补链接。
    project_orphans, other_orphans = [], []
    exempt_n = 0
    for p, rel in files:
        if orphan_exempt(rel):
            exempt_n += 1
            continue
        keys = {Path(rel).name, Path(rel).stem}
        if keys & used_keys:
            continue
        (project_orphans if rel.startswith(PROJECT_AREA_PREFIX) else other_orphans).append(rel)

    print(f"\n=== 项目区孤儿 {len(project_orphans)} 篇（豁免 {exempt_n} 篇）===")
    for rel in sorted(project_orphans):
        print(rel)
    print(f"\n=== 其他领域孤儿 {len(other_orphans)} 篇（信息性，不卡合格线）===")
    for rel in sorted(other_orphans):
        print(rel)


if __name__ == "__main__":
    main()