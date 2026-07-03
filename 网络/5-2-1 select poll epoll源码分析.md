---
title: "5-2-1 select poll epoll 源码分析"
category: 计算机网络
tags:
  - net/io
  - networking
difficulty: 深入
source: "Linux高性能服务器编程"
link: ["[[5-2 IO复用相关技术|IO复用概念]]","[[5-2-2 epoll下LT和ET的具体差别|LT/ET模式]]"]
---
#### 性能差距
##### 一次调用的完整开销对比（三者横向）
**统一基准**：监听总 fd 数 = n，本次就绪 fd 数 = k；典型网络服务场景下 n >> k（大量长连接、低活跃度），这也是多路复用性能差异最显著的场景。
> 注：select 原生受 `FD_SETSIZE=1024` 硬编码限制，默认最大 n=1024；以下理论对比默认突破该限制的等价场景，保证三者在同量级下公平对比。

---

###### 环节1：fd 集合传入内核
| 接口     | 核心操作                        | 时间复杂度                  | 详细说明                                                                                 |
| ------ | --------------------------- | ---------------------- | ------------------------------------------------------------------------------------ |
| select | 将完整 `fd_set` 位图从用户态拷贝到内核    | O(n)（上限固定为 FD_SETSIZE） | 输入输出复用同一位图结构，内核直接在该位图上修改就绪标记。**额外隐性开销**：用户态每次调用前必须手动清零、重新设置监听位，否则上一轮的就绪标记会残留，导致结果错误。 |
| poll   | 将完整 `pollfd` 结构体数组从用户态拷贝到内核 | O(n)                   | `events`（监听事件）与 `revents`（就绪结果）字段分离，输入输出互不干扰；用户无需每次调用都重置数组，仅新增/删除 fd 时修改即可。          |
| epoll  | 无全量 fd 拷贝                   | O(1)                   | fd 集合已通过 `epoll_ctl` 持久化存储在内核红黑树中，`epoll_wait` 不需要重复传递全量 fd 信息。                      |

---

###### 环节2：内核态事件检测（核心性能鸿沟）
| 接口     | 核心操作                                    | 时间复杂度 | 详细说明                                                           |
| ------ | --------------------------------------- | ----- | -------------------------------------------------------------- |
| select | 内核遍历全部 n 个 fd，逐个调用对应文件驱动的 `poll` 接口检查状态 | O(n)  | 与 poll 底层逻辑完全一致，仅遍历载体不同（位图 vs 数组）；无论 fd 是否活跃、有没有事件，必须从头到尾逐个轮询。 |
| poll   | 内核遍历全部 n 个 fd，逐个调用对应文件驱动的 `poll` 接口检查状态 | O(n)  | 与 select 无本质性能差异，同属「全量轮询」范式。                                   |
| epoll  | 直接读取就绪链表，无需遍历全量 fd                      | O(k)  | fd 就绪时由驱动回调主动插入就绪链表，内核被动接收事件，无轮询开销；总连接数增长不影响检测效率。              |

>疑问：**单条数据结构的大小差异完全可以忽略**。哪怕 `pollfd` 有 8 字节、位图只占 1bit，真正吃掉 CPU 的是「10000 次驱动层函数调用」，而不是几百字节的内存拷贝。内存拷贝是纳秒级开销，驱动轮询是微秒级开销，二者差了三个数量级。

---

###### 环节3：等待队列挂接（最容易被忽略的隐藏开销）
进程休眠等待事件时，需要将自身挂到 fd 的等待队列上，事件触发时被唤醒；唤醒后再从队列中摘除。这一步涉及自旋锁操作与进程上下文处理，单步开销远高于纯内存拷贝。

| 接口     | 核心操作                                    | 时间复杂度 | 详细说明                                                    |
| ------ | --------------------------------------- | ----- | ------------------------------------------------------- |
| select | 遍历 n 个 fd，逐个将当前进程挂到每个 fd 的等待队列；唤醒后再逐个摘除 | O(n)  | 每次调用都要完成「全量挂接 → 休眠 → 全量摘除」的完整流程；fd 越多，锁操作的累计开销越夸张。      |
| poll   | 遍历 n 个 fd，逐个将当前进程挂到每个 fd 的等待队列；唤醒后再逐个摘除 | O(n)  | 与 select 逻辑完全一致，无性能差异。                                  |
| epoll  | 进程仅挂到 epoll 实例自身的 1 个等待队列上              | O(1)  | fd 就绪时通过回调唤醒 epoll 等待队列，不需要逐个操作每个 fd 的等待队列；开销与总连接数完全无关。 |

