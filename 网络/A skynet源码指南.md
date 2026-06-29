---
title: "skynet源码指南"
category: 常用工具
tags:
  - net/server
  - networking
difficulty: 深入
source: "自整理"
---
# Skynet 源码阅读指南 —— 一场 Actors 的漫游
> 这不是一份说明书。这是一份地图,带你穿过 skynet 的钢筋骨架,看清它是怎么把"成千上万个 lua 服务塞进一个进程,还能跑得飞快"的。
>
> 阅读姿势:打开这份指南,旁边开着源码,按章节顺序往下走。每个小节都会告诉你**该翻开哪个文件、看哪几行、看什么**。
## 0. 先建立心智模型
在读任何一行 C 代码之前,请先在脑子里装下这三个词:
- **Service(服务)**:skynet 的基本运行单位。本质上是一个 `skynet_context` + 一个消息队列 + 一段回调函数。你写的每一个 lua 文件,运行起来都是一个 service。**service 之间没有共享内存,只能发消息。**
- **Message(消息)**:服务间通信的唯一方式。一条消息 = `{source, session, data, sz}`,外加编码在 `sz` 高 8 位的 `type`。这是 actor 模型的硬核实现。
- **Handle(地址)**:每个 service 有一个 32 位整数地址,形如 `:0100000a`。前 8 位是 harbor(节点 id),后 24 位是本节点内的槽位。`.name` 是它的别名。
skynet 的全部精华就是:**如何高效地把消息从一个 service 的队列,送到另一个 service 的队列,再被某个 worker 线程捞出来执行回调。**
把这个画面刻在脑子里,后面所有的 C 代码都是在回答这一个问题。
## 1. 鸟瞰:目录与模块划分
先看一眼仓库结构,建立"东西都在哪"的索引:
```
skynet/
├── skynet-src/        ← C 内核:actor 引擎、调度、消息队列、定时器、socket
├── service-src/       ← C 写的内置服务:logger / gate / harbor / snlua
├── lualib-src/        ← 给 lua 用的 C 扩展:netpack、socket、cluster、multicast...
├── lualib/            ← lua 层框架:skynet.lua 是灵魂
├── service/           ← lua 写的内置服务:launcher、bootstrap、gate、debug_console...
├── 3rd/               ← lua、lpeg、jemalloc 等第三方
├── examples/          ← 示例 config 与 main.lua
├── test/              ← 各种功能测试 lua
└── Makefile / platform.mk
```
**两层世界观:**
| 层 | 语言 | 职责 | 代表文件 |
|---|---|---|---|
| 内核层 | C | 调度引擎、消息队列、定时器、socket、模块加载 | [skynet-src/](skynet-src/) |
| 框架层 | Lua | 把 C 的回调模型包装成"协程化"的同步调用 API | [lualib/skynet.lua](lualib/skynet.lua) |
绝大多数业务代码只跟 lua 层打交道。但**要理解 skynet 为什么快、为什么不会因为一个服务卡死而拖垮全局,你必须下到 C 层。** 这也是本指南的重点。
---

## 2. 启动之旅:从 `main` 到第一个服务跑起来

打开 [skynet-src/skynet_main.c](skynet-src/skynet_main.c)。这是 `main()` 所在地,干三件事:解析配置文件 → 填 `skynet_config` → 调 `skynet_start()`。

真正干活的是 [skynet-src/skynet_start.c](skynet-src/skynet_start.c) 里的 `skynet_start()`(第 265 行)。跟着它走一遍启动序列,这是理解整个运行时的钥匙:

```
skynet_start(config)
  ├── skynet_harbor_init()      // 初始化节点 id
  ├── skynet_handle_init()      // 初始化 handle 存储(服务地址表)
  ├── skynet_mq_init()          // 初始化全局消息队列
  ├── skynet_module_init()      // 初始化 C 模块加载器(dlopen)
  ├── skynet_timer_init()       // 初始化定时器(时间轮)
  ├── skynet_socket_init()      // 初始化 socket 引擎
  │
  ├── skynet_context_new(logservice, logger)   // 创建 logger 服务
  ├── skynet_handle_namehandle(..., "logger")  // 注册别名 .logger
  ├── bootstrap(...)            // 启动引导服务(通常是 snlua bootstrap)
  │
  └── start(config->thread)     // ★ 创建全部线程,进入主循环
```

`bootstrap()`(第 237 行)解析配置里的 `bootstrap = "snlua bootstrap"` 这一行,然后 `skynet_context_new("snlua", "bootstrap")` —— 也就是**用 `snlua` 这个 C 模块创建一个服务,参数是字符串 `"bootstrap"`**。这个字符串最终会成为 `service/bootstrap.lua` 的入口。

