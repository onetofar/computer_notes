---
title: 7-1 UDP可靠传输-传递通知与有序判定
category: 网络
tags:
  - 教材精读
  - 网络
  - 格雷泽
  - UDP
  - 可靠性
difficulty: 进阶
source: "《网络多人游戏架构与编程》§7.5 / §7.5.1 标记传出的数据包 / §7.5.2 接收数据包并发送确认"
maturity: 完善
created: 2026-10-07
skills:
  - obsidian-markdown
---

> [!info] 核心结论
> UDP 自己不做任何可靠性，所以「送达」这件事必须由应用层自己记账。
> `DeliveryNotificationManager`（DNM）的做法是三步：给每个发出的包编**唯一序列号** →
> 接收端三路判包、只处理序号 **≥ 下一个期望值**的包 →
> 把确认**攒起来**塞进下一个反向包。
> 它**故意不做重传**：重传什么、传原版还是最新版，由上层依赖模块自己决定——
> 这是原文说的「基于 UDP 的可靠传输所提供的灵活性的主要来源，而 TCP 并不能提供」。

## 机制

### 一、DNM 做什么、不做什么（§7.5 引言）

原文列的三件事：

1. 传输时**唯一标识和标记每个发送出去的数据包**，把传递状态与每个包关联，
   并以一种有意义的方式传递给依赖模块。
2. 接收端**检查传入的数据包，并针对每个它决定处理的数据包发送一个确认**。
3. 回到发送端**处理传入的确认**，通知依赖模块哪个包被接收、哪个被丢弃。

**它自己不实现重传**，而是允许每个依赖模块**仅仅重传它决定重传的数据**。

**额外收益：不乱序处理。** 旧的数据包在新数据包之后到达时，DNM 会**假装这个包被丢弃并忽略它**。
原文说这防止了「更新的数据包中的新数据被包含在旧数据包中的过时数据意外覆盖」，
并自评「有点超出了 DNM 的目的，但这层实现这个功能非常常见和有效」。

### 二、发送侧：序列号不表示字节位置（§7.5.1）

**清单 7.1　使用序列号标记数据包**

```cpp
InFlightPacket* DeliveryNotificationManager::WriteSequenceNumber(
    OutputMemoryBitStream& inPacket)
{
    PacketSequenceNumber sequenceNumber = mNextOutgoingSequenceNumber++;
    inPacket.Write(sequenceNumber);

    ++mDispatchedPacketCount;

    mInFlightPackets.emplace_back(sequenceNumber);
    return &mInFlightPackets.back();
}
```

调用方先创建 `OutputMemoryBitStream`，调 `WriteSequenceNumber()` 写入并取出 `InFlightPacket*`，
**之后**才写负载并发送给目的主机。方法做三件事：写下下一个发出序列号并自增 →
`++mDispatchedPacketCount` → 把构造好的 `InFlightPacket` 追加进 `mInFlightPackets`
（跟踪所有尚未被确认的数据包，后面处理确认时要用）。

> **借 TCP 的技术，但不是 TCP 的语义**：原文明确说序列号是「从 TCP 中借用一个技术」，
> 但**不像 TCP 中那样表示流中的字节数**——只是简单地为每个传输的数据包提供一个唯一标识符。

**§7.5.1 注释（工程细节，逐条）**：

- `PacketSequenceNumber` 是 `typedef`，目的是让你**很容易改变序列号的比特数**。
  本例是 `uint16_t`，但可以根据计划发送的数据量用更多或更少的比特。
- **目的是在生成序列号时使用尽可能少的比特**，并最小化「遇到之前回绕生成的
  有相近序列号的数据包」的概率。
- **开发过程中包含一个展开的 32 位序列号，对于调试和验证都是非常有用的；
  接着在发布版本时，删除多余的序列号。**

### 三、接收侧：三路判包（§7.5.2）

**清单 7.2　处理传入的序列号**

