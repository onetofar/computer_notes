---
title: 7-2 UDP可靠传输-投递状态与重复处理
category: 网络
tags:
  - 教材精读
  - 网络
  - 格雷泽
  - UDP
  - 可靠性
difficulty: 进阶
source: "《网络多人游戏架构与编程》§7.5.3 接收确认并传递状态"
maturity: 完善
created: 2026-10-07
skills:
  - obsidian-markdown
---

> [!info] 核心结论
> **确认没回来 ≠ 包丢了。** 也可能是包到了、而「装着确认的那个包」丢了。
> 发送端**没有任何办法区分这两种情况**，只能报一个语义更弱的事实：**送达失败**。
> 因为序列号不复用、接收端也不去重，**重复处理的责任被完全推给上层依赖模块**——
> 每条可靠消息必须自带一个应用层 ID，让接收端能认出「这个我已经算过了」。

## 机制

### 一、确认是二值的，但它反映的不是它自己的事（§7.5.3）

一个在途包只有两种结局：确认在期限内到达（**送达成功**），或者期限内没到（**送达失败**）。

> **警告（原文）**：
>
> **确认的缺失并不真正表示数据包的丢失。** 数据可能已经成功到达，
> 但是**包含确认的数据包丢失了。原始发送数据的主机没有办法区分这两种情况。**
>
> **在 TCP 中，这不是一个问题，因为重传数据包使用与之前发送时相同的序列号。**
> 如果 TCP 模块收到重复的数据包，忽略它即可。
>
> **对于 DNM，不是这样的。因为丢失的数据包不一定被重传，所以每一个数据包都是唯一标识的，
> 序列号不会被重用。** 这意味着客户端模块可能根据丢失的确认来决定重传一些可靠数据，
> 同时接收端可能已经有这些数据了。在这种情况下，**由依赖模块唯一地识别数据本身，
> 以防止重复。** 例如如果 `ExplosionManager` 依赖 DNM 可靠地发送爆炸数据，
> 它应该唯一标识这个爆炸，**保证接收端不会意外地爆炸两次。**

### 二、`ProcessAcks()`：按序对账（清单 7.6）

**清单 7.6　处理确认**

```cpp
void DeliveryNotificationManager::ProcessAcks(
    InputMemoryBitStream& inPacket)
{
    bool hasAcks;
    inPacket.Read(hasAcks);

    if(hasAcks)
    {
        AckRange    ackRange;

        ackRange.Read(inPacket);
        //for each InFlightPacket with seq# < start, handle failure...
        PacketSequenceNumber nextAckdSequenceNumber =
            ackRange.GetStart();
        uint32_t onePastAckdSequenceNumber =
            nextAckdSequenceNumber + ackRange.GetCount();
        while(nextAckdSequenceNumber < onePastAckdSequenceNumber &&
            !mInFlightPackets.empty())
        {
            const auto& nextInFlightPacket = mInFlightPackets.front();
            //if the packet seq# < ack seq#, we didn't get an ack for it,
            //so it probably wasn't delivered
            PacketSequenceNumber nextInFlightPacketSequenceNumber =
                nextInFlightPacket.GetSequenceNumber();
            if(nextInFlightPacketSequenceNumber < nextAckdSequenceNumber)
            {
                //copy this so we can remove it before handling-
                //dependent modules shouldn't find it if seeing what's live
                auto copyOfInFlightPacket = nextInFlightPacket;
                mInFlightPackets.pop_front();
                HandlePacketDeliveryFailure(copyOfInFlightPacket);
            }
            else if(nextInFlightPacketSequenceNumber==
                nextAckdSequenceNumber)
            {
                HandlePacketDeliverySuccess(nextInFlightPacket);
                //received!
                mInFlightPackets.pop_front();
                ++nextAckdSequenceNumber;
            }
            else if(nextInFlightPacketSequenceNumber>
                nextAckdSequenceNumber)
            {
                //somehow part of this range was already removed
                //(maybe from timeout) check rest of range
                nextAckdSequenceNumber = nextInFlightPacketSequenceNumber;
            }
        }
    }
}
```

**为什么能只顺序扫、不必检查每个 `InFlightPacket`？** 原文给了理由链：

