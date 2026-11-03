#!/usr/bin/env python3
# @book: Systems Performance
# @topic: memory-and-network-latency
# @source: Chapter 10. Network（讲义）.md §10.3.5
# @expect: 小包受延迟支配, 大包受带宽支配; 要跑满带宽, 在途字节数必须够一个 BDP —— 并发度不足就是延迟账没还清

import sys

CAS_NS = 80.0          # 典型 DRAM 访问延迟量级 (ns)
LINK_GBS = 10.0        # 假想链路带宽 (GB/s)
RTT_NS = 2 * CAS_NS    # 假想往返时延 (ns)


def latency_limited(nbytes):
    """一次往返只传 nbytes: 受 RTT 支配。"""
    return RTT_NS


def bandwidth_limited(nbytes):
    """持续灌满链路: 受带宽支配。"""
    return nbytes / (LINK_GBS * 1e9) * 1e9


def bdp_bytes():
    """带宽时延积: 要跑满带宽, 在途字节数至少要有一个 BDP。"""
    return LINK_GBS * 1e9 * (RTT_NS / 1e9)


def main():
    print("=== 小包还是大包: 谁被延迟支配, 谁被带宽支配 ===")
    print("%-10s %12s %14s %14s %10s" % ("payload", "bytes", "latency_ms", "bw_ms", "dominant"))
    for payload in (64, 1500, 64 * 1024, 8 * 1024 * 1024):
        lat = latency_limited(payload) / 1e6
        bw = bandwidth_limited(payload) / 1e6
        dominant = "latency" if lat > bw else "bandwidth"
        print("%-10s %12d %14.4f %14.4f %10s" % ("payload", payload, lat, bw, dominant))

    print()
    print("=== 传 8MB 需要多少并发流才能跑满带宽 ===")
    total = 8 * 1024 * 1024
    print("link bandwidth : %.0f GB/s" % LINK_GBS)
    print("rtt            : %.0f ns" % RTT_NS)
    print("BDP            : %.0f bytes (在途字节数低于它就跑不满带宽)" % bdp_bytes())
    print()
    print("%-8s %16s %16s %14s" % ("streams", "in_flight_B", "total_ms", "vs_serial"))

    serial_ms = latency_limited(64) * (total / 64) / 1e6
    for streams in (1, 8, 64, 512, 4096):
        in_flight = streams * 64
        # 每个流受自己的 RTT 支配, 全部并行; 总时间取最大者
        per_stream_ms = latency_limited(64) * (total / 64 / streams) / 1e6
        total_ms = max(per_stream_ms, bandwidth_limited(total) / 1e6)
        print("%-8d %16d %16.3f %13.1fx" % (streams, in_flight, total_ms, serial_ms / total_ms))

    print()
    print("note: latency is a property of a round trip; bandwidth is a property of a stream.")
    print("      小包被延迟卡住时, 加并发流就能把它救回来 —— 这是延迟账没还清, 不是带宽不够。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