```cpp
bool DeliveryNotificationManager::ProcessSequenceNumber(
    InputMemoryBitStream& inPacket)
{
    PacketSequenceNumber    sequenceNumber;

    inPacket.Read(sequenceNumber);
    if(sequenceNumber == mNextExpectedSequenceNumber)
    {
        //is this expected? add ack to the pending list and process packet
        mNextExpectedSequenceNumber = sequenceNumber + 1;
        AddPendingAck(sequenceNumber);
        return true;
    }

    //is sequence number < current expected? Then silently drop old packet.
    else if(sequenceNumber < mNextExpectedSequenceNumber)
    {
        return false;
    }

    //otherwise, we missed some packets
    else if(sequenceNumber > mNextExpectedSequenceNumber)
    {
        //consider all skipped packets as dropped, so
        //our next expected packet comes after this one ...
        mNextExpectedSequenceNumber = sequenceNumber + 1;
        //add an ack for the packet and process it
        //when the sender detects break it acks, it can resend
        AddPendingAck(sequenceNumber);
        return true;
    }
}
```

返回 bool，指明该包应该被处理还是被完全忽略——**这就是 DNM 防止乱序处理的方式**。
`mNextExpectedSequenceNumber` 记录**这个接收端自己**下一个该收到的序列号。
因为每个传输的数据包都有连续递增的序列号，接收主机很容易预测来包应该是什么序号。

| 来包序号 | 原文的判断 | 处理 | 确认 |
|---|---|---|---|
| **== 期望值** | 正好接上 | 处理；期望值 +1 | 是 |
| **< 期望值** | 可能比已到达的数据包老 | **静默丢弃，什么都不做** | **否** |
| **> 期望值** | 一个或多个包丢失或延迟 | **仍然处理**；期望值 = 该号 + 1 | 是 |

三条各自的理由，原文都写了：

- **< 期望值不确认**：为了避免乱序操作，主机不应该处理该数据包，**也不应该确认**，
  因为主机**仅仅确认处理的数据包**。
- **> 期望值仍处理**：原文在这里划了界——
  **「与 TCP 不同，DeliveryNotificationManager 没有承诺按序处理每个单独的数据包。
  它仅仅承诺不乱序处理，并且当数据包丢失时发出报告。」**
  所以确认和处理在丢失包之后传入的包是完全安全的。
  为了避免处理旧的数据包，期望值应设为**最近接收数据包的序列号加 1**。
- **回绕的边界情况**（原文写在 `< 期望值` 那条里，容易漏）：
  如果当前期望值接近 `PacketSequenceNumber` 的最大值、而传入的序列号接近最小值，
  序列号**可能已经发生了回绕**。按发包速度和位宽，这种情况**有可能发生，也有可能不发生**；
  如果有，且两个值都说明很可能发生，**则与下一种情况（`> 期望值`）的处理方式相同**。

> **注释**：第一种情况和第三种情况**实际执行了完全相同的操作**。
> 它们在代码中分别被调用，是因为它们表明了不同的情况；
> 但在检查完 `sequenceNumber >= mNextExpectedSequenceNumber` 之后，**可以统一成一种情况**。

### 四、确认攒批：不为每个包单独发确认（§7.5.2）

`ProcessSequenceNumber()` **没有直接发送任何确认**，而是调 `AddPendingAck()` 跟踪起来。

**理由（原文）**：如果一台主机从另一台主机收到很多数据包，
**针对每个传入的数据包都发送一个确认是很低效的**。
**即使是 TCP，也允许每隔一个数据包才确认一次。**
多人游戏里，服务器可能需要给客户端发送几个 MTU 大小的数据包，
**在客户端给服务器发送反馈数据之前，服务器可能需要给客户端发送几个 MTU 大小的数据包**
——这种情况下**最好积累所有必要的确认**，放进客户端要发给服务器的下一个数据包。
DNM **可以积累几个非连续的确认**，用一个 `vector<AckRange> mPendingAcks` 跟踪并序列化。