1. 确认应该被**按序接收**，所以该方法假设 `InFlightPacket` 中**序列号比给定范围小的数据包都被丢弃**，并报告投递失败；范围内的全部报告成功。
2. 「在任何一个时间都有相当多的数据包在传输，但幸运的是，**没有必要检查每个单独的 InFlightPacket**」。
3. 「因为新的 `InFlightPacket` 被添加到 `mInFlightPackets` **双端队列**中，
   **所有的 `InFlightPacket` 已经通过序列号排序**」——
   于是顺序遍历即可：在范围内第一个包之前的全报丢失，
   发现第一个之后报成功，**剩余全部报成功并退出，不必再检查其他 `InFlightPacket`**。

**最后一个 `else if` 处理的是边界情况**（原文专门解释）：
**第一个已知的 `InFlightPacket` 在 `AckRange` 中，但不是在最前面的那个。**
原文接着回答了一个自然的疑问——**「之前被报告为丢失的数据包之后是如何被确认的？」**
答案是：**如果确认花费了很长时间才到达，就会发生这种情况**，
正如 TCP 中确认不及时时重传数据包一样。

### 三、超时对账：顺序扫队头（清单 7.7）

**流量稀疏时这个问题特别突出**（原文）：
可能**没有不连续的确认来单独标识一个数据包**，导致该数据包被报告丢失。
所以除了对账确认，还要**寻找超时的确认**——
主机应用程序应该**每帧调用** `ProcessTimedOutPackets()`。

**清单 7.7　处理超时数据包**

```cpp
void DeliveryNotificationManager::ProcessTimedOutPackets()
{
    uint64_t timeoutTime = Timing::sInstance.GetTimeMS() - kAckTimeout;
    while( !mInFlightPackets.empty())
    {
        //packets are sorted, so all timed out packets must be at front
        const auto& nextInFlightPacket = mInFlightPackets.front();

        if(nextInFlightPacket.GetTimeDispatched() < timeoutTime)
        {
            HandlePacketDeliveryFailure(nextInFlightPacket);
            mInFlightPackets.pop_front();
        }
        else
        {
            //no packets beyond could be timed out
            break;
        }
    }
}
```

`GetTimeDispatched()` 返回 `InFlightPacket` **创建时在构造函数中设置的时间戳**。
因为队列是排好序的，该方法**只需要检查列表前面，直到发现一个没有超时的数据包**；
在其之后，**可以保证传输中所有后续的数据包都没有超时**，于是 `break`。

> 原文**没有给 `kAckTimeout` 的具体数量级**。它决定「慢确认」与「真丢失」的区分精度：
> 太小则好包被误判，太大则真丢包恢复变慢。**别照抄经验值，按自己的 RTT 实测定。**

### 四、状态跟踪（清单 7.8）

```cpp
void DeliveryNotificationManager::HandlePacketDeliveryFailure(
    const InFlightPacket& inFlightPacket)
{
    ++mDroppedPacketCount;
    inFlightPacket.HandleDeliveryFailure(this);

}

void DeliveryNotificationManager::HandlePacketDeliverySuccess(
    const InFlightPacket& inFlightPacket)
{
    ++mDeliveredPacketCount;
    inFlightPacket.HandleDeliverySuccess(this);
}
```

这两个计数器不只是统计，原文给了两个用途：

- **跟踪数据包的投递速率，将来预测数据包丢失率。**
  **如果丢失率高，可以通知适当的模块降低传输率，或者模块直接通知用户
  主机的网络连接可能出错了。**
- **可以把这些值与 `mInFlightPackets` 容器的大小加起来，断言它们等于
  `WriteSequenceNumber()` 中增加的 `mDispatchedPacketCount`。**
  即 `送达 + 丢弃 + 在途 == 已发出`。

### 五、`TransmissionData`：可靠性的粒度在这里结束（清单 7.9）

每个 `InFlightPacket` 持有一个 `TransmissionData`（传输数据）指针的容器，
key 是模块自定的类型号。`TransmissionData` 是**抽象类**，有自己的
`HandleDeliverySuccess()` 和 `HandleDeliveryFailure()`。