然后 `start(thread)`(第 186 行)创建 **thread+3** 个线程:

| 线程 | 函数 | 数量 | 职责 |
|---|---|---|---|
| monitor | `thread_monitor` | 1 | 每 5 秒巡视一次,抓死循环 |
| timer | `thread_timer` | 1 | 每 2.5ms 推进时间轮,触发定时消息 |
| socket | `thread_socket` | 1 | epoll/kqueue 单线程网络引擎 |
| worker | `thread_worker` | N(配置 thread) | 从全局队列抢消息执行 |

> **关键设计直觉**:网络 I/O、定时、业务调度是三条独立的线程,各自只管自己那一摊。worker 是多条,它们共享一个全局队列去抢活。这就是 skynet 并发模型的全部骨架。

到这里,引擎已经点火了。接下来我们一头扎进 worker 的工作循环——这是整个 skynet 跳动的心脏。

---

## 3. 心脏:Worker 调度循环

打开 [skynet-src/skynet_start.c:156](skynet-src/skynet_start.c#L156) 的 `thread_worker`:

```c
while (!m->quit) {
    q = skynet_context_message_dispatch(sm, q, weight);  // 抢一个队列,派发若干条消息
    if (q == NULL) {
        // 没活干了,睡觉,等 timer/socket 线程唤醒
        ++m->sleep;
        if (!m->quit) pthread_cond_wait(&m->cond, &m->mutex);
        --m->sleep;
    }
}
```

核心在 `skynet_context_message_dispatch()` —— [skynet-src/skynet_server.c:292](skynet-src/skynet_server.c#L292)。逐行品味这段,它是 skynet 调度哲学的浓缩:

```
1. 从全局队列 pop 一个服务队列 q  (没有就返回 NULL → worker 睡觉)
2. 通过 q->handle 抓取对应的 skynet_context (grab,引用计数 +1)
3. 从 q 里 pop 消息,pop 到的次数 n 取决于 weight:
       weight=-1 → 只 pop 1 条   (重量级,每次只处理一条就换队列)
       weight= 0 → pop 当前队列全部消息 (n = length)
       weight> 0 → pop length >> weight 条 (折中,防止一个繁忙队列饿死别人)
4. 对每条消息:dispatch_message(ctx, msg)  ← 真正执行服务回调
5. 把 q 重新 push 回全局队列尾部,然后 pop 下一个队列返回
```

**读懂这段,你就读懂了 skynet 的公平调度:**

- 没有哪个服务能独占 worker。即使它的队列里堆了一万条消息,worker 也只处理 `n` 条就把它丢回队尾,去服务下一个。
- `weight` 数组在 [skynet_start.c:213](skynet-src/skynet_start.c#L213):前 4 个 worker 是 `-1`(只处理 1 条),然后 `0`(全处理),再 `1/2/3`(按位移折中)。**前几个 worker 是"特种兵",保证总有线程在做短任务、快速周转。**
- 服务队列空了就 `in_global = 0`,从全局队列里摘掉,不再被调度;下次有新消息进来(`skynet_mq_push`)再挂回去。这叫"懒挂载"。

### 3.1 `dispatch_message` —— 回调的执行现场

[skynet-src/skynet_server.c:255](skynet-src/skynet_server.c#L255)。注意三件事:

1. `pthread_setspecific(G_NODE.handle_key, ctx->handle)` —— **把"当前正在服务的 handle"存进线程局部存储**。这样你在回调里调 `skynet_current_handle()` 就能知道"我是谁"。lua 层的 `skynet.self()` 依赖于此。
2. `ctx->cb(ctx, ctx->cb_ud, type, session, source, data, sz)` —— 调服务的回调。对 lua 服务,这个 cb 就是 `skynet.dispatch_message`(见后文)。
3. 返回值 `reserve_msg`:回调返回 0 表示"这条消息我处理完了,内核帮我 free 掉 data";返回非 0 表示"data 我自己接管了"(lua 层用这个机制避免无谓拷贝)。

### 3.2 死循环检测:Monitor 线程

打开 [skynet-src/skynet_monitor.c](skynet-src/skynet_monitor.c),只有 47 行,极简。

每个 worker 有一个 `skynet_monitor`。`dispatch_message` 执行前后各调一次 `skynet_monitor_trigger(sm, source, destination)`,它把当前 source/destination 记下,并把 `version` 自增。

`thread_monitor`([skynet_start.c:94](skynet-src/skynet_start.c#L94))每 5 秒巡视所有 monitor:如果发现某个 monitor 的 `version` 没变(说明这个 worker 5 秒都没换过消息),就判定**死循环**,给目标服务打 `endless` 标记并报错。

> 这就是 skynet 的容错底线:一个 lua 服务写了 `while true do end`,不会让整个进程假死——5 秒后被 monitor 抓出来,其他服务照跑。

---

## 4. 一个服务的一生:`skynet_context`

打开 [skynet-src/skynet_server.c:42](skynet-src/skynet_server.c#L42) 看结构定义:

```c
struct skynet_context {
    void * instance;            // C 模块的实例指针(对 snlua 就是 struct snlua*)
    struct skynet_module * mod; // 这个服务用的 C 模块(动态库)
    void * cb_ud;               // 回调的用户数据
    skynet_cb cb;               // 回调函数
    struct message_queue *queue;// 这个服务专属的消息队列
    ATOM_POINTER logfile;
    uint32_t handle;            // 服务地址
    int session_id;             // session 自增器(用于 call)
    ATOM_INT ref;               // 引用计数
    bool init, endless, profile;
    ...
};
```

**一个 service = 一段 C 模块逻辑 + 一个消息队列 + 一个回调。** 仅此而已。

### 4.1 服务的创建:`skynet_context_new`

[skynet-src/skynet_server.c:124](skynet-src/skynet_server.c#L124)。跟着读:

```
1. skynet_module_query(name)      // 查/dlopen 这个 C 模块,拿到 create/init/release 三个符号
2. skynet_module_instance_create  // 调 mod->create(),生成实例(对 snlua 是新建一个 lua_State)
3. malloc context, ref=2          // 引用计数=2:一个给 handle 表,一个给 init
4. skynet_handle_register(ctx)    // 在 handle 表里占个槽,拿到地址
5. skynet_mq_create(handle)       // 建消息队列
6. skynet_module_instance_init    // ★ 调 mod->init(inst, ctx, param) —— 服务的真正初始化
7. init 成功 → skynet_globalmq_push(queue)  // 把队列挂上全局队列,开始被调度
   init 失败 → 回收一切,返回 0
```

注意第 6 步:`init` 是在**创建者线程**里同步执行的,不是 worker。所以一个服务的初始化阻塞,会阻塞创建它的那个服务的消息处理。这是 skynet 的一个隐性约定:**`init` 要快**。

### 4.2 C 模块加载:`skynet_module.c`

[skynet-src/skynet_module.c](skynet-src/skynet_module.c)。skynet 的所有服务类型都是 `.so` 动态库。约定每个库导出 4 个符号:

```
xxx_create()  → 创建实例
xxx_init(inst, ctx, param) → 初始化
xxx_release(inst) → 销毁
xxx_signal(inst, sig) → 信号(lua 服务的 inject/debug 用)
```

`skynet_module_query(name)`(第 103 行)按 `cservice_path` 配置找 `?` 通配符替换后 `dlopen`,再 `dlsym` 出这四个函数。查到后缓存在 `M->m[]` 数组里(最多 32 种)。**这就是 skynet 的"插件"机制——加一种新服务类型,写个 .so 就行。**

### 4.3 服务的销毁与引用计数

`ref` 用原子操作维护。`skynet_context_grab` +1,`skynet_context_release` -1,到 0 才真正 `delete_context`。这是为了**让 worker 正在处理某服务的消息时,别人 KILL 它不会 use-after-free**。

`cmd_kill` / `cmd_exit`([skynet_server.c:449](skynet-src/skynet_server.c#L449))走 `skynet_handle_retire` —— 把 handle 从表里摘掉(此后新消息发不进来),但**已经在队列里的消息还会被处理完**,处理完队列空了,context 引用归零才真正释放。优雅退出。

---

## 5. 消息队列:全局队列 + 服务队列

打开 [skynet-src/skynet_mq.c](skynet-src/skynet_mq.c)。这是 skynet 最精巧的数据结构之一,只有 250 行,务必通读。

**两级队列:**

1. **全局队列 `global_queue`**([mq.c:35](skynet-src/skynet_mq.c#L35)):一个简单的带 spinlock 的链表,链着所有"有消息待处理"的服务队列。worker 从这里 pop。
2. **服务队列 `message_queue`**([mq.c:21](skynet-src/skynet_mq.c#L21)):每个服务一个,环形数组(`head/tail/cap`),默认 cap=64,满 2 倍扩容。带自己的 spinlock。

**核心 trick —— `in_global` 标志位**([mq.c:18](skynet-src/skynet_mq.c#L18)):

```c
// 0 means mq is not in global mq.
// 1 means mq is in global mq , or the message is dispatching.
```

- `skynet_mq_push`(第 189 行):塞消息。如果 `in_global==0`,把它挂上全局队列并置 1;否则什么都不做(它已经在全局队列里了,或者正在被派发)。**这保证一个服务队列在全局队列里最多出现一次,无论堆了多少消息。**
- `skynet_mq_pop`(第 137 行):取消息。如果取空了(`head==tail`),把 `in_global` 置 0 —— **下次有新消息来才会重新挂回去。**

> 这就是 skynet 的"工作集"自管理:**全局队列里永远只放着"当下有活"的服务**,空队列自动离场。worker 永远不会在空队列上空转。

**过载保护**([mq.c:127](skynet-src/skynet_mq.c#L127) `skynet_mq_overload`):队列长度超过阈值(初始 1024)就翻倍阈值并记下 `overload`,worker 派发时读到非 0 就打 "May overload" 警告。阈值会随长度增长而指数退避,避免刷屏;队列空了重置回 1024。

---

## 6. 消息的发送与编码

发送入口 [skynet-server.c:695](skynet-src/skynet_server.c#L695) `skynet_send()`。读懂它就懂了 skynet 消息的全部秘密:

### 6.1 type 编码在 sz 的高 8 位

看 [skynet_mq.h:16](skynet-src/skynet_mq.h#L16):

```c
#define MESSAGE_TYPE_MASK (SIZE_MAX >> 8)
#define MESSAGE_TYPE_SHIFT ((sizeof(size_t)-1) * 8)
```

`skynet_message.sz` 字段同时编码了**消息长度(低 56/24 位)和消息类型(高 8 位)**。`_filter_args`([server.c:674](skynet-src/skynet_server.c#L674))在发送前把 type 塞进 sz 高位。派发时 `dispatch_message` 再拆开:

```c
int type = msg->sz >> MESSAGE_TYPE_SHIFT;
size_t sz = msg->sz & MESSAGE_TYPE_MASK;
```

> 这是 skynet 省 struct 空间的小聪明:一个 size_t 扛两份信息。

### 6.2 type 的含义

[skynet.h:9](skynet-src/skynet.h#L9) 定义了消息类型。值得记的:

| 类型 | 值 | 含义 |
|---|---|---|
| `PTYPE_RESPONSE` | 1 | call 的返回(由定时器/目标服务回送) |
| `PTYPE_CLIENT` | 3 | 客户端消息(gate 转发进来) |
| `PTYPE_SOCKET` | 6 | socket 事件(netpack/gate 用) |
| `PTYPE_ERROR` | 7 | 目标服务不存在/已退出,回告调用方 |

还有两个 tag(不是独立 type,是位标记):
- `PTYPE_TAG_DONTCOPY = 0x10000`:发送时不拷贝 data,直接移交所有权(零拷贝)。
- `PTYPE_TAG_ALLOCSESSION = 0x20000`:自动分配 session(call 用)。

### 6.3 session:`call` 的返回地址

`skynet_send` 的 `session` 参数是调用方生成的递增整数。**发送 `session>0` 的消息=我等回信;`session=0`=fire and forget(send)。** 目标处理完后用同样的 session 回一条 `PTYPE_RESPONSE`。lua 层的 `skynet.call` 就靠这个挂起协程、等 response 唤醒(见第 11 节)。

### 6.4 本地 vs 跨节点

`skynet_send` 末尾([server.c:719](skynet-src/skynet_server.c#L719)):

```c
if (skynet_harbor_message_isremote(destination)) {
    // 目标地址的 harbor 段 != 本节点 → 包成 remote_message 走 harbor 服务
    skynet_harbor_send(rmsg, source, session);
} else {
    // 本地 → 直接 push 进目标服务的队列
    skynet_context_push(destination, &smsg);
}
```

本地消息**不经过任何队列中转,直接 push 进目标 service 的队列**,这是 skynet 跨服务调用极快的根本原因——本质上就是一次内存拷贝 + 一次链表 push。

---

## 7. 定时器:经典时间轮

打开 [skynet-src/skynet_timer.c](skynet-src/skynet_timer.c)。这是教科书级的时间轮(timing wheel)实现,建议逐行读。

**结构**([timer.c:39](skynet-src/skynet_timer.c#L39)):

```c
struct timer {
    struct link_list near[256];   // 近槽:256 个桶,覆盖 256 个 tick(2.56 秒)
    struct link_list t[4][64];    // 远槽:4 级,每级 64 桶
    uint32_t time;                // 当前 tick(1 tick = 10ms = 1/100 秒)
    ...
};
```

- **1 tick = 10ms**(centisecond)。`thread_timer` 每 2.5ms 醒一次,但 `skynet_updatetime` 按 10ms 整数倍推进 `TI->current`。
- `near[256]`:256 个桶,精确到 tick。下一个 2.56 秒内的定时器都挂这。
- `t[4][64]`:4 级 64 桶,逐级粗粒度,覆盖更长周期。**这是 Linux 内核同款时间轮算法。**

**`timer_add`**([timer.c:88](skynet-src/skynet_timer.c#L88)):算出 `expire = time + 延迟`,根据 expire 落在 near 还是某级 t 里挂上。

**`timer_shift`**([timer.c:111](skynet-src/skynet_timer.c#L111)):每过一个 tick,把当前 near 槽清空派发,并把更高级的桶按需"降级"移到下一级(像水表进位)。**O(1) 的定时器推进。**

**`dispatch_list`**([timer.c:134](skynet-src/skynet_timer.c#L134)):到期了干什么?——**给目标服务 push 一条 `PTYPE_RESPONSE` 消息,session 就是 `skynet.timeout` 时分配的那个。** 所以定时器不是回调,而是"到期发一条消息",复用了消息机制。lua 层的 `skynet.timeout(ti, func)` 把 `func` 包成一个挂起的协程,等这条 response 唤醒它。

---

## 8. Socket 引擎:单线程网络

打开 [skynet-src/socket_server.c](skynet-src/socket_server.c)。2475 行,是仓库里最大的文件,但设计极清晰。

### 8.1 单线程 + 命令管道

**skynet 的所有网络 I/O 都在 `thread_socket` 这一个线程里**(见 [skynet_start.c:63](skynet-src/skynet_start.c#L63))。worker 线程想做网络操作(listen/connect/send/close),不能直接调 epoll——它们通过一个**命令管道**把请求投递给 socket 线程:

```
struct socket_server {
    int recvctrl_fd, sendctrl_fd;  // socketpair,worker 往里写命令,socket 线程从里读
    poll_fd event_fd;              // epoll/kqueue fd
    struct socket slot[65536];     // socket 槽位,MAX_SOCKET = 1<<16
    ...
};
```

worker 调 `skynet_socket_send`/`listen`/`connect` → 把请求结构体写进 `sendctrl_fd` → epoll 的事件循环(`skynet_socket_poll`)发现 recvctrl_fd 可读 → 解析命令执行。**这是一个典型的单线程 reactor + 跨线程命令队列模型。**

### 8.2 socket 的状态机

[socket_server.c:18](skynet-src/socket_server.c#L18) 定义了 socket 类型:从 `PLISTEN`(预备监听)→ `LISTEN`、`PACCEPT`(预备接受)→ `CONNECTING`→`CONNECTED`、`HALFCLOSE_READ/WRITE`... 一个 socket 的生命周期就是这些状态的迁移。

> 读这段时关注:**为什么要有 `P` 前缀的"预备"状态?** 因为 accept/connect 完成后,socket 线程不能直接派发——它要先通过消息把 fd 交给对应的 service,service 决定何时 `start` 这个 socket(注册进 epoll 关注可读)。这避免了"fd 还没主人就有数据来"的竞态。

### 8.3 收到数据后:netpack 切包

socket 线程从内核读到一段字节流,塞进 `skynet_socket_message` 发给"持有这个 socket 的 service"。但业务要的是**一条条完整的消息**,不是字节流。切片工作由 [lualib-src/lua-netpack.c](lualib-src/lua-netpack.c) 完成:

**协议:每个消息前 2 字节大端长度前缀。** netpack 维护每个 socket 的残包缓冲,按 2 字节长度切出完整消息,以 `PTYPE_CLIENT` 推给 service。这就是 skynet 内置的应用层分包协议,简单粗暴但够用。

### 8.4 gate 服务:连接管家

[lualib-src/service_gate.c](lualib-src/service_gate.c)(C 版)或 [service/gate.lua](service/gate.lua)(lua 版)是连接管理服务。它 listen 一个端口,accept 进来的连接,按配置把它们"绑定"到某个 watchdog service。**每个新连接由哪个 service 负责处理,是 gate 决定的。**

> 典型链路:`gate` accept → 转发 `PTYPE_SOCKET` 数据给 `watchdog` → watchdog 给连接分配一个 `agent` service → 之后该连接的消息直接进 agent。这是 skynet 经典的"一连接一服务"模型。

---

## 9. snlua:Lua 服务的容器

skynet 的业务服务几乎都是 lua 写的。每个 lua 服务运行在一个 `snlua` 实例里。打开 [service-src/service_snlua.c](service-src/service_snlua.c)。

**`snlua` = 一个独立的 `lua_State` + 内存统计 + 信号钩子。** 关键设计:

### 9.1 两阶段初始化

`snlua_init`([snlua.c:469](service-src/service_snlua.c#L469))很巧妙:

```c
skynet_callback(ctx, l, launch_cb);          // 先挂一个临时回调
skynet_send(ctx, 0, handle_id, ..., args);    // 给自己发一条消息(参数字符串)
```

它**不直接初始化 lua**,而是先挂个 `launch_cb` 回调,然后给自己发一条消息。等 worker 真正调度到这条消息时,才在 `launch_cb` 里调 `init_cb` 做真正的 lua 初始化。

> 为什么?因为 `skynet_context_new` 的 `init` 阶段是在创建者线程同步执行的,可能阻塞。把 lua 初始化推迟到 worker 线程异步执行,**避免创建一个重 lua 服务时卡住创建者**。这是 skynet 对"init 要快"原则的工程妥协。

### 9.2 `init_cb`:加载 loader → 跑你的服务脚本

[snlua.c:383](service-src/service_snlua.c#L383) `init_cb`:

1. `luaL_openlibs` 开标准库,替换 `coroutine.resume/wrap` 成带 profile 的版本([snlua.c:216](service-src/service_snlua.c#L216))。
2. 设置 `LUA_PATH` / `LUA_CPATH` / `LUA_SERVICE`(从 skynet env 取)。
3. **加载 `lualib/loader.lua`**,把服务名(如 `"bootstrap"`)作为参数传给它。loader 负责按 `LUA_SERVICE` 路径找到对应的 lua 文件并 `require` 执行。

所以一个 lua 服务的入口链是:`snlua init → loader.lua → 你的 service.lua`。你的 service.lua 里通常第一行就是 `skynet.start(function() ... end)`。

### 9.3 内存限制与信号注入

- `lalloc`([snlua.c:482](service-src/service_snlua.c#L482)):自定义 lua 内存分配器,统计 `l->mem`,超过 `mem_limit` 拒绝分配(防 OOM),超过 `mem_report` 翻倍报警。
- `snlua_signal`([snlua.c:534](service-src/service_snlua.c#L534)):收到 signal 0 时,设置 `lua_sethook` 在协程下一个指令处中断——**这就是 debug_console 能"打断"一个死循环 lua 服务的原理。** 用 `LUA_MASKCOUNT, 1` 让 hook 在下一条字节码触发,抛出 `signal 0` 错误,被 pcall 接住。

---

## 10. 跨节点:Harbor 与 Cluster

skynet 支持多进程组网,有两种机制,别混淆:

### 10.1 Harbor(老式,同集群)

[skynet-src/skynet_harbor.c](skynet-src/skynet_harbor.c) + [service-src/service_harbor.c](service-src/service_harbor.c)。每个节点有一个 harbor id(配置 `harbor = 1..255`),编码在 handle 的高 8 位。`skynet_harbor_message_isremote`([harbor.c:26](skynet-src/skynet_harbor.c#L26))靠这个判断目标是不是远程。

远程消息包成 `remote_message` 发给本节点的 harbor 服务(`REMOTE`),harbor 服务通过配置好的节点间 socket 连接转发。**这是 skynet 早期的内置集群方案,新项目多用 cluster。**

### 10.2 Cluster(新式,更灵活)

[lualib-src/lua-cluster.c](lualib-src/lua-cluster.c) + [service/clusterd.lua](service/clusterd.lua) 等。cluster 用**字符串名字**而非 harbor id 寻址,节点间连接管理更完善(clusterd/clusterproxy/clustersender/clusteragent 分工)。业务层用 `cluster.call("nodename", "service", ...)` 跨节点调用。**现代 skynet 分布式首选。**

---

## 11. Lua 层框架:把回调变成协程

至此 C 层已经讲完。但 skynet 真正好用的地方在于 lua 层——**它把"事件回调"这种反人类的模型,包装成了"同步 call"的协程模型。** 灵魂文件:[lualib/skynet.lua](lualib/skynet.lua)(1189 行,通读)。

### 11.1 `dispatch_message`:回调的入口

[skynet.lua:963](lualib/skynet.lua#L963)。C 层每派发一条消息就调它。它干两件事:

1. **按 type 查 `proto` 表找处理函数**(`skynet.dispatch` 注册的),没注册的走 `dispatch_unknown_request`。
2. **为这条消息新建一个协程**去跑处理函数(或复用挂起的协程)。

> 这是关键:每条进来的消息 = 一个新协程。协程间不共享栈,互不阻塞。一个服务可以同时"挂着"成百上千个协程等回信,但**同一时刻只有一个协程在跑**(单线程语义,无锁)。

### 11.2 `skynet.call`:同步语义的魔法

[skynet.lua:725](lualib/skynet.lua#L725):

```lua
function skynet.call(addr, typename, ...)
    local p = proto[typename]
    local session = auxsend(addr, p.id, p.pack(...))  -- 发消息,拿到 session
    return p.unpack(yield_call(addr, session))         -- 挂起协程等回信
end
```

`yield_call`([skynet.lua:714](lualib/skynet.lua#L714))把 `session → 协程` 存进 `session_id_coroutine` 表,然后 `coroutine_yield "SUSPEND"` 挂起。

等目标服务处理完,回一条 `PTYPE_RESPONSE`(带同样的 session),`dispatch_message` 收到后从表里取出协程 `resume` 它,把返回值喂回去。**于是你写的是 `local result = skynet.call(...)` 这种同步代码,底层却是完全异步的消息往来。** 这就是 skynet 编程体验的核心。

### 11.3 `skynet.ret`:回信

[skynet.lua:761](lualib/skynet.lua#L761)。处理完一条 call 消息后调 `skynet.ret(msg)` 给调用方回 `PTYPE_RESPONSE`,session 从协程上下文里取(`session_coroutine_id`)。**`session==0` 的是 send(无需回信),`ret` 直接 no-op。**

### 11.4 `skynet.start`:服务的入口

[skynet.lua:1077](lualib/skynet.lua#L1077):

```lua
function skynet.start(start_func)
    c.callback(skynet.dispatch_message)   -- 注册 C 层回调
    init_thread = skynet.timeout(0, function()
        -- 下一 tick 才真正跑 start_func
        ...
    end)
end
```

注册 `dispatch_message` 作为 C 回调,然后用 `timeout(0, ...)` 把 `start_func` 推迟到"消息循环开始运转后"执行。这样 `start_func` 里可以安全地 `skynet.call` 别的服务(此时消息机制已就绪)。

### 11.5 协议注册:`skynet.dispatch`

[skynet.lua:851](lualib/skynet.lua#L851)。`skynet.dispatch("lua", function(session, source, ...) ... end)` 把某 type 的消息处理函数记进 `proto` 表。`skynet.protocol` 还能注册带 pack/unpack 的自定义协议。**业务服务一般只 dispatch 几种 type,其余靠框架。**

---

## 12. 推荐阅读路线图

按这个顺序读,阻力最小:

### 第一圈:跑起来(1 小时)
1. 看 [examples/config](examples/config) 和 [examples/main.lua](examples/main.lua),理解配置项。
2. `make linux`,跑起 examples,用 `examples/client.lua` 连一下。
3. 读 [service/bootstrap.lua](service/bootstrap.lua) + [service/launcher.lua](service/launcher.lua),看启动后干了啥。

### 第二圈:C 内核主轴(半天)
按本指南第 2→6 节顺序:
1. `skynet_main.c` → `skynet_start.c`(启动 + 线程模型)
2. `skynet_server.c`(`skynet_context` + `dispatch` + `send`)
3. `skynet_mq.c`(两级队列,通读)
4. `skynet_monitor.c`(死循环检测)
5. `skynet_handle.c`(地址表,注意那套**分布式读者槽**优化 [handle.c:50](skynet-src/skynet_handle.c#L50),挺新)

### 第三圈:三大子系统(一天)
1. `skynet_timer.c`(时间轮,通读,150 行)
2. `socket_server.c`(挑重点:`skynet_socket_poll`、`forward_message`、状态迁移;不必逐行)
3. `service_snlua.c`(lua 服务容器,通读)

### 第四圈:Lua 框架(半天)
1. [lualib/skynet.lua](lualib/skynet.lua) 通读,重点 11 节那几个函数。
2. [lualib/loader.lua](lualib/loader.lua)(服务加载)
3. [service/gate.lua](service/gate.lua) 或 `service_gate.c`(连接管理)
4. [lualib-src/lua-netpack.c](lualib-src/lua-netpack.c)(分包)

### 第五圈:进阶(按需)
- cluster:`lua-cluster.c` + `service/clusterd.lua`
- multicast:`lua-multicast.c` + `service/multicastd.lua`(发布订阅)
- sharedata/sharetable:`lua-sharedata.c` / `lua-sharetable.c`(只读共享,省内存)
- debug_console:[service/debug_console.lua](service/debug_console.lua)(线上运维神器,通读能学到很多 skynet 内部命令)

---

## 13. 动手验证:几个该自己跑的实验

读源码不如跑源码。建议做这几个实验:

1. **改 worker 数量**:在 config 里把 `thread` 从 8 改成 1,观察一个 `skynet.call` 链路的延迟变化。理解"worker 是共享资源"。
2. **制造死循环**:写个服务 `while true do end`,5 秒后看 monitor 报错。然后 `kill` 它,确认别的服务不受影响。
3. **看消息队列**:用 debug_console 的 `stat` 命令看某个服务的 `mqlen`,压测时观察过载。
4. **抓一次 call**:在 `skynet.call` 前后打日志,配合 debug_console 的 `trace`,看清一条跨服务调用的 session 流转。
5. **读 weight 调度**:把 `skynet_start.c` 的 weight 数组改成全 0,压测一个长队列服务,对比"公平性"变化。

---

## 14. 设计哲学:为什么 skynet 长这样

读完上面这些,你会摸到云风(skynet 作者)的几条信念:

1. **Actor 优于锁。** 不共享内存,只发消息 → 天然无锁,天然可分布式。代价是拷贝,但本地消息零拷贝(`DONTCOPY`)和 sharetable 弥补了。
2. **单服务单线程语义,多服务并行。** 每个 lua 服务内部是单线程的,写业务不用考虑锁;并行性靠开多个服务 + 多 worker 实现。**把并发的复杂度从"锁"转移到"服务拆分"上。**
3. **C 做骨架,Lua 做血肉。** 性能敏感的调度/IO/队列在 C 层(快且稳),易变的业务逻辑在 Lua 层(灵活且热更)。snlua 是两者的粘合剂。
4. **公平调度 + 死循环兜底。** weight 抢占 + monitor 巡查,保证一个坏服务拖不垮全局。这是生产级游戏服务器的底线要求。
5. **一切皆消息。** 定时器到期是消息,socket 可读是消息,call 返回是消息。**只有一种控制流模型,心智负担最小。**

当你能在脑子里完整模拟"一条 `skynet.call` 从 service A 发出,经过 mq、worker、dispatch、协程 yield、B 服务处理、ret、response 唤醒 A 协程"的全过程时,你就真正读懂了 skynet。

---

## 附录:核心文件速查表

| 想了解 | 看这里 |
|---|---|
| 启动流程 | [skynet-src/skynet_main.c](skynet-src/skynet_main.c) → [skynet_start.c](skynet-src/skynet_start.c) |
| 服务结构/生命周期 | [skynet-src/skynet_server.c](skynet-src/skynet_server.c) |
| 消息队列/调度 | [skynet-src/skynet_mq.c](skynet-src/skynet_mq.c) |
| 死循环检测 | [skynet-src/skynet_monitor.c](skynet-src/skynet_monitor.c) |
| 服务地址表 | [skynet-src/skynet_handle.c](skynet-src/skynet_handle.c) |
| C 模块加载 | [skynet-src/skynet_module.c](skynet-src/skynet_module.c) |
| 定时器 | [skynet-src/skynet_timer.c](skynet-src/skynet_timer.c) |
| 网络 I/O | [skynet-src/socket_server.c](skynet-src/socket_server.c) |
| Lua 服务容器 | [service-src/service_snlua.c](service-src/service_snlua.c) |
| Lua 框架 API | [lualib/skynet.lua](lualib/skynet.lua) |
| 分包 | [lualib-src/lua-netpack.c](lualib-src/lua-netpack.c) |
| 连接管理 | [service/gate.lua](service/gate.lua) / [service-src/service_gate.c](service-src/service_gate.c) |
| 跨节点(老) | [skynet-src/skynet_harbor.c](skynet-src/skynet_harbor.c) |
| 跨节点(新) | [lualib-src/lua-cluster.c](lualib-src/lua-cluster.c) + [service/clusterd.lua](service/clusterd.lua) |
| 线上运维 | [service/debug_console.lua](service/debug_console.lua) |

---

> **最后一句话**:skynet 不大,C 内核满打满算 7000 行,lua 框架 1200 行。它值得你逐行读完。读完后你得到的不是"会用的框架",而是"单机高并发服务器可以怎么设计"的一整套答案——这套答案在你将来写任何 actor/协程/事件循环系统时都会反复用到。
>
> 现在,打开 [skynet-src/skynet_start.c](skynet-src/skynet_start.c),从 `skynet_start()` 开始,走吧。