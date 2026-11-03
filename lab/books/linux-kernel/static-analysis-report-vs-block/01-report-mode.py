#!/usr/bin/env python3
# @book: Linux Kernel Programming
# @topic: static-analysis-report-vs-block
# @source: Chapter 1. Kernel Workspace Setup（讲义）.md §3.4
# @expect: 扫出 3 条疑似问题(1 条真阳性 + 2 条误报), 退出码仍是 0 —— 只报告不阻断

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


def main():
    hits = scan(SAMPLE)
    print("模式    : 报告式 (report only)")
    print("规则集  : R1 双重释放 / R2 memcpy 未核长 / R3 结构体未清零")
    print("样本    : 内嵌的 C 片段 (run 函数)")
    print("---- 命中 ----")
    for rid, no, why in hits:
        print("  [%s] 第 %d 行: %s" % (rid, no, why))
    print("---- 合计 ----")
    print("疑似问题: %d 条" % len(hits))
    print("判定    : 通过 (报告式永远不阻断流水线)")
    print("代价    : 真阳性也在报告里, 没人看就等于没跑")
    print("退出码  : 0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