```cpp
class InFlightPacket
{
public:
    void SetTransmissionData(int inKey, TransmissionDataPtr inTransmissionData)
    {
        mTransmissionDataMap[ inKey ] = inTransmissionData;
    }
    const TransmissionDataPtr GetTransmissionData(int inKey) const
    {
        auto it = mTransmissionDataMap.find(inKey);
        return (it != mTransmissionDataMap.end()) ? it->second: nullptr;
    }

    void HandleDeliveryFailure(
        DeliveryNotificationManager* inDeliveryNotificationManager) const
    {
        for(const auto& pair: mTransmissionDataMap)
            pair.second->HandleDeliveryFailure(inDeliveryNotificationManager);
    }
    void HandleDeliverySuccess(
        DeliveryNotificationManager* inDeliveryNotificationManager) const
    {
        for(const auto& pair: mTransmissionDataMap)
            pair.second->HandleDeliverySuccess(inDeliveryNotificationManager);
    }
private:
    PacketSequenceNumber mSequenceNumber;
    float mTimeDispatched;
    unordered_map<int, TransmissionDataPtr>  mTransmissionDataMap;
};
```

**谁来决定重传**：每个依赖模块创建自己的 `TransmissionData` 子类，
写可靠数据时把实例 `SetTransmissionData()` 挂进包。
**当 DNM 通知投递成功还是失败时，模块记录了给定数据包中存储了什么，
这样能够允许它做最好的处理**：可以**根据需要重传某些数据，发送数据的最新版本，
或者在应用程序的某个位置更新自定义变量**。
原文结论：**通过这种方式，DNM 提供了建立基于 UDP 的可靠系统的坚实基础。**

> **小窍门（原文）**：
> 为了便于说明，把传输数据映射保存在 `unordered_map` 中**非常清晰、有用**。
> **但在 `unordered_map` 中迭代不是高效的，并且可能导致许多高速缓存缺失的情况。**
> 在产品中：**如果传输数据的类型少**，更好的方式是**为每个类型配一个专用的成员变量，
> 或者存储在固定数组中，每个类型一个专用的索引**；
> **如果需要更多的传输数据类型**，那么值得将它们**存储在排序向量（`vector`）中**。

> **注释（原文，架构级前提）**：
> **每对通信的主机都需要一对自己的 `DeliveryNotificationManager`。**
> 所以在客户端-服务器拓扑中，**如果服务器同时与 10 个客户端通信，
> 那么它需要 10 个 `DeliveryNotificationManager`，每个客户端一个。**
> 然后每个客户端主机使用自己的 DNM 与服务器通信。

### 六、原书的数字与工程建议（本篇逐条保留清单）

> 覆盖自检：原文里每个具体数字、每条工程建议都必须在此出现。缺一行就是净化失败。

| 原文位置 | 数字 / 建议 |
|---|---|
| §7.5.3 警告 | **确认缺失 ≠ 包丢失**；TCP 靠**重传复用同一序列号**免费去重 |
| §7.5.3 警告 | 序列号**不会被重用** → 重复处理责任在依赖模块（`ExplosionManager` 例子） |
| §7.5.3 正文 | **每帧调用** `ProcessTimedOutPackets()` |
| §7.5.3 正文 | 判据是 `GetTimeDispatched() < now - kAckTimeout`；有序 → 遇第一个未超时即 `break` |
| §7.5.3 正文 | 断言：`送达 + 丢弃 + 在途 == mDispatchedPacketCount` |
| §7.5.3 正文 | 丢包率高 → **通知模块降低传输率，或通知用户网络可能出错** |
| §7.5.3 小窍门 | `unordered_map` 迭代**缓存缺失**；类型少 → 专用成员变量 / 定长数组；类型多 → 排序 `vector` |
| §7.5.3 注释 | **每对通信主机一对 DNM**；服务器对 10 个客户端 = **10 个 DNM** |
| §7.5.3 正文 | `kAckTimeout` **原文未给数量级**，需按自身 RTT 实测 |

## 作者省略的前提

> [!note] 本节是补出来的，不在原文里。

