#!/usr/bin/env python3
# @book: Linux Kernel Programming
# @topic: static-analysis-report-vs-block
# @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.4
# @expect: 把人工确认过的 2 条误报加进白名单, 门禁只剩 1 条真阳性, 信号重新可信
# @note: 加 --strict 才真的返回退出码 1, 默认只打印判定, 便于横向对比三个实现

import re
import sys

SAMPLE = """\
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cfg {
    int width;
    int height;
};

/* src 的长度由调用方保证 <= 32; 静态检查器看不到这个前提 */
void run(const char *src)
{
    char buf[256];
    struct cfg c;
    int *p = malloc(sizeof(int) * 4);

    if (p == NULL)
        return;
    c.width = 8;
    c.height = 8;
    memcpy(buf, src, strlen(src) + 1);
    printf("%d %d %s %d", c.width, c.height, buf, p[0]);
    free(p);
    free(p);
}
"""

FREE_RE = re.compile(r"\bfree\s*\(\s*([A-Za-z_]\w*)\s*\)")
MEMCPY_RE = re.compile(r"\bmemcpy\s*\(")
STRUCT_RE = re.compile(r"^\s*struct\s+[A-Za-z_]\w*\s+[A-Za-z_]\w*\s*;\s*$", re.M)


def scan(text):
    """跑一遍规则集, 返回 [(规则号, 行号, 说明), ...]"""
    lines = text.splitlines()
    hits = []

    frees = {}
    for no, line in enumerate(lines, 1):
        for m in FREE_RE.finditer(line):
            frees.setdefault(m.group(1), []).append(no)
    for name, nos in frees.items():
        if len(nos) > 1:
            hits.append(("R1", nos[0], "同一指针 %s 被 free %d 次 (行 %s)"
                         % (name, len(nos), ",".join(str(n) for n in nos))))

    for no, line in enumerate(lines, 1):
        if MEMCPY_RE.search(line):
            hits.append(("R2", no, "memcpy 到栈缓冲区, 检查器无法核对长度"))
        if STRUCT_RE.match(line):
            hits.append(("R3", no, "结构体未整体清零 (但两个字段随后都被赋值)"))

    hits.sort(key=lambda h: (h[1], h[0]))
    return hits


# 人工复核后确认的误报: memcpy 那行的长度由调用方保证; 结构体两个字段都赋过值
WHITELIST = {"R2", "R3"}


def main():
    hits = scan(SAMPLE)
    kept = [h for h in hits if h[0] not in WHITELIST]
    muted = [h for h in hits if h[0] in WHITELIST]
    print("模式    : 驯化式 (tamed: 误报进白名单)")
    print("规则集  : R1 双重释放 / R2 memcpy 未核长 / R3 结构体未清零")
    print("样本    : 内嵌的 C 片段 (run 函数)")
    print("白名单  : %s (人工复核确认为误报)" % ", ".join(sorted(WHITELIST)))
    print("---- 命中 ----")
    for rid, no, why in hits:
        print("  [%s] 第 %d 行: %s  <- %s"
              % (rid, no, why, "抑制" if rid in WHITELIST else "保留"))
    print("---- 合计 ----")
    print("疑似问题: %d 条 -> 抑制 %d 条, 剩余 %d 条" % (len(hits), len(muted), len(kept)))
    print("判定    : 失败 (剩余的 1 条是真阳性, 该阻断)")
    print("代价    : 白名单要有人维护, 否则误报会重新长回来")
    print("退出码  : 1 (加 --strict 时真的返回 1)")
    if "--strict" in sys.argv[1:]:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