**清单 7.3　添加一个待确认**（要点：末位能延续就延续，不能就新起一段）

```cpp
void DeliveryNotificationManager::AddPendingAck(
    PacketSequenceNumber inSequenceNumber)
{
    if(mPendingAcks.size() == 0 ||
        !mPendingAcks.back().ExtendIfShould(inSequenceNumber))
    {
        mPendingAcks.emplace_back(inSequenceNumber);
    }
}
```

**`AckRange`（清单 7.4）**：`mStart` 存储第一个要确认的序列号，`mCount` 记录要确认的数量。
**这样，仅仅当序列中发生中断时，才需要多个 `AckRange`。**
`ExtendIfShould()` 检查来号是否连续（`inSequenceNumber == mStart + mCount`）：
是则 `++mCount` 并返回 true，否则返回 false，调用方据此另起一段。

序列化（`Write` / `Read`）的顺序：**先写起点，再写 `hasCount`（`mCount > 1`），为真才写 `mCount - 1`**。
原文给的理由：

- **和直接序列化个数不同，这些方法考虑到许多游戏通常一次只确认一个数据包**，
  所以用「熵编码」来有效地序列化个数，**其期望值是 1**。
- **它们使用 8 位整数来序列化个数，假设从来不需要多余的 256 个确认。**
  **原文自评：「8 位对于这个计数已经很多了，所以这个位数还可以再少一些。」**

**清单 7.5　写待确认**

```cpp
void DeliveryNotificationManager::WritePendingAcks(
    OutputMemoryBitStream& inPacket)
{
    bool hasAcks = (mPendingAcks.size() > 0);
    inPacket.Write(hasAcks);
    if(hasAcks)
    {
        mPendingAcks.front().Write(inPacket);
        mPendingAcks.pop_front();
    }
}
```

接收端在**写完自己的序列号之后**调它。三个设计取舍，原文都给了理由：

- **先写一个单独的比特标记是否包括确认**——因为**数据包丢失是例外，而不是常规**，
  所以**通常只有一个待发送的 AckRange**。
- **只写一个** `AckRange`（`front()` 之后立刻 `pop_front()`）——
  **写所有待发送的确认，需要一个额外的标识符来记录 AckRange 的个数，从而增加数据包的大小。**
- **「如果你确定游戏中永远不需要一次确认多个数据包，那么你可以完全删掉多确认系统，
  为每个数据包节省一些比特。」**

原文最后给了一条设计取向：**研究游戏的传输模式有助于你设计一个系统在边界情况下足够灵活，
同时在平均情况下足够有效。**

### 五、原书的数字与工程建议（本篇逐条保留清单）

> 这张表是**覆盖自检**：原文里每个具体数字、每条「如果你…可以…」式的建议都必须在此出现。
> 缺一行就是净化失败。

| 原文位置 | 数字 / 建议 |
|---|---|
| §7.5.1 注释 | 序列号位宽**尽量少用比特**（减少回绕撞车） |
| §7.5.1 注释 | **开发期用展开的 32 位序列号便于调试与验证，发布版删掉** |
| §7.5.1 注释 | 本例 `uint16_t`；typedef 是为了方便改位宽 |
| §7.5.2 注释 | `== 期望` 与 `> 期望` 两分支操作相同，**可合并成一个** |
| §7.5.2 正文 | **序列号不表示字节数**，与 TCP 不可比 |
| §7.5.2 正文 | **只确认处理过的包**；不回退到更旧 |
| §7.5.2 | ack 个数用 **8 位**，**原文自评「还可以再少」** |
| §7.5.2 | 每个包**只带一个** `AckRange`，先写 1 bit 存在标志 |
| §7.5.2 | **永远不需要确认多包时可整个删掉多确认系统** |
| §7.5.2 | `mDispatchedPacketCount` 在 `WriteSequenceNumber()` 里累加 |

