# epoll ET vs LT：内核就绪链表管理机制的全链路源码分析

> 核心问题：ET 和 LT 在源码层面到底哪里不同？就绪链表上的 fd 分别在什么时候、在哪个函数里、通过什么条件被删除？

---

## 一、先上结论

ET 和 LT 的**唤醒链路完全一致**（等待队列 + 硬件中断 + 软中断 + `ep_poll_callback` 回调 → 挂就绪链表）。唯一的区别在于**就绪链表上 fd 的删除位置和删除条件**：

| 维度 | ET | LT |
|:---|:---|:---|
| **删除函数** | `ep_send_events`（交付函数） | `ep_poll`（主循环） |
| **删除时机** | 交付完成后，当前帧的末尾 | 下一次 `epoll_wait` 调用时，下一帧的开头 |
| **触发条件** | 无条件删除（`if (events & EPOLLET)`） | `ep_item_poll` 检查发现不再就绪（`if (!revents)`） |
| **核心语义** | 只通知一次，用户负责读完 | 只要数据还在，就继续通知 |

---

## 二、关键数据结构：`eventpoll` 和就绪链表

```c
struct eventpoll {
    wait_queue_head_t wq;        // 等待队列：epoll_wait 时进程挂在这里
    struct list_head  rdlist;    // 就绪链表：ep_poll_callback 把就绪的 fd 挂到这里
    struct rb_root    rbr;       // 红黑树：epoll_ctl ADD 的 fd 长期存储在这里
};
```

**`rdlist`（就绪链表）是理解 ET/LT 区别的核心数据结构。** 所有被回调挂上去的 fd 都在这里等待交付给用户态。ET 和 LT 的区别，本质上就是对 `rdlist` 上节点的**删除时机和删除条件**不同。

---

## 三、回调函数：`ep_poll_callback`（ET 和 LT 完全相同）

```c
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
    struct epitem *epi = container_of(wait, struct epitem, wait);
    struct eventpoll *ep = epi->ep;

    // 1. O(1) 挂到就绪链表
    list_add_tail(&epi->rdllink, &ep->rdlist);

    // 2. 唤醒在 eventpoll 等待队列上睡眠的进程
    wake_up_locked(&ep->wq);

    return 0;
}
```

**这一步 ET 和 LT 完全一样。** 无论什么触发模式，数据到达后回调都执行相同的动作：把 fd 挂到 `rdlist`，唤醒等待进程。区别在后续的删除环节。

---

## 四、`epoll_wait` 的调用链

```
epoll_wait 系统调用
  │
  └→ ep_poll()                    // 主循环
       │
       ├─ 【LT 删除发生在这里】遍历 rdlist，重新 poll 每个 fd
       │    └─ 不再就绪的 LT fd → list_del_init 删除
       │
       ├─ rdlist 不为空 → 调用 ep_send_events()  // 交付函数
       │    └─ 【ET 删除发生在这里】交付后立即 list_del_init 删除
       │
       └─ rdlist 为空 → schedule() 睡眠等待
```

**核心要点**：`ep_send_events` 只是 `epoll_wait` 流程的最后一步。LT 的删除发生在它之前的 `ep_poll` 主循环中。

---

## 五、LT 删除的精确位置：`ep_poll` 中的预清理循环

```c
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
                   int maxevents, long timeout)
{
retry:
    // ================================================================
    // 【LT 删除的精确位置】
    // 每次 epoll_wait 被调用时，在进入交付之前，遍历就绪链表
    // 对链表上已有的每个 fd 重新调用 ep_item_poll 检查其就绪状态
    // ================================================================
    list_for_each_entry_safe(epi, tmp, &ep->rdlist, rdllink) {
        struct poll_table pt;
        init_poll_funcptr(&pt, NULL);
        
        // 重新 poll 这个 fd：检查 socket 接收缓冲区是否还有数据
        // 返回 0：数据已被用户读完，fd 不再就绪
        // 返回非 0：数据还在，fd 仍然满足就绪条件
        int revents = ep_item_poll(epi, &pt);
        
        if (!revents) {
            // LT 事件在这里被删除！
            // 原因：上次 epoll_wait 返回后，用户已经调用 read() 把数据读完了
            // 内核重新检查发现这个 fd 不再满足 EPOLLIN 条件 → 从链表移除
            list_del_init(&epi->rdllink);
        }
        // revents 非零 → 数据还在，保留在链表上，本次仍会交付给用户
    }

    // ========== 交付阶段 ==========
    if (!list_empty(&ep->rdlist)) {
        // 链表上的 LT fd 都是经过上述检查、确认仍然就绪的
        ep_send_events(ep, events, maxevents);  // 交付函数（ET 删除在这里面）
        return event_count;
    }

    // ========== 睡眠等待 ==========
    // 链表空 → schedule() 让出 CPU
    goto retry;
}
```

**`ep_item_poll` 做了什么**：

```c
static int ep_item_poll(struct epitem *epi, struct poll_table *pt)
{
    // 调用 socket 底层文件的 poll 方法
    // 对于 TCP socket，poll 检查接收缓冲区是否非空
    return epi->ffd.file->f_op->poll(epi->ffd.file, pt);
}
```

和 `select`/`poll` 检查 fd 就绪状态用的是**同一个 `poll` 方法**。区别在于：`select`/`poll` 每次调用时遍历全量 fd 去调这个方法；而 `epoll` 只在 LT 清理阶段对**已经挂在 `rdlist` 上的 fd** 调这个方法，数量远小于全量 fd。

---

