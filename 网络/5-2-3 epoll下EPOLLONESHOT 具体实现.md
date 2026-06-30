---
title: "5-2-3 epoll EPOLLONESHOT 实现"
category: 计算机网络
tags:
  - net/ip
  - networking
difficulty: 进阶
source: "Linux高性能服务器编程"
link: ["[[5-2-2 epoll下LT和ET的具体差别|LT/ET模式]]"]
---
# EPOLLONESHOT 源码级全链路分析

> 核心问题：`EPOLLONESHOT` 在内核中如何实现？它在哪些函数中生效？它能解决什么并发问题，不能解决什么？

---

## 一、EPOLLONESHOT 的设置

在 `epoll_ctl` 注册 fd 时指定：

```c
struct epoll_event ev;
ev.events = EPOLLIN | EPOLLONESHOT;  // 设置 EPOLLONESHOT
ev.data.fd = fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
```

`epoll_ctl` → 内核 `ep_insert`：

```c
epi->event.events = ev->events;  // 用户态 events 原样保存到 epitem
```

---

## 二、EPOLLONESHOT 的生效位置：`ep_poll_callback`

`EPOLLONESHOT` 的核心逻辑在回调函数中，不在交付函数中。

```c
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
    struct epitem *epi = container_of(wait, struct epitem, wait);
    struct eventpoll *ep = epi->ep;

    // ========== EPOLLONESHOT 检查 ==========
    if (epi->event.events & EPOLLONESHOT) {
        // 检查是否已被标记为"已触发"
        if (epi->event.events & EPOLL_DISABLED) {
            // ========== 第二次及后续回调：直接返回 ==========
            // fd 已被触发且尚未被用户重新激活
            // 不挂链表，不唤醒进程——防止重复通知
            return 0;
        }
        // ========== 第一次回调：标记为已触发 ==========
        epi->event.events |= EPOLL_DISABLED;
    }

    // 挂到就绪链表
    list_add_tail(&epi->rdllink, &ep->rdlist);

    // 唤醒等待队列上的所有进程
    wake_up_locked(&ep->wq);

    return 0;
}
```

**关键逻辑**：

1. **第一次数据到达**：`EPOLL_DISABLED` 未设置 → 设置 `EPOLL_DISABLED` → 挂链表 → 唤醒等待进程
2. **处理期间新数据到达**：`EPOLL_DISABLED` 已设置 → `return 0` → 不挂链表，不唤醒

**EPOLL_DISABLED 的职责**：防止 fd 在处理期间被**再次挂回链表**。它在回调层起作用，不是在交付层。

---

## 三、EPOLLONESHOT 在交付层的作用：无检查

`ep_send_events` 是 `epoll_wait` 的交付函数，负责把就绪链表上的事件拷贝给用户态。

```c
static int ep_send_events(struct eventpoll *ep, struct epoll_event __user *events, int maxevents)
{
    int event_count = 0;
    struct epitem *epi;

    // 遍历就绪链表
    list_for_each_entry(epi, &ep->rdlist, rdllink) {
        struct epoll_event event;
        event.events = epi->event.events;
        event.data = epi->event.data;

        // 拷贝给用户态
        if (copy_to_user(&events[event_count], &event, sizeof(event)))
            break;

        event_count++;
        if (event_count >= maxevents)
            break;

        // ET 删除
        if (epi->event.events & EPOLLET) {
            list_del_init(&epi->rdllink);
        }
        // ========== 没有检查 EPOLL_DISABLED！ ==========
        // ========== 没有检查 EPOLLONESHOT！ ==========
    }

    return event_count;
}
```

**关键结论**：`ep_send_events` 不关心 `EPOLL_DISABLED` 标志位。链表上有什么，它就交付什么。这意味着：

- **同一个 `wake_up` 唤醒的多个线程**：都能在 `ep_send_events` 中看到同一个 `epitem`，都能把它拷贝给用户态。`EPOLL_DISABLED` 不能阻止这个并发窗口。

---

## 四、EPOLLONESHOT 能解决什么、不能解决什么

### 场景一：同一个 wake_up 唤醒多个线程（EPOLLONESHOT 管不了）

```
1. 数据到达 → ep_poll_callback:
   ├─ 设置 EPOLL_DISABLED
   ├─ list_add_tail 挂 rdlist
   └─ wake_up_locked → 同时唤醒线程 A 和 B

2. 线程 A: ep_send_events → 遍历链表 → 拿到 conn_fd
3. 线程 B: ep_send_events → 遍历链表 → 也拿到 conn_fd
   ↑ EPOLL_DISABLED 管不了这一步！
   ↑ ep_send_events 不检查 EPOLL_DISABLED
```

### 场景二：处理期间新数据到达（EPOLLONESHOT 管住了）

```
4. 线程 A 正在处理 conn_fd（还没调用 epoll_ctl MOD）

5. 新数据到达 → ep_poll_callback:
   ├─ 检查 EPOLL_DISABLED → 已设置
   └─ return 0 ← 不挂链表，不唤醒新线程
   ↑ EPOLLONESHOT 在这里起作用！
```

### 对比表