---

###### 环节4：结果返回用户态
| 接口     | 核心操作                                 | 时间复杂度      | 详细说明                                                 |
| ------ | ------------------------------------ | ---------- | ---------------------------------------------------- |
| select | 将标记好就绪位的完整 `fd_set` 位图拷贝回用户态         | O(n)（上限固定） | 无论实际就绪多少个 fd，都必须返回完整位图；返回后原监听位被覆盖，下次调用必须全部重置。        |
| poll   | 将回填好 `revents` 的完整 `pollfd` 数组拷贝回用户态 | O(n)       | 每个元素的 `revents` 字段标记就绪状态，`events` 字段保留不变；用户下次调用无需重置。 |
| epoll  | 仅将就绪链表中的 k 个事件打包拷贝到用户态数组             | O(k)       | 拷贝量仅与活跃连接数正相关，与总监听数无关；返回数组前 k 个全部是有效就绪事件。            |

---

###### 环节5：用户态结果处理
| 接口 | 核心操作 | 时间复杂度 | 详细说明 |
|------|----------|------------|----------|
| select | 遍历所有监听的 fd，逐个调用 `FD_ISSET` 宏检查是否就绪 | O(n) | 必须遍历全部 n 个 fd 才能找出所有就绪项；位图结构无法直接定位就绪位置，只能挨个扫。 |
| poll | 遍历整个 `pollfd` 数组，逐个检查 `revents` 是否非零 | O(n) | 与 select 逻辑一致，必须全量遍历才能筛选出就绪 fd。 |
| epoll | 直接顺序处理返回的前 k 个事件，无需筛选 | O(k) | 无任何无效遍历，拿到即可直接处理业务逻辑。 |

---

##### 量化对比：10000 连接、10 个就绪的典型场景
| 开销环节 | select（n=10000, k=10） | poll（n=10000, k=10） | epoll（n=10000, k=10） |
|----------|------------------------|------------------------|------------------------|
| 入参拷贝 + 用户态重置 | 10000 bit 全量拷贝 + 全量位重置 | 10000 个结构体全量拷贝 | 0 |
| 内核态事件检测 | 10000 次驱动 poll 调用 | 10000 次驱动 poll 调用 | 10 次事件处理 |
| 等待队列挂接+摘除 | 10000 次挂接 + 10000 次摘除 | 10000 次挂接 + 10000 次摘除 | 1 次挂接 + 1 次摘除 |
| 结果回拷 | 10000 bit 全量回拷 | 10000 个结构体全量回拷 | 10 个事件回拷 |
| 用户态结果筛选 | 10000 次位检查 | 10000 次字段检查 | 10 次业务处理 |

---

##### 三者本质定位总结
1.  **select 与 poll 属于同一代技术，无代差**
    二者仅数据载体不同（位图 vs 结构体数组），核心的「全量轮询、全量拷贝、全量挂接」开销模型完全一致。poll 只是解决了 select 的 1024 硬编码限制、以及每次调用要重置位图的使用麻烦，没有解决任何核心性能瓶颈。相同 fd 数量下，二者性能几乎没有差别。

2.  **epoll 是范式级代差，不是优化级差异**
    epoll 通过「fd 集合内核持久化 + 回调驱动事件检测 + 增量返回就绪事件」三层设计，把所有 O(n) 的开销全部降为了 O(k) 或 O(1)。在「大量长连接、低活跃度」的典型后端场景下，综合性能是 select/poll 的数百到数千倍，且总连接数越大，优势越夸张。

3.  **表层数据大小的差异可以忽略**
    很多人误以为“位图比数组小所以 select 更快”，实际上单次内存拷贝的开销在纳秒级，真正吃掉 CPU 的是**驱动轮询调用**和**等待队列锁操作**，这两项 select 和 poll 完全站在同一起跑线上，和 epoll 有数量级差距。
---
### 源码与内核回调过程对比
>主要看回调,事件发生后怎么回到用户态的

