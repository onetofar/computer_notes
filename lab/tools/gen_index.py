#!/usr/bin/env python3
"""Generate the lab book index (books/README.md) from the directory tree.

Usage: python3 tools/gen_index.py [--write]
  Without --write it only prints to stdout.
"""
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

NL = chr(10)

ROOT = Path(__file__).resolve().parent.parent
BOOKS = ROOT / "books"

BOOK_NAMES = {
    "effective-cpp": "Effective C++ (3rd, Chinese)",
    "effective-modern-cpp": "Effective Modern C++",
    "cpp-concurrency-in-action": "C++ Concurrency in Action",
    "linux-multithread-server": "Linux Multi-thread Server Programming",
    "csapp": "Computer Systems: A Programmer Perspective",
    "os-concepts": "Operating Systems: Three Easy Pieces",
    "systems-performance": "Systems Performance",
    "game-arch": "Networked Game Architecture",
    "game-dev": "Networked Game Development in Practice",
    "linux-kernel": "Linux Kernel Programming",
}

IMPL_EXT = {".cpp", ".c", ".sh", ".py"}

HEADER = [
    "# kb-lab: 教材讲义的可执行对照实验",
    "",
    "每个子目录对应一本教材。每个主题目录是一个**对照讨论**：",
    "同一问题的多种写法并列，各自独立编译、可独立运行、输出可直接横向对比。",
    "",
    "## 书目索引",
    "",
    "| slug | 教材 | 主题数 | 实现文件数 |",
    "| --- | --- | --- | --- |",
]

TAIL = [
    "## 规则",
    "",
    "1. 一个主题目录 = 一个对照讨论，≥2 个实现文件",
    "2. 单文件 ≤ 120 行，只验证一个机制",
    "3. 文件头必须有 `@book @topic @source @expect` 元信息",
    "4. `@ub: yes` 标注故意演示未定义行为，默认跳过",
    "5. 引文只放在主题 README，不放代码里",
    "",
    "## 跑法",
    "",
    "```bash",
    "python3 tools/lab_check.py     # 守卫: 结构 / 元信息 / 颗粒度",
    "bash tools/run_all.sh           # 编译并运行全部",
    "bash tools/run_all.sh --ub      # 连 UB 演示一起跑",
    "bash tools/build_all.sh         # 只做语法检查",
    "```",
]


def scan():
    rows = []
    for bd in sorted(BOOKS.iterdir()):
        if not bd.is_dir() or bd.name.startswith("."):
            continue
        topics = []
        impl_total = 0
        for td in sorted(bd.iterdir()):
            if not td.is_dir() or td.name.startswith("."):
                continue
            n = 0
            for f in td.iterdir():
                if f.is_file() and f.suffix.lower() in IMPL_EXT:
                    n += 1
            impl_total += n
            topics.append((td.name, n))
        rows.append((bd.name, topics, impl_total))
    return rows


def render_books(rows):
    out = list(HEADER)
    for slug, topics, impl_total in rows:
        name = BOOK_NAMES.get(slug, slug)
        out.append("| `" + slug + "` | " + name + " | " + str(len(topics)) + " | " + str(impl_total) + " |")
    out.append("")
    out.append("## 各书主题目录")
    out.append("")
    for slug, topics, impl_total in rows:
        name = BOOK_NAMES.get(slug, slug)
        out.append("### `" + slug + "` — " + name)
        out.append("")
        for tname, n in topics:
            out.append("- `" + tname + "/` — " + str(n) + " 个实现文件")
        out.append("")
    out.extend(TAIL)
    return NL.join(out) + NL


def render_root(rows):
    out = [
        "| slug | 教材 | 主题数 |",
        "| --- | --- | --- |",
    ]
    for slug, topics, impl_total in rows:
        name = BOOK_NAMES.get(slug, slug)
        out.append("| `" + slug + "` | " + name + " | " + str(len(topics)) + " |")
    return NL.join(out) + NL


def main():
    rows = scan()
    if "--write" in sys.argv:
        target = BOOKS / "README.md"
        target.write_text(render_books(rows), encoding="utf-8", newline="")
        print("wrote " + str(target))
    else:
        sys.stdout.write(render_books(rows))
    tt = sum(len(t) for _, t, _ in rows)
    ti = sum(i for _, _, i in rows)
    print("--- topics table ---", file=sys.stderr)
    print(render_root(rows), file=sys.stderr)
    print("totals: topics=" + str(tt) + " impl=" + str(ti), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