## 六、ET 删除的精确位置：`ep_send_events` 中

```c
static int ep_send_events(struct eventpoll *ep, struct epoll_event __user *events, int maxevents)
{
    int event_count = 0;
    struct epitem *epi;

    // 只遍历就绪链表，长度 = k（就绪 fd 数量），O(k)
    list_for_each_entry(epi, &ep->rdlist, rdllink) {
        struct epoll_event event;
        event.events = epi->event.events;
        event.data = epi->event.data;

        // 单个事件拷贝给用户态
        if (copy_to_user(&events[event_count], &event, sizeof(event)))
            break;

        event_count++;
        if (event_count >= maxevents)
            break;

        // ================================================================
        // 【ET 删除的精确位置】
        // ET 模式：交付后直接从就绪链表删除，下次不重复通知
        // LT 模式：不删除，保留在链表上
        // ================================================================
        if (epi->event.events & EPOLLET) {
            list_del_init(&epi->rdllink);
        }
        // LT：不在这里删除，留给下一次 ep_poll 的预清理循环处理
    }

    return event_count;
}
```

**为什么 ET 可以在这里直接删除？** 因为 ET 的语义是“只通知一次”。交付完成 = 通知使命结束。如果后续还有新数据到达，会再次触发 `ep_poll_callback`，重新挂回链表。

**为什么 LT 不在这里删除？** 因为 LT 的语义是“只要数据还在，就继续通知”。删除的决策权不属于交付函数，而属于下一次 `ep_poll` 中的状态检查。如果这里删了，LT 就退化成了 ET。

---

## 七、LT fd 完整生命周期（用用户态代码走一遍）

假设 `conn_fd` 以 LT 模式注册，客户端发来数据：

```
═══════════ 第一次 epoll_wait ═══════════

1. 数据到达 → ep_poll_callback 被调用
   └─ list_add_tail 把 conn_fd 挂到 rdlist
   └─ wake_up_locked 唤醒 main 进程

2. ep_poll 中的预清理循环:
   └─ ep_item_poll(conn_fd) → 返回非 0（数据还在）
   └─ 不删除，保留在 rdlist

3. ep_send_events 交付:
   └─ 遍历 rdlist → 找到 conn_fd → copy_to_user → LT 不删除

4. epoll_wait 返回，nfds > 0

5. 用户态 for 循环:
   └─ read(conn_fd) → 读了一半数据，剩下一半还在缓冲区

═══════════ 第二次 epoll_wait ═══════════

6. ep_poll 中的预清理循环:
   └─ ep_item_poll(conn_fd) → 返回非 0（数据还剩一半！）
   └─ 不删除，保留在 rdlist

7. ep_send_events 交付:
   └─ 再次找到 conn_fd → copy_to_user → LT 不删除

8. epoll_wait 返回，nfds > 0

9. 用户态 for 循环:
   └─ read(conn_fd) → 这次读完了所有数据

═══════════ 第三次 epoll_wait ═══════════

10. ep_poll 中的预清理循环:
    └─ ep_item_poll(conn_fd) → 返回 0！（数据已空）
    └─ list_del_init(&epi->rdllink) ← LT 在这里被删除！

11. rdlist 为空 → schedule() 睡眠，等待新事件
```

**如果是 ET 模式**：步骤 3 中 `ep_send_events` 交付后直接 `list_del_init` 删除。步骤 5 中用户必须循环读到 `EAGAIN`，否则剩余数据永远留在缓冲区，直到新数据到达触发下一次回调。

---

## 八、ET 和 LT 源码级对比总结

| 维度 | ET | LT |
|:---|:---|:---|
| **回调函数** | `ep_poll_callback` | `ep_poll_callback`（完全相同） |
| **挂链表动作** | `list_add_tail` | `list_add_tail`（完全相同） |
| **删除函数** | `ep_send_events`（交付函数） | `ep_poll`（主循环） |
| **删除时机** | 交付后立即删除，当前帧末尾 | 下一次 `epoll_wait` 入口，下一帧开头 |
| **删除触发条件** | `if (epi->event.events & EPOLLET)` 无条件删 | `if (!ep_item_poll(epi, &pt))` 检查后删 |
| **删除检查机制** | 不检查，无条件删除 | 重新 poll fd，确认不再就绪才删 |
| **数据未读完的后果** | 剩余数据丢在缓冲区，不再通知 | 下次 `epoll_wait` 继续返回该 fd |
| **适用场景** | 海量连接，追求极限性能 | 一般并发，追求编程简单可靠 |

---

## 九、直观类比

**ET** 像快递柜的一次性取件码——收到通知去取快递，不管一次有没有取完，码用一次就失效。你还有包裹没拿？等下一次快递员放进新的包裹、触发新的通知。所以你必须一次拿完所有东西。

**LT** 像物业电子屏——只要快递柜里还有没取的包裹，每次路过电子屏都能看到提醒。你今天取了一半，明天路过还能看到。直到你全部取完，物业检查发现柜子空了，才不再显示你的名字。

---

## 十、一句话总结

ET 和 LT 在唤醒链路上完全一致。唯一的分岔口在**删除环节**：ET 在 `ep_send_events` 交付后立即通过 `list_del_init` 从就绪链表删除，只通知一次；LT 在 `ep_send_events` 中不删除，而是在下一次 `ep_poll` 的预清理循环中，通过 `ep_item_poll` 重新检查 fd 的就绪状态，发现数据已被用户处理完才从链表删除。**ET 是“交付即删”，LT 是“下次查了再删”。**