| 并发场景 | 无 EPOLLONESHOT | 有 EPOLLONESHOT |
|:---|:---|:---|
| **同一 wake_up 唤醒的多线程同时拿到同一 fd** | 可能 | 可能（链表遍历无锁，交付层不检查 DISABLED） |
| **处理期间新数据到达，触发新回调** | 是，fd 被再次挂链表 | 否，`ep_poll_callback` 中 `EPOLL_DISABLED` 拦截 |
| **新回调唤醒新线程，分发同一 fd** | 是，多线程竞争 | 否，回调直接返回，不唤醒新线程 |
| **fd 被反复挂链表，被不同线程反复拿到** | 是 | 否 |

---

## 五、完整生命周期（源码跟踪）

### 5.1 用户态代码

```c
// 注册时设置 EPOLLONESHOT
ev.events = EPOLLIN | EPOLLONESHOT;
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// 等待事件
int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
for (int i = 0; i < nfds; i++) {
    int fd = events[i].data.fd;
    process(fd);  // 处理 fd
    // 处理完毕，重新激活
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}
```

### 5.2 内核源码跟踪

```
═══════════ 第一次数据到达 ═══════════

ep_poll_callback(fd):
  ├─ container_of → 定位 epitem
  ├─ epi->event.events & EPOLLONESHOT → 是
  ├─ epi->event.events & EPOLL_DISABLED → 否（首次触发）
  ├─ epi->event.events |= EPOLL_DISABLED   ← 标记为已触发
  ├─ list_add_tail(&epi->rdllink, &ep->rdlist)  ← 挂链表
  └─ wake_up_locked(&ep->wq)  ← 唤醒所有等待线程

═══════════ 线程 A 被唤醒 ═══════════

ep_poll():
  ├─ LT 清理循环...
  ├─ rdlist 不为空 → ep_send_events()
  │   └─ list_for_each_entry → 找到 conn_fd
  │       └─ copy_to_user → 交付给线程 A
  └─ 返回用户态

═══════════ 线程 A 处理期间，新数据到达 ═══════════

ep_poll_callback(fd):  ← 再次被调用！
  ├─ container_of → 定位 epitem
  ├─ epi->event.events & EPOLLONESHOT → 是
  ├─ epi->event.events & EPOLL_DISABLED → 是！已被标记
  └─ return 0  ← 直接返回！不挂链表，不唤醒！
      ↑ EPOLLONESHOT 的核心价值：防止处理期间重复通知

═══════════ 线程 A 处理完毕 ═══════════

epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev):
  └─ 内核: 清除 EPOLL_DISABLED，重新激活 fd

═══════════ 下一次数据到达 ═══════════

ep_poll_callback(fd):
  ├─ EPOLL_DISABLED 已被清除
  ├─ 正常挂链表
  └─ 正常唤醒
```

---

## 六、并发窗口的详细分析

### 6.1 为什么 ep_send_events 不检查 EPOLL_DISABLED？

**设计权衡**：内核把并发控制放在**入口**（回调层），而不是**出口**（交付层）。

- 回调层的检查是一次性的——`EPOLL_DISABLED` 一旦设置，后续所有回调都被拦截
- 如果交付层也检查，每个 fd 在每次交付时都要做额外判断，增加热路径开销
- 链表遍历无锁是性能选择——代价是同一个 `wake_up` 唤醒的多线程会竞争

### 6.2 多线程同时拿到同一个 fd 怎么办？

**用户态解决**：

```c
// 方案一：单线程 epoll_wait + 任务队列（推荐）
// 主线程只负责 epoll_wait，拿到 fd 后通过无锁队列分发给工作线程

// 方案二：EPOLLONESHOT + 用户态锁
for (int i = 0; i < nfds; i++) {
    int fd = events[i].data.fd;
    if (try_lock(fd)) {
        process(fd);
        unlock(fd);
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);  // 重新激活
    }
}
```

---

## 七、EPOLLONESHOT 与 ET/LT 的对照

| 维度 | ET | LT | EPOLLONESHOT |
|:---|:---|:---|:---|
| **生效位置** | `ep_send_events` | `ep_poll` | `ep_poll_callback` |
| **核心机制** | 交付后 `list_del_init` 删除 | `ep_item_poll` 检查后删除 | `EPOLL_DISABLED` 阻止再次挂链表 |
| **解决什么问题** | 只通知一次，不重复通知 | 只要数据还在就继续通知 | 防止处理期间 fd 被重新分发 |
| **并发保护** | 无 | 无 | 部分（防止二次通知，不防止同次竞争） |

---

## 八、设计哲学

EPOLLONESHOT 的设计哲学是**把"禁用"的时机提前到回调层**——在事件产生的入口就拦截，而不是在事件交付的出口拦截。这保证了 fd 在"被触发 → 被处理 → 被重新激活"的整个周期内，只会被挂一次链表，只会被交付一次（虽然可能被多个线程同时看到这一次交付）。

**它解决的是"事件重复产生"的问题，而不是"事件被多个消费者同时看到"的问题。** 后者的并发控制需要用户态配合。

---

## 九、一句话总结

`EPOLLONESHOT` 在 `ep_poll_callback` 中通过 `EPOLL_DISABLED` 标志位阻止 fd 在处理期间被再次挂回就绪链表，从而防止同一个 fd 因新数据到达而被反复通知、被不同线程反复拿到。但 `ep_send_events` 交付层不检查 `EPOLL_DISABLED`，所以它不能阻止同一个 `wake_up` 唤醒的多个线程在同一个交付周期内同时拿到同一个 fd。后者的并发控制需要用户态的锁机制或单线程 `epoll_wait` + 任务队列来完成。