过程:
```
==================== 上游公共链路（三者100%一致）====================
【硬件层】网卡收到数据包
    ↓ ① DMA直写内存 + 触发硬中断
【驱动层】网卡驱动中断处理 → NAPI软中断收包
    ↓ ② 数据包从环形缓冲区递交协议栈
【协议栈层】IP层 → TCP层校验处理 → 写入对应socket接收缓冲区
    ↓ ③ 标准动作：唤醒该 socket 的等待队列
====================================================================
                         ↓ 从此处开始分化
┌─────────────────────┬─────────────────────┬─────────────────────┐
│      select         │        poll         │       epoll         │
├─────────────────────┼─────────────────────┼─────────────────────┤
│  执行poll_wait注册  │  执行poll_wait注册  │  执行ep_poll_callback│
│  的临时回调          │  的临时回调          │  永久回调            │
│  ↓                  │  ↓                  │  ↓                  │
│  标记该fd为就绪     │  标记该fd为就绪     │  将epitem插入就绪链表│
│  ↓                  │  ↓                  │  ↓                  │
│  唤醒当前进程       │  唤醒当前进程       │  唤醒epoll等待队列   │
│  ↓                  │  ↓                  │  ↓                  │
│  进程醒来           │  进程醒来           │  进程醒来           │
│  不知道哪个fd就绪   │  不知道哪个fd就绪   │  就绪链表全是就绪fd  │
│  ↓                  │  ↓                  │  ↓                  │
│  遍历全部n个fd      │  遍历全部n个fd      │  遍历k个就绪节点     │
│  逐个poll复查确认   │  逐个poll复查确认   │  直接打包返回        │
│  ↓                  │  ↓                  │                     │
│  从所有fd等待队列   │  从所有fd等待队列   │  无需逐个摘除回调    │
│  全部摘除进程       │  全部摘除进程       │  仅退出epoll等待队列 │
│  ↓                  │  ↓                  │  ↓                  │
│  全量位图拷回用户态 │  全量数组拷回用户态 │  仅就绪事件拷回用户态│
└─────────────────────┴─────────────────────┴─────────────────────┘
```
#### 一、select 内核核心：`do_select` 注释版
```c
// select 内核主逻辑：do_select
// 入参：n = 最大fd编号+1；inp/outp/exp 分别为读/写/异常三类监听位图
int do_select(int n, fd_set *inp, fd_set *outp, fd_set *exp)
{
    fd_set res_in, res_out, res_ex;   // 内核侧结果位图副本
    int retval = 0;

    // ========== 阶段1：用户态 → 内核态 全量拷贝 ==========
    // 【无状态特征】每次调用都完整拷贝全量fd集合，内核不保留任何历史信息
    // 下次调用必须重新传一遍所有监听fd
    copy_from_user(&res_in, inp, sizeof(fd_set));
    copy_from_user(&res_out, outp, sizeof(fd_set));
    copy_from_user(&res_ex, exp, sizeof(fd_set));

    // ========== 阶段2：第一轮全量遍历：检查状态 + 挂等待队列 ==========
    // 【O(n) 开销1】逐个fd轮询，总监听多少个就要查多少个
    for (int fd = 0; fd < n; fd++) {
        if (!FD_ISSET(fd, &res_in) && !FD_ISSET(fd, &res_out) && !FD_ISSET(fd, &res_ex))
            continue; // 未监听的fd跳过

        struct file *file = fget(fd);
        // 调用对应文件驱动的poll方法，检查当前是否就绪
        unsigned int mask = file->f_op->poll(file, &wait);
        
        // 【O(n) 开销2】逐个把当前进程挂到每个fd的等待队列上
        // 原因：不知道哪个fd会先就绪，必须全量挂载才能保证任意一个就绪都能唤醒自己
        poll_wait(file, &wait_queue, ...);

        // 标记就绪状态
        if (mask & POLLIN)  FD_SET(fd, &res_in);
        if (mask & POLLOUT) FD_SET(fd, &res_out);
        retval++;
    }

    // ========== 阶段3：无就绪事件则进程休眠 ==========
    if (retval == 0) {
        schedule(); // 让出CPU，挂起到等待队列
    }

    // ========== 阶段4：第二轮全量遍历：唤醒后复查 ==========
    // 【核心缺陷】唤醒后只知道"有fd就绪了"，但不知道具体是哪一个
    // 必须把所有fd再遍历一遍，逐个确认状态
    retval = 0;
    for (int fd = 0; fd < n; fd++) {
        if (!FD_ISSET(fd, inp) && !FD_ISSET(fd, outp) && !FD_ISSET(fd, exp))
            continue;

        struct file *file = fget(fd);
        unsigned int mask = file->f_op->poll(file, NULL);
        
        // 回填最终就绪结果
        if (mask & POLLIN)  FD_SET(fd, &res_in), retval++;
        if (mask & POLLOUT) FD_SET(fd, &res_out), retval++;
    }

    // ========== 阶段5：全量摘除等待队列 ==========
    // 【O(n) 开销3】每个fd的等待队列都要操作一次自旋锁，隐藏开销极大
    for (每个监听的fd) {
        remove_wait_queue(...);
    }

    // ========== 阶段6：内核态 → 用户态 全量回拷 ==========
    // 哪怕只有1个fd就绪，也要把完整位图全部拷回用户态
    copy_to_user(inp, &res_in, sizeof(fd_set));
    copy_to_user(outp, &res_out, sizeof(fd_set));
    copy_to_user(exp, &res_ex, sizeof(fd_set));

    return retval;
}
```