## 作者省略的前提

> [!note] 本节是补出来的，不在原文里。

- **序列号位宽为什么是安全性问题而不只是省字节**（原文只说「尽可能少用比特」）：
  接收端只有一个「下一个期望值」做三路比较，**窗口外的序列号无法被区分**——
  判不出「很早以前的旧包」还是「刚回绕的新包」。位宽越窄，回绕后撞上在途序号的窗口越小。
- **「< 期望值就丢弃」的代价原文没讲**：被丢的**不一定是重传的旧包**，
  也可能是**乱序先到的新包**——它本该把期望值往前推。所以这是**有意丢包**。
- **`WritePendingAcks()` 每包只发一个 `AckRange` 的后果原文没讲**：
  队列里若积累了多段，剩下的**要等下一个反向包**才能发。
  **单向流量停止时这些确认永远发不出去**，发送端只能靠超时判失败。

## 成立条件与失效边界

| 条件 | 满足则 | 不满足则 |
|---|---|---|
| 单向序号连续递增 | 接收端能用单个「期望值」三路判包 | 必须引入重排缓冲 |
| 接收窗口 ≪ 序列号位宽范围 | 回绕撞车概率可忽略 | 老序号伪装成新序号并被处理 |
| 双向都有流量 | 确认能被捎带出去 | 攒的确认发不出去，退化为超时判失败 |
| 依赖数据幂等或自带去重 ID | 重传安全 | **重复生效** → 见 [[7-2 UDP可靠传输-投递状态与重复处理]] |

## 与原文的出入

| 原文说 | 实际应为 | 依据 |
|---|---|---|
| §7.5.2 正文：序号回绕时「与下一种情况的处理方式相同」（即按 `>` 处理并确认） | 清单 7.2 是 `else if(sequenceNumber < mNextExpectedSequenceNumber) { return false; }`——**无条件丢弃，没有回绕分支** | 正文与代码清单**直接矛盾**。照抄代码实现不出正文承诺的语义，需自己补回绕判断 |
| 正文说 `mNextExpectedSequenceNumber` 记录「**目的主机**应该接收的下一个序列号」 | 记录的是**接收端自己**的期望值 | `ProcessSequenceNumber()` 运行在收包的那一端 |
| §7.5.2：「使用**熵编码**来有效地序列化个数，其期望值是 1」 | 实现是 **1 bit 标志 + 8 bit 定长**；严格说属可选字段的定长编码，**没有出现 Huffman/算术编码** | 作者自己的注释写的是 "let's assume you want to ack max of 8 bits" |

## 本项目落点

| 教材结论 | 落到哪 | 怎么落 |
|---|---|---|
| DNM 本体 | **不实现** | 本项目传输层是 **raw TCP（决策 D1）**，裸 UDP 已在 [[07-TCP的真实代价]] 里被否。这套记账层没有落地对象 |
| 「只承诺不乱序应用，不承诺 FIFO」 | 作为 TCP 的对照说明 | TCP **天然拿到 FIFO**——这是它的隐性收益之一，原文没把它算进代价 |
| 「确认缺失不等于丢包」 | 将来改走 UDP 时的必踩坑 | 见 [[7-2 UDP可靠传输-投递状态与重复处理]] |

**本次没有落点。** 本篇是**协议选型的对照材料**：
如果哪天为展示自定义传输层改走 UDP，工作量下限就在这三步里。

## 关联笔记

- [[计算机网络-MOC|计算机网络 领域MOC]]
- [[7-2 UDP可靠传输-投递状态与重复处理|UDP 可靠传输 · 投递状态与重复处理]]（本节的续篇）
- [[07-TCP的真实代价|TCP 的真实代价]]（§7.4，上游：为什么会有这一节 · 跨分区）
- [[2-1 UDP头部|UDP 头部]]（UDP 给了什么、没给什么）
