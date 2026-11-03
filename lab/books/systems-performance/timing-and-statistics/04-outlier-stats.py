#!/usr/bin/env python3
# @book: Systems Performance
# @topic: timing-and-statistics
# @source: Chapter 2. Methodologies（讲义）.md §2.8.6 Outliers
# @expect: 均值被少数离群点拉高, 中位数几乎不动; 去掉离群点后两者都回落 —— 说明"平均值藏细节"

import sys


def synthetic_samples():
    """复刻教材的双峰场景: 绝大多数 1ms, 少数 100ms+ 的离群点。"""
    data = []
    for i in range(1000):
        if i % 50 == 0:
            data.append(100.0 + (i % 7) * 5.0)   # 2% 的慢 I/O
        elif i % 50 == 7:
            data.append(7.0 + (i % 5) * 1.0)     # 慢路径峰
        else:
            data.append(0.8 + (i % 11) * 0.02)    # 快路径峰
    return data


def load(path):
    data = []
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                data.append(float(line.split(":")[-1]))
            except ValueError:
                continue
    return data


def percentile(sorted_data, p):
    if not sorted_data:
        return 0.0
    k = (len(sorted_data) - 1) * p
    lo = int(k)
    hi = min(lo + 1, len(sorted_data) -1)
    return sorted_data[lo] + (sorted_data[hi] - sorted_data[lo]) * (k - lo)


def main():
    if len(sys.argv) > 1:
        data = load(sys.argv[1])
        if not data:
            print("no samples parsed from %s" % sys.argv[1])
            return 0
    else:
        data = synthetic_samples()

    s = sorted(data)
    n = len(s)
    mean = sum(s) / n
    median = percentile(s, 0.50)
    cut = percentile(s, 0.99)
    trimmed = [x for x in s if x < cut]
    tmean = sum(trimmed) / len(trimmed)
    var = sum((x - mean) ** 2 for x in s) / n
    stddev = var ** 0.5

    print("samples        : %d" % n)
    print("mean           : %.3f" % mean)
    print("median (p50)   : %.3f" % median)
    print("p95            : %.3f" % percentile(s, 0.95))
    print("p99            : %.3f" % percentile(s, 0.99))
    print("max            : %.3f" % s[-1])
    print("stddev         : %.3f" % stddev)
    print("CoV            : %.1f%%" % (stddev / mean * 100.0))
    print("trimmed mean   : %.3f  (drops the top 1%%)" % tmean)
    print("mean - median  : %+.3f  <- the gap is where the outliers live" % (mean - median))
    print("note: report the distribution, not just the average.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