**poll_wait 到底做了什么**

`poll_wait(file, wait, ...)` 的本质只有一件事：

> 把当前进程对应的等待节点 `wait`，添加到目标文件 `file` 自己的等待队列头里。

伪代码实现
```c
void poll_wait(struct file *filp, wait_queue_head_t *wq_head, wait_queue_entry_t *wait)
{
    if (!wq_head) return;

    // 核心：把进程的等待节点，挂到目标文件的等待队列头上
    add_wait_queue(wq_head, wait);
}
```

关键细节

1. **每个 fd 对应一次挂载**
    
    你监听 n 个 fd，就要调用 n 次 `poll_wait`，把进程的等待节点分别挂到 n 个文件各自的队列头上。
    
    原因很朴素：每个 fd 的就绪是独立事件，你不知道哪个先触发，必须全量挂载才能保证任意一个就绪都能唤醒自己。
    
2. **不是同一个节点挂多个队列**
    
    一个链表节点只能属于一个链表。因此 select/poll 内部会维护一张 `poll_table` 表，每监听一个 fd 就分配一个独立的等待节点，分别挂到对应文件的队列上；所有节点的唤醒回调都指向同一个目标：唤醒当前进程。
---

#### 二、poll 内核核心：`do_poll` 注释版
poll 和 select 是**完全同源的执行模型**，仅数据载体从「位图」换成了「pollfd 结构体数组」，核心开销、遍历逻辑、性能瓶颈 100% 一致。

```c
// poll 内核主逻辑：do_poll
// 入参：ufds = 用户态pollfd数组首地址；nfds = 数组长度
int do_poll(struct pollfd *ufds, unsigned int nfds)
{
    struct pollfd *kfds; // 内核侧数组副本
    int retval = 0;

    // ========== 阶段1：全量拷贝数组到内核 ==========
    // 和select完全一致：无状态，每次调用全量传入，用完即丢
    kfds = kmalloc(nfds * sizeof(struct pollfd), GFP_KERNEL);
    copy_from_user(kfds, ufds, nfds * sizeof(struct pollfd));

    // ========== 阶段2：第一轮全量遍历：检查 + 挂队列 ==========
    // 逻辑和select完全相同，只是遍历对象从位图变成了数组
    for (int i = 0; i < nfds; i++) {
        int fd = kfds[i].fd;
        struct file *file = fget(fd);

        unsigned int mask = file->f_op->poll(file, &wait);
        // 逐个挂等待队列，和select无任何差异
        poll_wait(file, &wait_queue, ...);

        // 回填revents就绪字段
        kfds[i].revents = mask & kfds[i].events;
        if (kfds[i].revents) retval++;
    }

    // ========== 阶段3：无就绪则休眠 ==========
    if (retval == 0) {
        schedule();
    }

    // ========== 阶段4：第二轮全量遍历：唤醒后复查 ==========
    // 和select同样的缺陷：不知道哪个fd唤醒的自己，必须全量复查
    retval = 0;
    for (int i = 0; i < nfds; i++) {
        int fd = kfds[i].fd;
        struct file *file = fget(fd);
        unsigned int mask = file->f_op->poll(file, NULL);

        kfds[i].revents = mask & kfds[i].events;
        if (kfds[i].revents) retval++;
    }

    // ========== 阶段5：全量摘除等待队列 ==========
    // 同样O(n)锁开销，和select无差异
    for (int i = 0; i < nfds; i++) {
        remove_wait_queue(...);
    }

    // ========== 阶段6：全量数组回拷用户态 ==========
    copy_to_user(ufds, kfds, nfds * sizeof(struct pollfd));
    kfree(kfds);

    return retval;
}
```

