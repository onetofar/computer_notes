---
title: "ping 网关实践"
category: 计算机网络
tags:
  - net/ip
  - net/practice
  - net/udp
  - networking
difficulty: 基础
source: "自整理"
---

ping 网关 192.168.1.1
```
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0xa036
  标志: 0x0002, 偏移: 0
  TTL: 64, 协议: 1
  头部校验和: 0x1719
  Source IP: 192.168.1.8
  Destination IP: 192.168.1.1
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x036c
  标志: 0x0000, 偏移: 0
  TTL: 64, 协议: 1
  头部校验和: 0xf3e3
  Source IP: 192.168.1.1
  Destination IP: 192.168.1.8
=======================
```

这是本机 192.168.1.8 ping 网关 192.168 .1.1 的 ICMP 请求与响应包。均为标准 IPv4，头部 20 字节无选项，DSCP/ECN 为 0，协议 1（ICMP），TTL64。请求包设 DF 不分片，标识、校验和、源目 IP 因收发互换不同，无分片无转发，是正常内网 ping 报文

ping 公网 223.5.5.5

```
正在监听网卡: eth1
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x97c0
  标志: 0x0002, 偏移: 0
  TTL: 64, 协议: 1
  头部校验和: 0xfd2d
  Source IP: 192.168.1.8
  Destination IP: 223.5.5.5
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x07f6
  标志: 0x0000, 偏移: 0
  TTL: 51, 协议: 1
  头部校验和: 0xd9f8
  Source IP: 223.5.5.5
  Destination IP: 192.168.1.8
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x980a
  标志: 0x0002, 偏移: 0
  TTL: 64, 协议: 1
  头部校验和: 0xfce3
  Source IP: 192.168.1.8
  Destination IP: 223.5.5.5
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x097e
  标志: 0x0000, 偏移: 0
  TTL: 51, 协议: 1
  头部校验和: 0xd870
  Source IP: 223.5.5.5
  Destination IP: 192.168.1.8
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x98f7
  标志: 0x0002, 偏移: 0
  TTL: 64, 协议: 1
  头部校验和: 0xfbf6
  Source IP: 192.168.1.8
  Destination IP: 223.5.5.5
=======================
====== IP Header ======
  版本: 4, IHL: 20 bytes
  DSCP: 0, ECN: 0
  总长度: 84 bytes
  标识: 0x0b77
  标志: 0x0000, 偏移: 0
  TTL: 51, 协议: 1
  头部校验和: 0xd677
  Source IP: 223.5.5.5
  Destination IP: 192.168.1.8
=======================
====== IP Header ======
  版本: 0, IHL: 0 bytes
  DSCP: 0, ECN: 1
  总长度: 2048 bytes
  标识: 0x0604
  标志: 0x0000, 偏移: 1
  TTL: 72, 协议: 69
  头部校验和: 0xe6a0
  Source IP: 220.41.192.168
  Destination IP: 1.8.0.0
=======================
====== IP Header ======
  版本: 0, IHL: 0 bytes
  DSCP: 0, ECN: 1
  总长度: 2048 bytes
  标识: 0x0604
  标志: 0x0000, 偏移: 2
  TTL: 84, 协议: 43
  头部校验和: 0x7664
  Source IP: 160.106.192.168
  Destination IP: 1.1.72.69
=======================

```

本次监听eth1网卡共捕获8个IP报文，前6个为**本机192.168.1.8 ping公网223.5.5.5**的3组标准ICMP请求与响应包，后2个为非法网络垃圾报文。 有效报文均为标准IPv4格式，头部20字节无选项，DSCP、ECN均为0，总长度84字节无分片，协议号1对应ICMP。请求包TTL为64（本机初始值），标志位DF=1禁止分片，标识字段逐包自增；响应包TTL降至51，经13跳公网路由衰减，标志位无DF设置，源目IP与请求包反转，交互合规。 

末尾两个报文版本为0、IHL为0字节，头部格式非法，属于网络异常碎片，与本次ping无关。本机为主机关闭IP转发，仅处理目标IP为本机的报文，非本机包均直接丢弃，未转发任何数据，完全符合主机IP模块处理规则，整体公网ping通信正常。
