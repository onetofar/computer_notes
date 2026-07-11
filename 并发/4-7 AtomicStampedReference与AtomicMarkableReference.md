---
title: "4-7 AtomicStampedReference 与 AtomicMarkableReference"
category: 并发编程
tags:
  - conc
difficulty: 进阶
source: "JCiP"
link: ["[[4-4 原子变量与非阻塞算法|原子变量与非阻塞算法]]"]
---
# 4-7 AtomicStampedReference 与 AtomicMarkableReference
（基于 JCiP / JDK 并发包，解决 ABA 问题核心工具）
## 一、核心概述
这两个类是 **JDK 为无锁 CAS 操作解决 ABA 问题**提供的官方实现，均为 `AtomicReference` 的增强类：
- 通过为**对象引用 + 附加标记**组成二元组，让 CAS 同时校验引用和标记
- 彻底避免「值被改回原值导致 CAS 误判」的 ABA 问题
- 仅用于**底层无锁数据结构**（如并发链表、队列），业务代码极少使用
## 二、解决 ABA 原理
1. 普通 `AtomicReference` 只校验**引用值**，无法感知中间修改
2. 增强类同时校验：**引用值 + 版本/标记**
3. 即使引用值变回原值，**标记不匹配则 CAS 直接失败**，杜绝 ABA
4. markable防不住：A→B→C→A、标记翻转两次复原的高阶 ABA 变种，会发生 false-》ture-》false两次翻转，无法判定
---

## 三、核心区别（简洁版）
| 特性     | AtomicStampedReference        | AtomicMarkableReference          |
| -------- | ----------------------------- | -------------------------------- |
| 附加标记 | `int stamp`（版本号，可递增） | `boolean mark`（标记位，二状态） |
| 作用     | 精确记录修改次数/版本         | 仅标记状态（如：已删除/未删除）  |
| 性能     | 常规                          | 更轻量                           |
| ABA 防护 | 完整、严格                    | 简化、够用                       |

---

## 四、核心 API（极简）
### 1. AtomicStampedReference
```java
// 构造：引用值 + 初始版本号
AtomicStampedReference<String> ref = new AtomicStampedReference<>("A", 1);

// CAS：必须 引用+版本 同时匹配
ref.compareAndSet("A", "B", 1, 2);
```

### 2. AtomicMarkableReference
```java
// 构造：引用值 + 初始标记
AtomicMarkableReference<String> ref = new AtomicMarkableReference<>("A", false);

// CAS：必须 引用+标记 同时匹配
ref.compareAndSet("A", "B", false, true);
```

---

## 五、标准适用场景
1. **AtomicStampedReference**
   - 需要**严格追踪版本/修改次数**的场景
   - 内存型乐观锁、强一致性无锁结构

2. **AtomicMarkableReference**
   - 无锁链表/队列 **节点标记删除**（JDK 标准用法）
   - 只需判断是否被修改，不关心修改次数
   - `ConcurrentLinkedQueue` 核心设计思想

---

## 六、总结
1. 两者**底层都是 CAS**，唯一区别是**附加标记类型**
2. `AtomicStampedReference` = 完整版版本号（强防护）
3. `AtomicMarkableReference` = 简化标记位（轻量高效）
4. 均为**底层框架优化工具**，不推荐普通业务开发使用