> 一句话总结 poll 与 select 的关系：**换汤不换药**。poll 只是解决了 select 的 1024 硬限制、不用每次重置位图，核心性能模型没有任何改变。

---

#### 三、epoll 核心函数注释版
epoll 拆成两个核心部分：**就绪回调函数**（事件驱动的核心）+ **事件交付函数**（对应 epoll_wait 的主逻辑）。

##### 1. 就绪回调：`ep_poll_callback`
fd 就绪时由 socket 等待队列自动触发，是「事件驱动、无需轮询」的根源。
```c
// epoll 就绪回调：ep_poll_callback
// 触发时机：fd状态变化（收包、可写等）时，驱动自动调用
static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync, void *key)
{
    // 通过容器宏拿到当前fd对应的epitem对象
    struct epitem *epi = container_of(wait, struct epitem, wait);
    struct eventpoll *ep = epi->ep;

    // 【核心动作1】把自身直接插入就绪链表
    // 精准知道"我这个fd就绪了"，直接入队，不用别人来查
    list_add_tail(&epi->rdllink, &ep->rdlist);

    // 【核心动作2】唤醒epoll等待队列上的进程
    wake_up_locked(&ep->wq);

    return 0;
}
```

##### 2. 事件交付：`ep_send_events`
对应 `epoll_wait` 的核心逻辑，只处理就绪链表中的事件。
```c
// epoll 事件交付主逻辑：ep_send_events
// 入参：events = 用户态结果数组；maxevents = 最大返回数量
static int ep_send_events(struct eventpoll *ep, struct epoll_event __user *events, int maxevents)
{
    int event_count = 0;
    struct epitem *epi;

    // ========== 只遍历就绪链表，长度 = k（就绪fd数量） ==========
    // 【O(k) 核心优势】完全不碰红黑树，不遍历全量fd
    // 总连接数从100涨到10万，这里的开销完全不变
    list_for_each_entry(epi, &ep->rdlist, rdllink) {
        struct epoll_event event;

        // 打包成用户态约定的格式
        event.events = epi->event.events;
        event.data = epi->event.data;

        // 单个事件拷贝给用户态
        if (copy_to_user(&events[event_count], &event, sizeof(event)))
            break;

        event_count++;
        if (event_count >= maxevents)
            break;

        // ========== 触发模式处理 ==========
        // ET边缘触发：处理完直接从链表移除，下次不重复通知
        // LT水平触发：事件未耗尽则保留链表，下次继续返回
        if (epi->event.events & EPOLLET) {
            list_del_init(&epi->rdllink);
        }
    }

    return event_count;
}
```

##### ET和LT具体差异:[[5-2-2 epoll下LT和ET的具体差别]]
---
#### 四、三者核心逻辑横向对比表
| 核心维度      | select / poll          | epoll                             |
| --------- | ---------------------- | --------------------------------- |
| fd 集合生命周期 | 单次调用有效，每次全量拷贝，内核无状态    | 红黑树持久化存储，一次注册永久生效                 |
| 就绪检测方式    | 内核主动全量轮询，两次 O(n) 遍历    | fd 就绪通过回调主动上报，零轮询                 |
| 等待队列挂接    | 每次调用逐个挂接+逐个摘除，O(n) 锁开销 | `ADD` 时注册一次永久保留，仅 1 个 epoll 级等待队列 |
| 回调能力      | 仅唤醒进程，不记录具体是哪个 fd 就绪   | 唤醒 + 精准插入就绪链表，明确知晓就绪 fd 身份        |
| 唤醒后行为     | 必须二次全量遍历所有 fd 复查状态     | 直接读取就绪链表，无需复查                     |
| 结果返回量     | 全量位图/数组回拷，O(n) 内存拷贝    | 仅就绪事件回拷，O(k) 内存拷贝                 |
| 时间复杂度     | O(n)，开销随总连接数线性增长       | O(k)，开销仅与就绪连接数相关                  |
| 设计范式      | 轮询范式：内核主动挨个查           | 事件驱动范式：就绪主动上报                     |
