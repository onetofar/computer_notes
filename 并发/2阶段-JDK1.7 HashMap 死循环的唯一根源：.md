---
title: "JDK1.7 HashMap 死循环的唯一根源"
category: 并发编程
tags:
  - conc/container
  - concurrency
difficulty: 进阶
source: "自整理"
---
# **JDK1.7 HashMap 死循环的唯一根源**：
`transfer()` 节点转移方法使用 **头插法** + **多线程并发扩容** → 链表节点互相引用形成**环形闭环** → 遍历链表时无限循环。
## 一、前置基础（必须先懂）
1. **JDK1.7 HashMap 结构**
   数组 + 单向链表，哈希冲突时链表挂载
2. **扩容规则**
   元素数量 > 阈值 → 扩容为**原容量的2倍**
3. **核心扩容方法**
   `resize()` → 调用 `transfer()` **转移旧数组节点到新数组**
4. **头插法**
   新节点插入到链表**头部**，转移后链表顺序**倒置**
## 二、单线程扩容：正常流程（无问题）
### 1. JDK1.7 transfer() 源码（死循环元凶）
```java
// 旧数组转移到新数组：头插法
void transfer(Entry[] newTable, boolean rehash) {
    int newCapacity = newTable.length;
    // 遍历旧数组的每个桶
    for (Entry<K,V> e : table) {
        // 遍历链表：e是当前节点，next是下一个节点
        while(null != e) {
            Entry<K,V> next = e.next; // 【关键1】记录下一个节点
            if (rehash) {
                e.hash = null == e.key ? 0 : hash(e.key);
            }
            // 计算新数组下标
            int i = indexFor(e.hash, newCapacity);
            // 【关键2：头插法】当前节点指向新数组桶位的头节点
            e.next = newTable[i];
            // 当前节点放到新数组头部
            newTable[i] = e;
            // 处理下一个节点
            e = next;
        }
    }
}
```
### 2. 单线程转移示例（链表倒置）
旧数组桶位链表：`A → B → C`
转移后新数组链表：`C → B → A`（头插法倒置顺序）
✅ 单线程完全正常，无任何问题
---

## 三、多线程并发扩容：环形链表形成（核心推演）
### 场景设定
- 旧数组：1个桶位，链表 `A → B`
- **线程1** 和 **线程2** 同时触发扩容，同时执行 `transfer()`

---

### 第一步：两个线程同时执行到 关键代码行
```java
Entry<K,V> next = e.next; 
```
- 线程1：`e = A`，`next = B`
- 线程2：`e = A`，`next = B`
此时两个线程的**局部变量完全一致**

---

### 第二步：线程1 被CPU挂起（休眠）
线程1 只执行了：`next = e.next`，就暂停了
线程2 继续执行，**完成完整转移**

线程2 转移结果（新数组）：
`B → A`（头插法倒置）

---

### 第三步：线程1 恢复执行（灾难开始）
线程1 继续从暂停位置执行，此时它的**局部变量还是旧的**：
`e = A`，`next = B`

#### 线程1 执行第1轮循环：
1. `e.next = newTable[i];`
   A 的 next 指向新数组头节点 `B`
   → `A → B`
2. `newTable[i] = A;`
   新数组头节点变成 A
   → 新数组：`A → B`
3. `e = next;` → `e = B`

#### 线程1 执行第2轮循环：
1. `next = e.next;` → B 的 next 是 A（线程2转移时设置的）
   → `next = A`
2. `e.next = newTable[i];`
   B 的 next 指向新数组头节点 `A`
   → `B → A`
3. `newTable[i] = B;`
   新数组头节点变成 B
4. `e = next;` → `e = A`

#### 最终结果：**环形链表诞生！**
```
A → B → A → B → A ...
```
两个节点互相指向，形成**无限闭环**

---

## 四、死循环触发后果
当线程执行 `get()` / `put()` 遍历这个链表时：
```java
while(e != null) {
   e = e.next;
}
```
- e 永远不为 null
- 无限循环遍历
- CPU 使用率飙升至 100%，服务卡死

---

## 五、终极总结（一句话）
1. **JDK1.7 用头插法转移节点**，多线程并发时会让链表节点**互相引用**；
2. 形成**环形链表**后，遍历操作会触发**无限死循环**；
3. JDK1.8 修复了这个问题：改用**尾插法**，扩容不会倒置链表，**无死循环**。

---

### 极简记忆点
✅ JDK1.7 HashMap 死循环 = **transfer头插法 + 多线程并发扩容**
✅ 核心现象：**链表环形引用**
✅ 修复方案：JDK1.8 尾插法 / 使用 ConcurrentHashMap