- **原文说在途队列「因为新的被加到队尾，所以已经按序列号排序」——这个因果是跳步的。**
  `push_back` 只保证「**入队顺序 = 分配序号顺序**」，前提是分配与入队之间没有被打断。
  §7.6（L6487）恰恰改成「往周期性提供的待发包里补写数据」，那时两者**解耦**，
  排序不再自动成立，必须显式重排。**这直接决定 `ProcessTimedOutPackets()` 的 `break` 是否正确。**
- **`HandlePacketDeliveryFailure` 为什么先 `pop_front` 再回调**（原文只有一句注释）：
  是**重入安全**约定——回调里极可能重传，重传会 `push_back`；元素若还在队列里，
  上层遍历看到的队列状态与预期不符。**注意成功路径没有同样的保护。**
- **「由依赖模块唯一地识别数据本身」只给了例子，没说怎么做到**：
  每条可靠消息自带**应用层 ID**（爆炸编号 / 事件序号），接收端维护已处理 ID 集合去重。
  关键约束：该 ID **必须能从 `TransmissionData` 里重建出来**，否则重传时构造不出同一个 ID。
- **`mInFlightPackets` 原文没有任何上限**：持续丢包 / 对端不回应时在途队列**无界增长**
  （每个元素还持有 `TransmissionData`）→ 内存涨到 OOM。生产实现需设上限并触发背压。

## 成立条件与失效边界

| 条件 | 满足则 | 不满足则 |
|---|---|---|
| 可靠消息**幂等** | 重传无副作用 | 必须额外去重，否则**状态翻倍**（爆炸两次、扣血两次） |
| 依赖模块实现了 `TransmissionData` | 能拿到送达/失败通知 | 拿不到任何通知，只能自己另判 |
| `kAckTimeout` ≫ 单程延迟 + 抖动 | 好包不会被误判 | 抖动一次就触发无谓重传 → 放大成重传风暴 |
| 在途队列保持按序号有序 | 超时扫描只需看队头 | `break` 会漏判后面已超时的包 → **通知永久漏发** |

## 与原文的出入

| 原文说 | 实际应为 | 依据 |
|---|---|---|
| §7.5.3 正文：「因为新的 `InFlightPacket` 被添加到队列中，**所有的已经通过序列号排序**」 | 这不是 `push_back` 的必然结果，而是「分配与入队严格同步」这一**前提**下的结果。§7.6 的改造会打破它 | 见 §7.6。**直接影响 `ProcessTimedOutPackets()` 里 `break` 的正确性** |
| §7.5.3 并列描述 `ProcessAcks()` 三条分支，没说清**包是否已移出队列** | 三条分支**都会**移除或移动游标：`<` 先拷贝后移除，`==` 回调后移除，`>` 只移动游标不移除 | 清单 7.6。**不移除的那条容易被读成「留着不动」** |

## 本项目落点

| 教材结论 | 落到哪 | 怎么落 |
|---|---|---|
| DNM 的送达/失败通知 | **不实现**（本项目是 raw TCP，决策 D1；确认不会丢，这机制没有存在理由） | —— |
| 「确认缺失 ≠ 丢失」这条**语义** | 作为 TCP 的**代价对照**保留 | TCP 消灭了整类「确认丢失」歧义，这正是它买来的东西；[[07-TCP的真实代价]] 讲的是它付出的代价 |
| 「重复处理的责任在调用方」 | 以另一种形式存在 | 本项目靠 [[04-格雷泽-全量快照与增量]] 的**全量快照幂等性**吸收重复/过时数据。**同一问题两条解法** |

**本次没有落点，但有反向对照价值**：本项目选 TCP 换到「不会重复、不会乱序」，
代价是队头阻塞与过时重传；DNM 反过来——自己拿到可控语义，代价是整套记账层自己写。

## 关联笔记

- [[计算机网络-MOC|计算机网络 领域MOC]]
- [[7-1 UDP可靠传输-传递通知与有序判定|UDP 可靠传输 · 传递通知与有序判定]]（前置：编号与确认攒批）
- [[07-TCP的真实代价|TCP 的真实代价]]（另一条路的代价清单 · 跨分区）
- [[04-格雷泽-全量快照与增量|全量快照与增量]]（幂等性如何吸收重复）
