---
title: "网络多人在线游戏开发实战教程 · 第10章 分布式登录与Redis内存数据库"
category: 教材原文
type: 原文章节稿
source_book: "网络多人在线游戏开发实战教程"
source_section: "第10章 分布式登录与Redis内存数据库"
tags:
  - 教材原文
  - 彭放教程
  - C++服务器
source: "《网络多人在线游戏开发实战教程》（彭放，机械工业出版社 2019，ISBN 9787111667926）第10章 分布式登录与Redis内存数据库 · md 稿提取（文字层 + 图表 OCR 回填），保留原文未净化"
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 第10章 分布式登录与Redis内存数据库

> **原文章节稿 · 未经净化。** [[网络多人在线游戏开发实战教程|← 返回本书目录]]
> 提取自 PDF（文字层 + 图表视觉 OCR 回填，138 张图表）。**保留原书全部技术细节与代码，未做删减或改写**。各章末尾的「图表转写」是该章配图的文字转录；原始裁图在 `_raw/B_img/`。

## 本章小节

- [[#1.1 所有space进程都开放一个监听端口。|1.1 所有space进程都开放一个监听端口。]]
- [[#1.2 所有space进程都连接所有game进程，向其发送本进程的状态，也就是说，所有game进程上都有所有space进程的当前状态信息。这|1.2 所有space进程都连接所有game进程，向其发送本进程的状态，也就是说，所有game进程上都有所有space进程的当前状态信息。这]]
- [[#2.1 所有game进程都自动连接所有space端口。|2.1 所有game进程都自动连接所有space端口。]]
- [[#2.2 所有game进程都开放一个监听端口，用于客户端连接。|2.2 所有game进程都开放一个监听端口，用于客户端连接。]]
- [[#2.3 所有game进程都连接appmgr进程，向其发送本进程的状态。appmgr会将这些数据发送给login进程，以便任何login了解所有gam|2.3 所有game进程都连接appmgr进程，向其发送本进程的状态。appmgr会将这些数据发送给login进程，以便任何login了解所有gam]]
- [[#2.4 所有game进程都连接dbmgr，用于数据读取。|2.4 所有game进程都连接dbmgr，用于数据读取。]]
- [[#3.1 所有login进程都开放一个监听端口，用于客户端连接。|3.1 所有login进程都开放一个监听端口，用于客户端连接。]]
- [[#3.2 所有login进程都连接appmgr，向其发送本进程的状态，以便HTTP请求时，appmgr了解所有login进程信息，择优进入。|3.2 所有login进程都连接appmgr，向其发送本进程的状态，以便HTTP请求时，appmgr了解所有login进程信息，择优进入。]]
- [[#4.1 appmgr只有一个唯一实例，提供HTTP端口，用于客户端HTTP请求。|4.1 appmgr只有一个唯一实例，提供HTTP端口，用于客户端HTTP请求。]]
- [[#5.1 dbmgr只有一个唯一实例，提供DB存储。|5.1 dbmgr只有一个唯一实例，提供DB存储。]]

> **尚未净化** —— 本章还没有对应的详解笔记。

---

本章将建立最终的框架需要的所有进程，即增加 game 和 space 进程。同

时，引导玩家进入 game 进程中，在这个过程中使用了 Redis 内存数据库。本章

包括以下内容：

·增加 game 和 space 工程。

·Redis 内存数据库的使用。

·完善登录流程。

1 0 . 1

g a m e 与 s p a c e 的 定 位

本节将建立最终的框架需要的所有进程，即增加 game 和 space 进程。图 10-1 展示了 game 和 space 进程之间的关系。

**图 10-1 进程分布**

客户端通过 HTTP 请求到一个合适的 login 进程进行账号验证，验证成功之后，login 会分配一个合适的 game 进程给

客户端进行连接，game 进程提供玩家角色加载，如果有多角色进行选角操作，space 进程就为其分配最终的地图实例。

以 MOBA 游戏的流程为例，当我们登录游戏进行玩家对战匹配之后会进入副本，这个副本的真实数据就运行在 space

进程上。space 进程对玩家是不可见的，game 进程会进行代理。

为什么要把 game 和 space 分开，放在一起不是更容易开发吗？

当然是为了容纳更多的玩家，假如 game 进程中可以连接一万个玩家，这一万个玩家可能在 10 个 space 进程中玩游

戏，从而分散了计算压力。space 承载了副本功能，但除了这个功能之外，还可以有其他功能。例如，我们可以创建一

个特别的聊天组件，在这个组件中处理所有玩家的聊天信息，并将它们发送到目的地。

space 进程很重要的作用是实现聚合功能。例如，我们可以将游戏中的帮派系统放在 space 上，在这个系统中处理

所有帮派相关的事宜。假设有两个玩家分别在 game1 和 game2 进程中，他们属于同一帮派，两个玩家登录时，帮派系统

会收到他们登录的信息，如果在 game1 上的玩家推送了一个帮派聊天协议，最终这个协议会到达帮派系统在 space 的实

例上，帮派系统会将这个协议发送到本帮派所有玩家的客户端上。正是因为有了帮派系统的存在，所以不需要关心对

方是否存在于本进程中。

总之，space 进程是为了解决聚合问题，运行在 space 进程上的每一个实体都应当是一个空间。这里的空间可以是

地域上的空间，也可以是逻辑上的空间，空间中的对象需要相互交互。

现在，框架中有 6 个进程，但是这些进程可以根据具体需求调整。可能在某些需求下，需要将 login 和 game 进程合

并，这个过程非常容易。因为所有 login 的组件都是 Actor 模式的，放在任何进程中都可以正常工作，这里不再详述，

可参见 allinone 工程。

接下来需要明确一个问题，这些进程两两之间如何连接必须要有一个规范。设计原则如下：

1.space 进程


## 1.1 所有space进程都开放一个监听端口。


## 1.2 所有space进程都连接所有game进程，向其发送本进程的状态，也就是说，所有game进程上都有所有space进程的当前状态信息。这

2.game 进程


## 2.1 所有game进程都自动连接所有space端口。


## 2.2 所有game进程都开放一个监听端口，用于客户端连接。


## 2.3 所有game进程都连接appmgr进程，向其发送本进程的状态。appmgr会将这些数据发送给login进程，以便任何login了解所有gam


## 2.4 所有game进程都连接dbmgr，用于数据读取。

3.login 进程


## 3.1 所有login进程都开放一个监听端口，用于客户端连接。


## 3.2 所有login进程都连接appmgr，向其发送本进程的状态，以便HTTP请求时，appmgr了解所有login进程信息，择优进入。

4.appmgr 进程


## 4.1 appmgr只有一个唯一实例，提供HTTP端口，用于客户端HTTP请求。

5.dbmgr 进程


## 5.1 dbmgr只有一个唯一实例，提供DB存储。

剔除一些细节，从头梳理一下，一个完整的登录流程是这样的：

• （1）客户端通过 HTTP 连接得到一个可用于登录的 login 进程的 IP 和端口。

• （2）连接到 login 进程上进行账号验证，成功登录之后，创建角色和选择角色，目前大部分游戏都是单一角色，

可能没有选择角色这一功能。选定角色之后，login 进程会分析所有 game 进程的数据，选择一个合适的发送给客户端，

并同时生成 token。

• （3）客户端通过 token 连接到 game 进程上，正式开始游戏，进入某个地图。

• （4）收到进入地图的协议，game 进程首先向 appmgr 发起请求，判断是否有该地图的实例，如果没有，就马上在一

个合适的 space 中创建一个地图实例。同时，在 game 进程中生成 WorldProxy 实例用于代理地图数据，WorldProxy 一旦生

成，所有数据均会转发至 space 进程，game 进程则作为一个中转进程，类似网关进行数据转发。地图建立成功之后，向

客户端发送进入地图协议，加载地图与玩家模型，玩家正式进入地图。

如果读者第一次接触游戏，最后一步一定让读者有一种头晕目眩的感觉。先来考虑前 3 步，如何登录 game 进程，虽

然登录看上去只有短短几步，但是需要考虑的东西却不少。以验证账号为例，如果有玩家使用同一个账号同时在

login1 和 login2 进程登录，该如何处理呢？我们先将这个问题放在这里，在后面的章节中再来解决，本节先为所有进

程建立两两连接的网络，把框架建立起来。

在有些架构中，在网络连接部分，在需要时才建立网络连接。例如 game 和 space 进程，开始可能没有建立 game 与任

何 space 进程的网络通信，需要时才建立一个，但是这种连接有一定的风险性，可能会丢失数据。创建一个网络需要时

间，当我们发送数据时才创建网络，这就涉及 Packet 的缓存问题，Packet 需要缓存到它依赖的网络创建好了再发送。

鉴于此，本书的框架中选择在所有进程初始化时就连接所有需要连接的目的地。这样，在玩家需要跳转到 space2

上时，如果 space2 出现异常宕机，在 game 进程上就会抛出一个断线协议。当玩家发送跳转协议时，可以检测到该网络

连接已经找不到了，从而向客户端发送异常提示。

本节讨论的多进程项目已经在 10_01_game 目录中创建出来了，增加了 game 和 space 两个新的工程。目前 space 没有

任何组件，只是一个空工程。在 10_01_game 目录的项目中，我们加入了登录 game 的操作。先来运行一下看看结果，再

分析具体的代码。先启动 allinone，再启动 robots，登录一个账号进行测试：

login -a test

test begin

Http-Connecting over. time:4e-06s

Http-Connected over. time:0.00738s

Login-Connecting over. time:0.017818s

Login-Connected over. time:0.021281s

Login-SelectPlayer over. time:0.048624s

Game-Connecting over. time:0.059159s

Game-Connected over. time:0.063525s

从 Robot 类的状态来看，一个账号从登录到最后完成了 7 个状态的功能，最终这个 Robot 成功登录 game 进程。登录成

功的标志是 Robot 类向 game 进程发送 C2G_LoginByToken 协议，使用 token 登录，服务端返回 C2G_LoginByTokenRs 协议，

如果成功，Robot 的状态就变为 Game-Connected。

测试 10_01_game 工程时有两种启动方式，第一种是 allinone，这种方式很简单;第二种是以多进程启动。现在我


```bash
[root@localhost bin]# ./dbmgrd
[root@localhost bin]# ./appmgrd
[root@localhost bin]# ./logind -sid=101
[root@localhost bin]# ./gamed -sid=201
[root@localhost bin]# ./spaced -sid=301
```

们至少需要启动 5 个进程，它们分别是：

进程的参数 101 和 201 是在 engine.yaml 文件中配置的，根据配置可以读出相关的监听地址。下面来看 login 启动后


```bash
[root@localhost bin]# ./logind -sid=101
```

的打印：

...

connected appType:appmgr appId:0  socket:9 networkType:TcpConnector connect type:App

connected appType:dbmgr appId:0  socket:10 networkType:TcpConnector connect type:App

进程 login 连接上了 appmgr 和 dbmgr。这部分代码在 NetworkConnector 初始化时就加上了，前面已经讲过。下面来


```bash
[root@localhost bin]# ./gamed -sid=201
```

看 game 进程启动后的打印输出：

...

connected appType:dbmgr appId:0  socket:488 networkType:TcpConnector connect type:App

connected appType:appmgr appId:0  socket:492 networkType:TcpConnector connect type:App

connected appType:space appId:301  socket:508 networkType:TcpConnector connect type:App

game 进程连接了 id 为 301 的 space 进程。如果这时关闭 301 进程，game 进程就会敏锐地察觉到并打印以下信息：

[DEBUG] - remove appType:space appId:301

这时打开 301 进程，game 会马上重新连接上去。在整个框架中，当我们用多进程的方式启动时，不需要启动顺序，

所有的进程都会对自己需要的连接进行自动重连。总之，我们已经将所有需要的进程进行了两两连接，在后面使用的

过程中只需要配置好数据即可。可以在 engine.yaml 的 space 下配置多个需要开启的 space 进程。关于自动重连接网络的

部分，本节不再详细介绍。下面从代码分析的角度看看 Robot 是如何从 login 进程跳转到 game 进程的，它是根据什么来

选择跳转以及跳转的，之后如何验证正误。

1 0 . 1 . 1 　 选 择 合 适 的 g a m e 进 程

进程 login 如何知道在众多的 game 进程中哪一个是合适的呢？有无数种方法可以达到这个目的，但这里我们只讨论

两种方案：

第一种方案是让 game 进程向第三方的 appmgr 进程定时发送自己的状态信息，例如有多少人在线，当 login 进程需要

时，向 appmgr 请求数据，从而获得一个负载最小的 game 进程。这个方案是一个异步方案，而且每次向 appmgr 请求数据

时必然要经过一个串行处理。appmgr 上需要编写一个组件来处理这个消息，当协议到达这个消息时就进行串行的处

理，从而会降低效率。

**图 10-2 展示了这两种方案的流程。左边是第一种方案，右边是第二种方案。再来看看第二种方案。**

**图 10-2 选择Game进程**

在第二种方案中，game 与 login 两两通信，game 定时向所有 login 进程发送自己的状态信息。当 login 进程需要时，

不需要异步过程，直接就知道哪个 game 进程负载最小。但是这个方案有一个问题，如果有两个人分别处于 login1 和

login2 进程上，在一瞬间两个 login 进程分析出来的数据得到的最小负载的 game 进程是同一个进程，这意味着从短时间

来看，负载可能不是精确均衡的，从长期来看，这个问题会得以解决。

除此之外，第二种方案还有一个问题，如果我们有 10 个线程，也就是说每个 game 都必须连接 10 个 login，连接这 10

个 login 的原因只是为了发送状态数据，显然有点不值。为此，合并一下两种方案。login 进程不要直接和 game 进程产

生网络连接，game 进程向 appmgr 进程发送自己的状态，但是每隔两秒，appmgr 进程将收集到的数据推送给所有 login。

这样既减少了网络连接，在 login 进程上分配 game 进程时又不需要使用异步数据。

下面来看具体实现的步骤。本节源代码位于 10_01_game 目录中。先来梳理一下思路。如果要均衡玩家在各个 game

进程中，是否需要知道每个进程中到底有多少玩家？一个 game 进程中可能有无数地图，这些地图上有无数玩家，而这

些地图是分散在各个线程中的。因此，需要放置一个收集组件，让每个地图定时向这个收集组件发送当前地图的人

数。同时，这个收集组件定时向 appmgr 进程发送它的统计数据。


```cpp
void WorldProxyGather::Awake() {
    AddTimer(0, 10, true, 2, BindFunP0(this, &WorldProxyGather::SyncGameInfo));
    ...
    pMsgCallBack->RegisterFunction(Proto::MsgId::MI_WorldProxySyncToGather, BindFunP1(this, &WorldProxyGa
}
void WorldProxyGather::SyncGameInfo() {
    Proto::AppInfoSync proto;
    const int online = std::accumulate(_maps.begin(), _maps.end(), 0, [](int value, auto pair) {
            return value + pair.second.Online;
        });
    proto.set_app_id(Global::GetInstance()->GetCurAppId());
    proto.set_app_type((int)Global::GetInstance()->GetCurAppType());
    proto.set_online(online);
    MessageSystemHelp::SendPacket(Proto::MsgId::MI_AppInfoSync, proto, APP_APPMGR);
}
```

在 game 进程中增加了新组件 WorldProxyGather，就是用来做这件事情的。它的主要代码如下：

在 WorldProxyGather 组件从对象池中被唤醒时，为它增加了一个每 10 秒调用的定时器，该定时器调用

SyncGameInfo 函数向 appmgr 进程发送数据。

在这个组件中，在线玩家的数据是保存在一个字典中的，这是由其他类通知 WorldProxyGather 而形成的，在

WorldProxyGather 组件中，它关心一个名为 MI_WorldProxySyncToGather 的协议。在工程里，全局搜索这个协议，会发

现它在 WorldComponentGather 组件中定时发送。

登录 game 进程，还没有进入具体地图的玩家汇聚在大厅中，在代码中即 Lobby 类。Lobby 类是一个类似地图的类，

它是无形的。这个类生成时被挂上了一个组件 WorldComponentGather。该组件可以挂在任何一个需要发送在线数据的


```cpp
void WorldComponentGather::SyncWorldInfoToGather() const {
    Proto::WorldProxySyncToGather proto;
    proto.set_world_sn(GetSN());
    proto.set_world_proxy_sn(GetSN());
    proto.set_world_id(dynamic_cast<IWorld*>(_parent)->GetWorldId());
    const int online = _parent->GetComponent<PlayerCollectorComponent>()->OnlineSize();
    proto.set_online(online);
    MessageSystemHelp::DispatchPacket(Proto::MsgId:: MI_WorldProxySyncToGather, proto, nullptr);
}
```

World 类中，定时发送的协议如下：

WorldComponentGather 组件定时从其父类的 PlayerCollectorComponent 组件取出当前玩家的数量，发送

MI_WorldInfoSyncToGather 协议，并不关心谁会收到它，这个组件的责任就是组织这些数据并定时发送，任务简单明

了。

无论我们有多少个 game 进程，每个进程上有多少个地图，只要地图实例上挂上了 WorldComponentGather 组件，都

会将数据传递给本进程中的 WorldProxyGather 组件。而 WorldProxyGather 组件定时将这些数据发送到 appmgr 进程中，

appmgr 又定时将这些数据广播到所有 login 进程中，这样 login 就有了 game 进程的数据。图 10-3 展示了这一系列流程。

无论我们登录时落在哪一个 login 进程上，都可以得到所有 game 进程的当前状况，从而选择一个最优的进程登录。

**图 10-3 采集数据流程**

1 0 . 1 . 2 　 使 用 t o k e n 登 录 g a m e 进 程

当 login 进程中的 Account 类选择好可以登录的 game 进程时，它会发送一个 L2C_GameToken 协议到客户


```cpp
void Account::HandleSelectPlayer(Packet* pPacket) {
    ...
    auto pPlayer = pPlayerMgr->GetPlayerBySocket (pPacket->GetSocketKey().Socket);
    ...
    Proto::GameToken protoToken;
    AppInfo info;
    if (!GetOneApp(APP_GAME, &info)) {
        protoToken.set_return_code(Proto::GameToken_ReturnCode_ GameToken_NO_GAME);
    } else {
        protoToken.set_return_code(Proto::GameToken_ReturnCode_ GameToken_OK);
        protoToken.set_ip(info.Ip.c_str());
        protoToken.set_port(info.Port);
        protoToken.set_token("");
    }
    MessageSystemHelp::SendPacket(Proto::MsgId::L2C_GameToken, pPlayer, protoToken);
}
```

端。发送的代码如下：

这里编写了一行特别的代码，将 token 设置为空。有关 token 的生成将在 10.2 节讲解。当 Robot 收到协议

之后，从协议中取出可以登录的地址，这里它会断开与 login 的通信，转而登录 game 进程。Robot 会带上

login 进程发给它的 token，登录 game 进程的协议为 C2G_LoginByToken。而 game 进程的 Lobby 类中处理了该协


```cpp
void Lobby::HandleLoginByToken(Packet* pPacket) {
    auto pPlayerCollector = GetComponent<PlayerCollectorComponent>();
    auto proto = pPacket->ParseToProto<Proto::LoginByToken>();
    auto pPlayer = pPlayerCollector->AddPlayer(pPacket, proto.account());
    if (pPlayer == nullptr) {
        MessageSystemHelp::DispatchPacket(Proto::MsgId:: MI_NetworkRequestDisconnect, pPacket);
        return;
    }
    LOG_DEBUG("enter game. account:" << proto.account().c_str());
    Proto::LoginByTokenRs protoLoginGameRs;
    protoLoginGameRs.set_return_code(Proto::LoginByTokenRs::LGRC_OK);
    MessageSystemHelp::SendPacket(Proto::MsgId::C2G_LoginByTokenRs, pPacket, protoLoginGameRs);
}
```

议。

作为一个测试登录的工程，game 进程收到这个协议之后进行了打印输出，没有实际做出正误判断，同

时发出一个返回协议 C2G_LoginByTokenRs，通知客户端已登录成功。关于 Robot 的状态以及状态的切换，这

里我们就不再多说了，感兴趣的读者可以看一下源代码。

1 0 . 1 . 3 　 P l a y e r 组 件

在 10_01_game 目录的工程中新增了一个 libplayer 库，它的目的是将逻辑与底层框架分开。在这个 libplayer 库

中，我们新建了几种数据便于以后使用，涉及地图的是 IWorld 类，涉及玩家的类有 Player 类以及玩家管理组件

PlayerCollectorComponent。关于玩家的类都是 Entity 类，也就是说这些类可以加载无数个组件。以往在编写逻辑代

码时，如果 Player 中有一个邮件系统信息，那么写法可能为“class Player{Mail_mail;}”，在 Player 中生成一个

Mail 实例或采用继承的关系“class Player：public Mail”来定义邮件数据。

现在要颠覆这个观点了，因为 Entity 类就是一个大容器。所谓容器，就是你给它什么，它就表现什么。一个空杯

子装上蓝色的水就是蓝色的，装上红色的水就是红色的。


```cpp
Player->AddComponent<MailComponent>();
```

要实现邮件系统，可以编写一个 MailComponent 加载到 Player 类中，代码如下：


```cpp
auto pMail = Player->GetComponent<MailComponent>();
```

使用时，用 GetComponent 函数取出：

使用组件的方式，代码显得更加简洁，还有另一个很好的用途就是重用。当我们把一些原子功能编写到组件内

时，这些组件就可以被不同的实体加载，以达到功能复用的目的。

在目录 10_01_game 工程的在 login 进程中，Account 类有 PlayerCollectorComponent 组件管理玩家。在 game 工程中

也给 Lobby 类加上了 PlayerCollectorComponent 组件。PlayerCollectorComponent 组件主要是管理 Player 的类，而

Player 类是一个空杯子，我们给这个空杯子装上什么组件，它就有什么特性。因此，即使是相同的类，在 login 进程和

game 进程中的表现完全不同。下面查看一下 PlayerCollectorComponent 的定义：


```cpp
public:
    Player* AddPlayer(NetworkIdentify* pIdentify, std::string account);
    void RemovePlayerBySocket(SOCKET socket);
    Player* GetPlayerBySocket(SOCKET socket);
    Player* GetPlayerByAccount(std::string account);
```

class PlayerCollectorComponent :public Component<PlayerCollectorComponent>, public IAwakeFromPoolSystem<>


```cpp
private:
    // <socket, Player*>
    std::map<SOCKET, Player*> _players;
    // <account, socket>
    std::map <std::string, SOCKET> _accounts;
};
```

...

在该组件中，我们维护了一个 Player 列表，保存了 Socket 与 Player 的对应关系。在 login 进程中，Player 维护的是

一些登录信息，这是 login 进程中所特有的，所以为这样的需求编写了 PlayerComponentAccount 组件，加载到 Player 实

体上。

同样是 Player 类，它在 game 进程中并没有 PlayerComponentAccount 组件，也不需要。

1 0 . 2

R e d i s 及 其 第 三 方 库

在 10.1 节中遗留了一个问题没有处理，就是 token 的生成以及验证方式。

只有将这部分内容完成，整个登录流程才算真正完成了。生成 token 是在 login

进程中，而验证 token 却是在 game 进程中，如何让一个数据在两个进程之间共

享，这是我们需要解决的问题。

为了解决这个问题，使用到了 Redis 内存数据库。现在有一个新的知识点

——Redis，先来看看它是什么。

和 MySQL 不同，Redis 是一个非关系型数据库，它是 Key-Value 数据库，由

一系列的列表和字典构成，既可持久化保存，又可以只保存在内存中。Redis

因为是内存数据库，性能比 MySQL 高一些，而 Redis 也提供锁机制，非常适合进

程之间的数据交互。除了用在 token 上外，任何需要进程交互或暂存数据的功

能都可以使用 Redis。

1 0 . 2 . 1 　 R e d i s 的 安 装

使用 Redis，除了安装 Redis 本身之外，还需要安装 hiredis 库，hiredis 库

主要提供编码时操纵 Redis 的一些 API 函数。在 Linux 下安装命令如下：

yum -y install --enablerepo=epel redis

yum -y install --enablerepo=epel hiredis

yum -y install --enablerepo=epel hiredis-devel

Redis 默认安装的配置文件位于/etc/redis.conf，使用 vim 打开该文件，

默认的端口为 6379。在 Linux 安装 Redis 还需要修改配置，找到 bind 127.0.0.1

改为 bind 192.168.0.120 127.0.0.1，其中 192.168.0.120 是本地 IP 地址，修

改成开启的虚拟机 IP 地址。这样，Redis 既可以被外部访问，又可以被本地访

问。外部访问时可以用于测试和查看数据。


```bash
[root@localhost ~]# systemctl start redis
```

启动 Redis 的命令如下：


```bash
[root@localhost ~]# systemctl enable redis
```

设置为开机启动的命令如下：

本书源代码的 dependence 目录中提供了一个已编译好的用于查看 Redis 数

据的工具 redis-desktop-manager。其 GitHub 主页为

https://github.com/uglide/RedisDesktopManager/releases。

如果没有特别指定，Redis 默认的数据库就指向 select（0），即 db0。默

认的 Redis 是没有账号和密码的，大部分情况下 Redis 在内部网络中使用，不需

要设置账号和密码。

在本书工程中使用 hiredis 时，需要修改工程的 CMakeLists.txt 文件，在

属性 target_link_libraries 上增加-lhiredis。

Windows 系统要操作 Redis，需要取得 hiredis 库的 Windows 版，其下载地址

为 https://github.com/MicrosoftArchive/redis/releases。本书源代码的

dependence 目录中提供了一个 3.2.100 版本。解压后进入 redis-win-

3.2.100\msvs 目录，打开 RedisServer.sln 工程，编译 Hiredis 和

Win32_Interop 工程，需要编译出 32 位版本。在编译之前修改运行库的设置，

设置为“多线程调试 DLL（/MDd）”。编译完成后，将生成的 hiredis.lib 和

Win32_Interop.lib 放入源代码目录 libs\windows 中，以便于后面的工程调

用。

除此之外，要在 Windows 系统中使用 Redis，还需要做一些善后工作：

• （1）打开 redis-win-3.2.100\deps 目录，将 hiredis 目录复制到根目录

include\windows 下，成为 include\windows\hiredis 子目录。

• （2）在 include\windows\hiredis 目录下新建一个 Win32_Interop 目录，

将位于 redis-win-3.2.100\src\Win32_Interop 目录下的文件

win32_types_hiredis.h 复制过来。

• （3）修改 hiredis.h 文件：


```bash
#include"../../src/Win32_Interop/win32_types_hiredis.h"。
```

·修改前：

·修改后：#include"Win32_Interop/win32_types_hiredis.h"。

这样做的目的是让头文件脱离原工程，如果是多人协作，不用每个人都下

载 Redis 编译修改，只需要调用编译好的 lib 和 include 目录就可以马上使用。

准备工作做完之后，还需要在使用 Redis 的工程中加入 lib 引用。目前使用

Redis 的工程为 login 和 game 工程。本节参考源代码位于 10_02_redis 目录中，

工程已配置完成。

1 0 . 2 . 2 　 R e d i s 命 令 行 命 令

在深入使用 Redis 之前，需要进一步了解什么是 Redis。Redis 是一个 Key-

Value 数据类型的数据库，数据存放在内存中，所以它的读取速度相当快，需

要时，它的数据也可以像 MySQL 一样保存在硬盘上。在 Linux 中，我们可以使用


```bash
[root@localhost ~]# redis-cli
```

redis-cli 来连接 Redis。下面具体来看 Redis 的一些常用命令。

127.0.0.1:6379>

首先执行 redis-cli 进入，因为没有修改默认端口，所以默认进入的本地

端口为 6379。使用--help 来查看 redis-cli 命令：

-h <hostname>      Server hostname (default: 127.0.0.1).

-p <port>          Server port (default: 6379).

假如要连接的 Redis 服务不在本地，可以使用“redis-cli-h-p”来指定 IP

地址和端口。下面做一个简单的 Redis 命令测试。


```bash
[root@localhost ~]# redis-cli
```

关键命令 1 ： set 和 get

127.0.0.1:6379>

进入 redis-cli，当我们输入 set 之后，会有一个灰色的提示“set key

value[EX seconds][PX milliseconds][NX|XX]”。我们可以如下使用 set：

127.0.0.1:6379> set mykey "hello world"

OK

这里的 set 命令相当于给 mykey 一个一对一的值，我们给出的值是一个字符

串“hello world”。然后使用 get 命令取出：


```cpp
"hello world"
```

127.0.0.1:6379> get mykey

关键命令 2 ： setex

命令 setex 的格式为：setex key seconds value。它是 set 的扩展，为一

个 key 赋值的同时设置了这个值的生存时间。这条命令非常有用：

127.0.0.1:6379> setex mykey2 3 token1

OK

将字符串 mykey2 设置为 token1，同时让它存活 3 秒。如果在 3 秒之内使用

get 命令，就会得到之前赋给它的值。


```cpp
"token1"
```

127.0.0.1:6379> get mykey2

如果在 3 秒之后获取，就返回空值。


```cpp
(nil)
```

127.0.0.1:6379> get mykey2

这个功能非常适合用来登录 token。token 一旦生成，为了安全起见，一段

时间之后就需要销毁。而销毁的这个过程由 Redis 自行控制，就不需要我们关

心了，非常省事。

关键命令 3 ： setnx

命令 setnx 的格式与 set 一样：setnx key value，用于对一个给定的 key 赋

值。它们的区别在哪里呢？setnx 是 SET if Not eXists 的缩写，如果不存在，

就设置，这是一个原子操作。如果 key 值已经存在，就返回出错信息。

127.0.0.1:6379> setnx key1 1

(integer) 1

127.0.0.1:6379> setnx key1 1

(integer) 0

第一次对 key1 赋值时，返回值为 1 表示正确;第二次赋值时，返回值为 0 表

示出错。使用 setnx 命令主要是用在账号登录上，它就像是一个开关，当某个

账号在线时，如果该账号再次登录，setnx 就会出错。

关键命令 4 ： expire

命令 expire 的格式为：expire key seconds，是对一个已存在的键

（Key）设置生存周期。例如，我们登录时用 setnx 产生的键，就可以对它进行

一个 expire 存活时间设置。

目前学习这几条基本的命令即可，其他的命令可以参见 Redis 官方网站。

关于这些命令的具体使用，在后面的章节还会讲解到。

1 0 . 2 . 3 　 h i r e i d s 库

在 Linux 和 Windows 系统上都使用了第三方库 hiredis。这个库和 Redis 的关系就好像 MySQL

Connector/C 库与 MySQL 的关系。hiredis 库提供 API 函数对数据库进行访问。先了解一下它的几个重要函

数：

关键函数 1 ： redisConnectWithTimeout

作为了一个数据库，当然必不可少就是连接函数。hiredis 库提供的连接函数为


```cpp
redisContext *redisConnectWithTimeout(const char *ip, int port, const struct timeval tv);
```

redisConnectWithTimeout，函数参数如表 10-1 所示，函数原型如下：

表 10-1 redis 连接函数参数


```cpp
struct timeval {
__time_t tv_sec;        /* Seconds. */
__suseconds_t tv_usec;  /* Microseconds. */
};
```

结构 timeval 定义如下：

tv_sec 为秒数，tv_usec 为微秒数。例如，struct timeval outTime={1，500000};，即为 1.5 秒，

500 000 微秒为 0.5 秒。

调用 redisConnectWithTimeout 函数之后，其返回值为 redisContext 指针，若为 null，则表示不成

功;若不为 null，并且返回的 redisContext 中的 err 不为 null，则也认为是不成功的，此时需要检查一

下返回值中的出错标志。

关键函数 2 ：释放连接函数


```cpp
void redisFree(redisContext *c);
```

不再用 Redis 时，需要手动释放之前的连接实例，其原型如下：

参数 redisContext 的指针是创建时生成的指针。

关键函数 3 ：命令执行函数

操作 Redis 数据时，一般情况下都是采用命令方式。执行 Redis 命令的函数为 redisCommand，类似于

MySQL 的 SQL 语句的执行。其函数原型有几种模板：


```cpp
void *redisCommand(redisContext *c, const char *format, va_list ap);
void *redisCommand(redisContext *c, const char *format, ...);
void *redisCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen);
```

函数 redisvCommand 返回 redisReply 指针。向 Redis 查询数据，返回结果类都是 redisReply 指针，但

想象一下，我们查询一个数据，有时候可能想得到一个字符串，有时候可能想得到一个数值，怎么来区


```bash
#define REDIS_REPLY_STRING 1
#define REDIS_REPLY_ARRAY 2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_NIL 4
#define REDIS_REPLY_STATUS 5
#define REDIS_REPLY_ERROR 6
```

分呢？redisReply 类中提供一个名为 type 的属性，可以得到执行的结果的类型，返回类型定义如下：

分别表示字符串、数据、数值、空、状态和出错几种类型。后面具体用到的时候，我们再深入讲解

这些类型。

关键函数 4 ：释放查询结果

通过 redisCommand 函数得到的结果指针 redisReply 不使用时需要释放，调用函数 freeReplyObject


```cpp
void freeReplyObject(void *reply);
```

释放。其原型如下：

综上，基本常用的就是这几个函数。下面通过具体的编码来了解 hiredis 库中的这些函数的使用。

1 0 . 2 . 4 　 组 件 R e d i s C o n n e c t o r

本节的源代码位于目录 10_02_redis 中。为了更好地使用 Redis，为它编写了一个新组件 RedisConnector，源代码


```cpp
class RedisConnector :public Entity<RedisConnector>, public IAwakeSystem<> {
public:
    void Awake() override;
    bool Connect();
    bool Disconnect();
    ...
protected:
    bool Ping() const;
    bool Setex(std::string key, std::string value, int timeout) const;
    bool SetnxExpire(std::string key, int value, int timeout) const;
    void Delete(std::string key) const;
    ...
};
```

位于 libserver 工程的 redis_connector.h 文件中，其定义如下：

组件 RedisConnector 从对象池中被唤醒时进行了数据库的连接，连接目的地配置在 engine.yaml 文件中，组件


```cpp
void RedisConnector::Awake() {
    ...
    auto pYaml = ComponentHelp::GetYaml();
    const auto pConfig = pYaml->GetConfig(APP_DB_MGR);
    auto pDBConfig = dynamic_cast<DBMgrConfig*>(pConfig);
    auto pRedisConfig = pDBConfig->GetDBConfig(DBMgrConfig::DBTypeRedis);
    if (pRedisConfig == nullptr) {
        LOG_ERROR("Init failed. get redis config is failed.");
        return;
    }
    _ip = pRedisConfig->Ip;
    _port = pRedisConfig->Port;
    // time function
    AddTimer(0, 2 * 60, false, 0, BindFunP0(this, &RedisConnector::CheckPing));
    Connect();
}
```

RedisConnector 的初始代码如下：

初始化时指定了一个定时器，从绑定函数名可以看出进行了 ping 操作。该定时器每两分钟进行一次 ping 操作。在


```cpp
bool RedisConnector::Connect() {
    const struct timeval outTime = { 1, 500000 };  // 1.5 seconds
    redisContext* c = redisConnectWithTimeout(_ip.c_str(), _port, outTime);
    if (c == nullptr || c->err) {
        if (c) {
            LOG_ERROR("RedisConnector::Connect. errno=" << c->err << ", error=" << c->errstr);
            redisFree(c);
        } else {
            LOG_ERROR("RedisConnector error: can't allocate redis context");
        }
        return false;
    }
    // 选择 (1) 号数据库
    int db_index = 1;
    std::string sql = strutil::format("select %d", db_index);
    redisReply* pRedisReply = static_cast<redisReply*>(redisCommand(c, sql.c_str()));
    if (nullptr == pRedisReply) {
        LOG_ERROR("RedisConnector::Connect. errno=" << c->err << ", error=" << c->errstr);
        redisFree(c);
        return false;
    }
    if (!(pRedisReply->type == REDIS_REPLY_STATUS && strncmp(pRedisReply->str, "OK", 2) == 0)) {
        LOG_ERROR("RedisConnector::Connect. errno=" << c->err << ", error=" << c->errstr);
        freeReplyObject(pRedisReply);
        redisFree(c);
        return false;
    }
    freeReplyObject(pRedisReply);
    _pRedisContext = c;
    LOG_DEBUG("\tRedisConnector::Connect: successfully!");
```

初始化时，连接数据库的函数如下：


```cpp
    return Ping();
}
```


```cpp
bool RedisConnector::Ping() const {
    if (_pRedisContext == nullptr)
        return false;
    redisReply* pRedisReply = static_cast<redisReply*>(redisCommand(_pRedisContext, "PING"));
    if (nullptr == pRedisReply) {
        LOG_ERROR("RedisConnector::ping: errno=" << _pRedisContext->err << " error=" << _pRedisContext->e
        return false;
    }
    if (!(pRedisReply->type == REDIS_REPLY_STATUS && strncmp(pRedisReply->str, "PONG", 4) == 0)) {
        LOG_ERROR("RedisConnector::ping: errno=" << _pRedisContext->err << " error=" << _pRedisContext->e
        freeReplyObject(pRedisReply);
        return false;
    }
    freeReplyObject(pRedisReply);
    return true;
}
```

连接时使用了 hiredis 的 redisConnectWithTimeout 函数，成功连接之后做了一个 ping 调用：

在上面的代码中，使用了 hiredis 中的 redisCommand 函数，该函数执行了一条命令。在 redisCommand 中使用的命令


```bash
[root@localhost ~]# redis-cli
```

可以在 Shell 中使用。例如，上面使用的 ping 可以在 Shell 中使用，结果如下：

127.0.0.1:6379> ping

PONG

在调用了 redisCommand（_pRedisContext，"PING"）代码之后，返回 redisReply 指针。在 RedisConnector::

Ping 函数中，判断返回值是否是一个 PONG，非 PONG 值都认为是失败。如果 ping 失败了，就会在定时器中再次重新连

接。

组件 RedisConnector 作为 Redis 的基础组件，另外还提供了一些常用函数，如 Setex 函数就是将 setex 命令进行了封

装，更多细节不再一一细述。下面来看 Redis 在框架中的应用。

1 0 . 2 . 5 　 R e d i s 在 l o g i n 中 的 应 用

前面有一个问题没有处理。如果一个玩家用两个客户端登录同一个账号，如果有两 login 进程这两次登录既可能同

时落在同一个 login 进程上，又可能落在两个 login 进程上。如果落在不同的 login 进程上，那么它们之间肯定只有一个

能登录，我们如何来限制这种事情发生呢？

这就是本节要用 Redis 处理的问题，如何跨进程检查账号是否在线。

本节的源代码位于目录 10_02_redis 中。在 10.2.4 小节，介绍了在 libserver 库中编写了一个 RedisConnector 组


```cpp
class RedisLogin : public RedisConnector {
private:
    void RegisterMsgFunction() override;
    void HandleLoginTokenToRedis(Packet* pPacket);
    void HandleAccountQueryOnline(Packet* pPacket);
    void HandleAccountSyncOnlineToRedis(Packet* pPacket);
    void HandleAccountDeleteOnlineToRedis(Packet* pPacket);
};
```

件，现在派上了用场。在 login 的工程中创建一个 RedisLogin 组件，其定义在 redis_login.h 文件中，定义如下：

RedisLogin 类继承自基础的 RedisConnector 类，有 3 个任务：第一个任务是验证账号是否在线;第二个任务是生成

可以登录 game 进程的 token;第三个任务是给在线账号生成在线标志。下面逐一进行说明。

> **关键点 1：验证账号是否在线**

当收到一个客户端验证账号的协议时，Account 类会转发一个协议向 Redis 询问登录的账号是否有相同的账号在


```cpp
void RedisLogin::HandleAccountQueryOnline(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::AccountQueryOnlineToRedis>();
    auto account = proto.account();
    Proto::AccountQueryOnlineToRedisRs protoRs;
    protoRs.set_account(account.c_str());
    protoRs.set_return_code(Proto::AccountQueryOnlineToRedisRs::SOTR_Offline);
    // 是否正在登录
    if (!SetnxExpire(RedisKeyAccountOnlineLogin + proto.account(), Global::GetInstance()->GetCurAppId(),
        protoRs.set_return_code(Proto::AccountQueryOnlineToRedisRs:: SOTR_Online);
    // Game 是否在线
    if (GetInt(RedisKeyAccountOnlineGame + proto.account()) != 0)
        protoRs.set_return_code(Proto::AccountQueryOnlineToRedisRs:: SOTR_Online);
    MessageSystemHelp::DispatchPacket(Proto::MsgId:: MI_AccountQueryOnlineToRedisRs, protoRs, nullptr);
}
```

线，协议号为 MI_AccountQueryOnlineToRedis。组件 RedisLogin 收到了这个协议，它是这样处理的：

在这个函数中，向 Redis 数据库查询了两条数据：第一条是 RedisKeyAccountOnlineLogin;第二条是

RedisKeyAccountOnlineGame。从名字上可以看出，第一条是想知道这个账号是否在 login 进程上，第二条则是查看它


```bash
#define RedisKeyAccountOnlineLogin  "engine::online::login::"  // 角色登录
#define RedisKeyAccountOnlineLoginTimeout  6 * 60
#define RedisKeyAccountOnlineGame  "engine::online::game::"    // 角色在线
#define RedisKeyAccountOnlineGameTimeout  6 * 60
```

是否在 game 进程上。看看如下两个定义：

使用这两条数据时采用了字典型（含键值对的数据，Key-Value Pair）的数据库。它们的格式都是 Key::账号

名。每个账号不同，Redis 数据库中保存的键也不同，例如以账号名为 test 的账号登录，则它在 login 进程中的在线标

志位为 engine::online::login::test，也就是上面定义的 RedisKeyAccountOnlineLogin+账号名，组成了字典

中的键。

在查询过程中调用了 SetnxExpire 函数，用于查询当前账号是否在其他 login 上登录过。所有的 login 进程连接的是

同一个 Redis，如果登录过，那么该键（Key）必有值（Value）。函数 SetnxExpire 的实现如下：


```cpp
bool RedisConnector::SetnxExpire(std::string key, const int value, const int timeout) const {
    if (_pRedisContext == nullptr) {
        LOG_WARN("RedisContext == nullptr. connect failed.");
        return false;
    }
    std::string command = strutil::format("SETNX %s %d", key.c_str(), value);
    redisReply* pRedisReply = static_cast<redisReply*>(redisCommand (_pRedisContext, command.c_str()));
    if (pRedisReply->type != REDIS_REPLY_INTEGER || pRedisReply->integer != 1){
        // 没有设置成功，可能已经有值，也可能 Redis 挂掉，都认为已经在线
        LOG_COLOR(LogColorPurple, "[SETNX] failed 1. command:" << command.c_str() << " pRedisReply->type:
        freeReplyObject(pRedisReply);
        return false;
    }
    freeReplyObject(pRedisReply);
    // 成功之后，为这个值设置一个时间限制，在这个时间内不能重复登录
    command = strutil::format("EXPIRE %s %d", key.c_str(), timeout);
    pRedisReply = static_cast<redisReply*>(redisCommand(_pRedisContext, command.c_str()));
    if (pRedisReply->type != REDIS_REPLY_INTEGER || pRedisReply->integer != 1){
        // 没有设置成功，可能有异常，可能 Redis 挂掉，都认为已经在线
        LOG_COLOR(LogColorPurple, "[EXPIRE] failed 2. command:" << command.c_str() << " pRedisReply->type
        freeReplyObject(pRedisReply);
        return false;
    }
    freeReplyObject(pRedisReply);
    return true;
}
```

在上面的函数中使用了 setnx 和 expire 两个命令，它们在前面的章节中都已经讲过。如果两个玩家在两个 login 进

程上同时登录，他们同时触发 setnx 命令向同一个键（Key）写值（Value），其中有一个必定会失败。因为 setnx 是一

个原子操作，它本身是带锁的，执行该命令必定是一个串行操作，如果第一个操作成功，再想要写入值，就必定会失

败。

命令 setnx 执行成功之后，再调用 expire 函数给键加一个限时销毁的时长，此后游戏逻辑中不再对这个数据进行操

作时，由 Redis 去分担一部分销毁工作。

函数 SetnxExpire 既完成了查询，又完成了设值。这样就可以保证同一账号不会在多个 login 上登录。

> **关键点 2：生成token**

当所有的验证选择角色完成之后，需要生成一个 token，这个 token 需要在 login 与 game 进程中同时访问，所以将它

保存在 Redis 中成为一个很好的选择。在 Account 类中，验证账号成功之后，将发起一个 MI_LoginTokenToRedis 协议，


```cpp
void RedisLogin::HandleLoginTokenToRedis(Packet* pPacket) {
    auto protoToken = pPacket->ParseToProto<Proto::LoginTokenToRedis>();
    auto account = protoToken.account();
    auto playerSn = protoToken.player_sn();
    auto token = Global::GetInstance()->GenerateUUID();
    // 将 tokeninfo 序列化为一个串，存入 Redis
    Proto::TokenInfo protoInfo;
    protoInfo.set_token(token);
    protoInfo.set_player_sn(playerSn);
    std::string tokenString;
    protoInfo.SerializeToString(&tokenString);
    const std::string key = RedisKeyAccountTokey + account;
    const int timeoue = RedisKeyAccountTokeyTimeout;
    if (!Setex(key, tokenString, timeoue)) {
        token = "";
        LOG_ERROR("account:" << account.c_str() << ". failed to set token.");
    }
    // 将生成的 token 返回给 Account 类
    Proto::LoginTokenToRedisRs protoRs;
    protoRs.set_account(account.c_str());
    protoRs.set_token(token.c_str());
    MessageSystemHelp::DispatchPacket(Proto::MsgId::MI_LoginTokenToRedisRs, protoRs, nullptr);
}
```

该协议是根据账号和所选角色生成一个 token，该协议的处理函数如下：

在上面的代码中，生成 token 使用了 Global::GenerateUUID，可以参见源代码。这里使用了两套方案：在

Windows 下使用的是 GUID;在 Linux 下使用的是 UUID。如果 Linux 环境下还没有安装 UUID，那么需要安装 UUID，只需执行

命令行：yum install libuuid-devel。在上面的代码中生成 token 之后，调用 Setex 函数发送了一条 setex 命令。


```bash
#define RedisKeyAccountTokey  " engine::token::"
#define RedisKeyAccountTokeyTimeout  2 * 60
```

该命令的键定义如下：

对于一个名为 test 的账号，生成的 token 键是 engine::token::test。而键的值是一个 string。该 string 是由


```cpp
bool RedisConnector::Setex(std::string key, std::string value, const int timeout) const {
    const std::string command = strutil::format("SETEX %s %d %s", key.c_str(), timeout, value.c_str());
    return Setex(command);
}
bool RedisConnector::Setex(std::string command) const {
    redisReply* pRedisReply = static_cast<redisReply*> (redisCommand(_pRedisContext, command.c_str()));
    if (pRedisReply->type != REDIS_REPLY_STATUS || strncmp(pRedisReply->str, "OK", 2) != 0) {
        LOG_COLOR(LogColorPurple, "[SETEX] failed. command:" << command.c_str() << " pRedisReply->type:"
        freeReplyObject(pRedisReply);
        return false;
    }
    freeReplyObject(pRedisReply);
    return true;
}
```

Proto::TokenInfo 结构序列化而来的，其中包括 token 与角色 Id。函数 Setex 实现如下：

命令 setex 自带一个有效时间，在存储 token 时，采用的有效时间为 RedisKeyAccountTokeyTimeout 定义的 2 分钟。

> **关键点 3：写入在线标志**

通过 Redis，在登录时可以保证同一账号进行串行登录，但是它依然有一个潜在的风险，这个登录标记有一个时

限。分析代码可以发现 RedisKeyAccountOnlineLogin 这个键在玩家要求进行账号验证之前就产生了，但是账号验证是

一个不确定的事件，向外部发起一个 HTTP 请求。假设我们将 RedisKeyAccountOnlineLogin 键的超时设为 10 秒，10 秒之

后 HTTP 请求还没有返回，Redis 会将这个键销毁。但真实情况可能是在第 11 秒 HTTP 返回结果，这中间有 1 秒的时间差，

如果在这 1 秒内，该账号再次发起登录，就会产生异常。

因此，需要有一种机制，只要玩家还在 login 进程上，就定时不断地向 Redis 写入在线的标志。将这个功能交给一

个新组件 PlayerComponentOnlineInLogin 来完成。这个组件在 Redis 验证成功之后、HTTP 验证开始之前被加载到 Player


```cpp
void Account::HandleAccountQueryOnlineToRedisRs(Packet* pPacket) {
    auto protoRs = pPacket->ParseToProto<Proto:: AccountQueryOnlineToRedisRs>();
    ...
    // 在线组件
    pPlayer->AddComponent<PlayerComponentOnlineInLogin> (pPlayer-> GetAccount());
    // 验证账号，发起一个 HTTP 请求
    MessageSystemHelp::CreateConnect(NetworkType::HttpConnector, pPlayer->GetObjectKey(), _httpIp.c_str()
}
```

对象上。

该组件被初始化时，向时间堆写入了每隔一段时间就要调用的函数。这个时间正好是 Redis 设置在线标志销毁的一


```cpp
void PlayerComponentOnlineInLogin::Awake(std::string account) {
    _account = account;
    AddTimer(0, (int)(RedisKeyAccountOnlineLoginTimeout*0.5), true, 0, BindFunP0(this, &PlayerComponentOn
}
void o ::SetOnlineFlag() const {
    // 设置在线标志
    Proto::AccountSyncOnlineToRedis protoSync;
    protoSync.set_account(_account.c_str());
    MessageSystemHelp::DispatchPacket (Proto::MsgId::MI_AccountSyncOnlineToRedis, protoSync, nullptr);
}
```

半时间，即在 Redis 要销毁在线标志之前会重新写入在线标志，并将它的时间重置。

函数 SetOnlineFlag 的任务就是向 Redis 发送一条在线标志设置消息，而 Redis 组件收到这个消息之后调用了 Setex


```cpp
void RedisLogin::HandleAccountSyncOnlineToRedis(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::AccountSyncOnlineToRedis>();
    const std::string key = RedisKeyAccountOnlineLogin + proto.account();
```

函数。


```cpp
    const std::string value = std::to_string(Global::GetInstance() ->GetCurAppId());
    Setex(key, value, RedisKeyAccountOnlineLoginTimeout);
}
```

函数 Setex 会将键存活时间进行重新设置。综上所述，只要账号还在 login 进程中，Redis 中的在线标志就会一直存

在。基于此，如果同一账号再次登录，就会发现在 Redis 中这个在线标志已经存在，不能再次登录了。

> **关键点 4：效果测试**

打开 log4_help.h 文件，打开宏 LOG_REDIS_OPEN。编译工程，我们用 robots 登录看一下数据。使用 robots 登录账号


```cpp
[DEBUG] -   @@ SETNX. key:SETNX engine::online::login::test 0
[DEBUG] -   @@ EXPIRE. key:EXPIRE engine::online::login::test 360
[DEBUG] -   @@ SETEX. key:SETEX engine::online::login::test 360 0
```

test，可以看到有如下的打印输出：


```cpp
[DEBUG] -   @@ DEL. key:del engine::online::login::test
[DEBUG] -   @@ DEL. key:del engine::token::test
[DEBUG] -   @@ SETEX. key:SETEX engine::online::game::test 360 1
```

[DEBUG] -  account:test. gen token:03f291ba-b401-4406-892b-941f6d7dc152

打印信息的最后两步是由 game 进程产生的，后面再进行讨论。Redis 的流程示意图如图 10-4 所示。

**图 10-4 Redis生成token**

首先，向 Redis 执行了一个 SETNX 原子操作，使用 EXPIRE 为这个值在线标志设置了生命周期 360 秒。随着组件

PlayerComponentOnlineInLogin 的生成，为 engine::online::login::test 键重置了它的生命周期，随后，生成

了一个 token 值。账号 test 登录 game 进程之后，组件 PlayerComponentOnlineInLogin 被销毁，导致了 DEL.key：del

engine::online::login::test 的发生。

1 0 . 2 . 6 　 R e d i s 在 g a m e 中 的 应 用

客户端登录 game 进程发起的第一个协议是 C2G_LoginByToken，处理该协议的类为 Lobby 类。连接上 game 进程之后，

就可以发起该协议，发送 token 以请求登录。怎么理解这个 token 呢？我们登录 login 时发送了账号和密码，这里的

token 也扮演着这样的角色。只是这个 token 的过程玩家并不知道。

在进程 game 中，为了处理请求 token 的协议增加了一个 RedisGame 类，这个类与之前的 RedisLogin 的原理是一致

的。RedisGame 有两个任务：第一个任务是查询已知的账号是否存在 token;第二个任务是为当前在 game 进程中的玩家

设置在线标志。

> **关键点 1：查询一个已知账号的token**

处理 C2G_LoginByToken 协议时，从客户端传来的 token 暂存到了 PlayerComponentToken 组件中，对于 Lobby 来说，

它并不知道 Redis 数据在哪里，所以向系统广播了 MI_GameTokenToRedis 协议，向其他组件请求当前账号的 token 值。


```cpp
void Lobby::HandleLoginByToken(Packet* pPacket) {
    ...
    // 添加组件
    pPlayer->AddComponent<PlayerComponentToken>(proto.token());
    pPlayer->AddComponent<PlayerComponentOnlineInGame> (pPlayer->GetAccount(), 1);
    // 请求 Token
    Proto::GameTokenToRedis protoToken;
    protoToken.set_account(pPlayer->GetAccount().c_str());
    MessageSystemHelp::DispatchPacket(Proto::MsgId::MI_GameTokenToRedis, protoToken, nullptr);
}
```

Lobby 中对于 C2G_LoginByToken 的处理实现如下：


```cpp
void RedisGame::HandleGameTokenToRedis(Packet* pPacket) {
    auto protoToken = pPacket->ParseToProto<Proto::GameTokenToRedis>();
    Proto::GameTokenToRedisRs protoRs;
    protoRs.set_account(protoToken.account().c_str());
    const std::string tokenValue = GetString(RedisKeyAccountTokey + protoToken.account());
    protoRs.mutable_token_info()->ParseFromString(tokenValue);
    Delete(RedisKeyAccountTokey + protoToken.account());
    MessageSystemHelp::DispatchPacket(Proto::MsgId::MI_GameTokenToRedisRs, protoRs, nullptr);
}
```

MI_GameTokenToRedis 协议发出之后，是由 RedisGame 组件来处理的，实现如下：


```cpp
std::string RedisConnector::GetString(std::string key) const {
    std::string strRs = "";
    std::string command = strutil::format("get %s", key.c_str());
    redisReply* pRedisReply = static_cast<redisReply*>(redisCommand (_pRedisContext, command.c_str()));
    if (pRedisReply->type != REDIS_REPLY_STRING) {
        LOG_WARN("[GET failed] execute command:" << command.c_str() << " pRedisReply->type:" << pRedisRep
    } else {
        strRs = pRedisReply->str;
    }
    freeReplyObject(pRedisReply);
    return strRs;
}
```

在查询 Redis 时用到了 GetString 函数，token 字段保存的是 Proto::TokenInfo 结构的序列化字符串。

在上面的代码中使用了 Redis 的 get 命令，GetString 是对这个命令的一个封装。如果插入的值是 int，返回值就是

REDIS_REPLY_INTEGER。之前插入的对象是序列化的一个字符串，所以这里检查一下返回值类型是不是

REDIS_REPLY_STRING，得到字符串之后再反序列化。得到 token 数据之后，将数据设置到协议结构 Proto::


```protobuf
message TokenInfo {
    string token = 1;
    uint64 player_sn = 2;
}
message GameTokenToRedisRs {
    string account = 1;
    TokenInfo token_info = 2;
}
```

GameTokenToRedisRs 中，看一下 msg.proto 中关于该结构的定义：

在 game 进程向 Redis 查询协议的返回结构中包容了有一个 TokenInfo 结构。回到 RedisGame::

HandleGameTokenToRedis 函数创建中来，得到 TokenInfo 之后注意，这里我们调用了一个 Delete 函数，删除了键

（RedisKeyAccountTokey+protoToken.account（））。token 只能使用一次，使用完成之后，将这个键对应的数据删

除。

Lobby 类接到 RedisGame 返回的 MI_GameTokenToRedisRs 协议，其处理函数为 Lobby::

HandleGameTokenToRedisRs，首先要做的就是验证玩家数据的合法性，前面我们已经反复强调了，每一步都是异步操

作。当协议回到 Lobby 类中时，不确定这个玩家是否还在线，有可能已经断线了，所以需要进行一个账号的确认。确认

之后，取出之前暂存的 PlayerComponentToken 组件，对两个 token 进行比较，如果出错，就返回给客户端一个出错编

号;如果没有出错，就打印一句“enter game”。至此，玩家从 login 进程成功地登录 game 进程。

> **关键点 2：设置在线标志**

回看之前的代码，在 login 进程收到 C2L_AccountCheck 登录协议时，先向 Redis 发起了一个查询。这个查询分为两

部分：一部分是检查账号是否在 login 进程中;另一部分是检查账号是否在 game 进程中。

当账号处于正在登录阶段时，login 进程为每一个正在登录的 Player 对象绑定了一个组件

PlayerComponentOnlineInLogin，该组件的任务是定时向 Redis 写入在线标志，标志该对象在 login 进程上。game 进程

中的在线标志设置原理和 login 进程中完全一致，其组件名为 PlayerComponentOnlineInGame。只要玩家一直与 game 进

程保持通信状态，组件 PlayerComponentOnlineInGame 就会定时向 Redis 中写入在线标志。

更多细节可以参见 10_02_redis 目录中的源代码。

1 0 . 2 . 7 　 从 R e d i s 删 除 数 据

在 login 和 game 进程中有两个组件定时向 Redis 写入账号在线标志，当一个机器人类 Robot 退出时，可以看到类

似“@@DEL.key：del engine::online::game::test”这样的打印信息。程序主动删除了 Redis 中的在线标志，

那么这些标志是如何被销毁的呢？

在 Lobby 类中注册了一个函数来处理来自网络层的消息 MI_NetworkDisconnect，该消息表示有一个 Socket 被关闭


```cpp
void Lobby::Awake() {
    ...
    auto pMsgSystem = GetSystemManager()->GetMessageSystem();
    pMsgSystem->RegisterFunction(this, Proto::MsgId::MI_NetworkDisconnect, BindFunP1(this, &Lobby::Handle
}
```

了，以 Lobby 为例的代码如下：

当一个玩家连接到 game 进程又断开了网络时，底层会分发 MI_NetworkDisconnect 协议，这正好是 Lobby 关心的协议


```cpp
void Lobby::HandleNetworkDisconnect(Packet* pPacket) {
    GetComponent<PlayerCollectorComponent>()->RemovePlayerBySocket (pPacket->GetSocketKey().Socket);
}
void PlayerCollectorComponent::RemovePlayerBySocket(SOCKET socket) {
    const auto iter = _players.find(socket);
    if (iter == _players.end())
        return;
    Player* pPlayer = iter->second;
    _players.erase(socket);
    _accounts.erase(pPlayer->GetAccount());
    GetSystemManager()->GetEntitySystem()->RemoveComponent(pPlayer);
}
```

之一，处理函数为 HandleNetworkDisconnect。下面是 Lobby 类收到 Socket 断开时的处理方式：

从代码来看，找到这个断开的 Socket 对应的玩家，然后将这个玩家从组件 PlayerCollectorComponent 中移除，以

释放内存。那么它是如何触发删除在线标志的呢？这里需要注意，实体销毁时，其附加的组件会同时全部销毁。

在 RemovePlayerBySocket 函数中，移除玩家时调用了 GetEntitySystem::RemoveComponent 函数，这个函数会将

当前实体以及在实体上的所有组件移除，同时调用每个组件的 BackToPool 函数。而 Player 类的


```cpp
void PlayerComponentOnlineInGame::BackToPool() {
    Proto::PlayerDeleteOnlineToRedis protoSync;
    protoSync.set_account(_account.c_str());
    protoSync.set_version(_onlineVersion);
    MessageSystemHelp::DispatchPacket(Proto::MsgId:: MI_PlayerDeleteOnlineToRedis, protoSync, nullptr);
}
```

PlayerComponentOnlineInGame 组件在销毁时发送了 Redis 数据销毁的协议，其实现如下：


```cpp
void RedisGame::HandlePlayerDeleteOnlineToRedis(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::PlayerDeleteOnlineToRedis>();
    auto curValue = proto.version();
    const std::string key = RedisKeyAccountOnlineGame + proto.account();
    const auto onlineVersion = this->GetInt(key);
    if (curValue < onlineVersion)
        return;
    Delete(key);
}
void RedisConnector::Delete(std::string key) const {
    // Del 返回值是被删除的数量
    std::string command = strutil::format("del %s", key.c_str());
    redisReply* pRedisReply = static_cast<redisReply*>(redisCommand (_pRedisContext, command.c_str()));
    ...
    freeReplyObject(pRedisReply);
}
```

MI_PlayerDeleteOnlineToRedis 协议的处理函数在 RedisGame 组件中。

在删除过程中，调用基类函数 RedisConnector::Delete 删除了一个键。删除的时候，使用 hiredis 库的

redisCommand 函数执行了一个 del 命令。

这一系列的流程彻底地删除了玩家位于内存和 Redis 中的所有数据。在 login 进程中的在线标志也是一样的删除流

程。

1 0 . 3

性 能 瓶 颈 分 析

在目前的框架中，已经让玩家登录了 game 进程，生成了 token，并做出了

验证。前面的章节中增加了许多功能，加入了 HTTP 服务，加入了 libplayer

库，建立了 game、space 工程，等等。

本节缓和一下学习节奏，再来讨论一下性能。在 10_01_game 工程中，同时

登录 1000 个账号，在 Windows 下的效率已经非常缓慢了，登录 1000 个账号至少

花费了在 Linux 下的 20 倍时间。要么是我们在设计上有欠缺，要么就是有瓶颈

产生。

如果在开发中产生了瓶颈，但又不知道瓶颈在哪里，那么可以使用本节介

绍的通过查看日志来找出瓶颈的方法。

1 0 . 3 . 1 　 使 用 日 志 查 看 瓶 颈

当我们面对一个性能瓶颈的时候，有时候即使使用了工具也找不到产生瓶颈的原因，更

不知道该如何优化程序。这里提供一种思路，就是对现有框架的数据进行实时采集与分析，

了解数据之后才能分析出问题的本源。

以 10_01_game 目录中的工程为例，在工程中准备了一个宏 LOG_TRACE_COMPONENT_OPEN，

用于跟踪 Socket 的日志。这个宏在 libserver 工程的 trace_component.h 文件中。这个宏在工

程中默认是打开的。我们来看看它有什么作用？

在 Windows 系统下运行 allinone 进程，再批量登录 500 个账号。登录完成后，robots 进程

会出现“Login-SelectPlayer over”的提示。现在可以查看这些数据了，在 robots 控制台输

入“trace-account test123”，表示想要看账号 test123 使用的所有 socket 值。

trace -account test123

socket: 884 1160

账号 test123 在整个登录过程中使用了两个 Socket：一个是 884;另一个是 1160。我们可

以分别看看两个 Socket 上的一些关键信息，输入“trace-connect socket”，可看到如下的

信息：

trace -connect 884

key:884

...21:03:06.610  create connect != 0 waiting, err=10035 network type:HttpConnector

...21:03:06.610  create. network type:HttpConnector

...21:03:09.636  close.  network type:HttpConnector

...21:03:09.636  create connect != 0 waiting, err=10035 network type:TcpConnector

...21:03:09.636  create. network type:TcpConnector

从上面的信息可知，在 884 这个 Socket 上使用了两次：一次是 HTTP 请求;另一次是 TCP 请

求。当前这个 TCP 请求还没有断开，说明 test123 使用的可能是 TCP。而 HTTP 请求时的 Socket 可

能来自另一个账号。再来看看在 Socket 值 884 上的一些状态：

trace -player 884

key:884

...21:03:07.886  enter state:Http-Connectted

...21:03:07.886  http connected. account:test55 socket:884

...21:03:08.905  enter state:Login-Connectting

...21:03:10.285  enter state:Login-Connectted

...21:03:10.285  send check account. account:test123 socket:884

...21:03:12.710  enter state:Login-SelectPlayer

在前 3 行中，Socket 值 884 是一个 HTTP 请求，它的关闭时间是 09 分，到了 10 分时，test123

使用了值为 884 的 Socket，两分钟之后，接收数据完成并进入了 SelectPlayer 状态。

trace 这条命令在 allinone 进程上也可以使用，要查看 trace 命令的更多使用方法，可以

执行命令“trace-help”。使用“trace-packet 884”可以看到值为 884 的 Socket 发送与接收

过的所有协议，因为太长了，这里就不再一一列举出来了。

Trace 命令只是用于辅助工作，我们不占用篇幅来详细说明它的实现，有兴趣的读者可以

直接查看源代码。但是看过这些日志，分析一下性能消耗的原因，可能发生在网络层。为什

么从连接成功到选择玩家协议中间花了两分钟？显然是不合理的。

以 MI_NetworkConnected 协议为例，这个协议的作用是当网络连接成功时向目标对象发送

成功的消息，并附带 SocketKey。现在我们有 1000 个 Robot 和两次连接（一次是 HTTP 连接，另

一次是 TCP 连接），在很短的时间内发送了 2000 次 MI_NetworkConnected 协议。这个协议的注

册函数使用的是过滤对象的方式，也就是说这 2000 个协议产生了 2000×1000 次函数对比。协

议是无差别地发送到各个对象上的，对这个协议感兴趣的对象 Robot 类有 1000 个，一个协议要

对比 1000 次才能找到自己真正的目的地，即使第一个对象调用 GetPacketObject 就找到了目标

对象，但是后面的 999 次调用依然会产生，并没有机制让 Packet 找到了目标对象之后就退出分

发这个流程。因此，有 2000×1000 次函数调用。而且每个 Packet 都会对所处线程中的对象询

问是否对自己的协议号感兴趣，轮询的方式比较耗时。

通过上面的分析，相信读者已经发现了问题所在。显然 Robot 类作为注册处理协议的实

体，这个设计思路是不行的。要解决这个问题有两个办法：第一个办法是在 Robot 类上再增加

RobotCollection 类，用于在线程中管理 Robot，由 RobotCollection 类来处理协议，这时协议

到来时其命中率将大大增加，只需要调用一次 GetPacketObject 就能够找到目标对象;第二个

办法是做一个类似 NetworkLocation 的定位组件，以达到一步找到目标对象的目的。

本书中反复提到了性能测试的重要性。我们在编码的时候常常忽视这一环节，并不是功

能效果达到了，功能就做完了。

本节的参考源代码目录为 10_02_redis。在 robots 工程中，每个线程增加了一个

RobotCollection 组件。RobotCollection 管理着本线程中所有的 Robot 对象，而将协议处理提

到了 RobotCollection 层面之后，有几个线程就对比几次，调用 RobotCollection::

GetMsgObj 函数一次就可以找到 Robot 对象。

虽然增加了新组件，相关协议处理函数从 Robot 迁移到了 RobotCollection 类，但是

robots 工程逻辑并没有改变。

1 0 . 3 . 2 　 优 化 M e s s a g e C o m p o n e n t 组 件

我们把目光放在底层框架上也会发现问题，首当其冲的就是 MessageComponent 组件。MessageComponent 组件维护

了一个字典，关联着 MsgId 和它的处理函数的对应关系。审视一下这个组件，我们可以发现，如果有 1000 个实体都关心

网络断开事件，在 MessageSystem 中就有 1000 个 MessageComponent，在这 1000 个 MessageComponent 组件中存放着这 1000

个实体对应网络断开事件的回调函数。图 10-5 的左图展示了这个流程中的内存数据。

MessageComponent 组件阻隔了实体与 MessageSystem 之间的连接，它们彼此不知道对方的存在，但是

MessageComponent 带来了性能上的问题，而且它显然是多余的。鉴于该组件的操作频次，在 MessageSystem 系统中将

MessageComponent 取消，协议号 MsgId 直接注册到 MessageSystem，这样做的好处一目了然，如图 10-5 的右图所示。对

于一个确定的协议来说，不再需要遍历所有的 MessageComponent 组件就可以拿到所有需要回调的处理函数。本节的参


```cpp
class MessageSystem :virtual public ISystem<MessageSystem> {
public:
    void RegisterFunction(IEntity* obj, int msgId, MsgCallbackFun cbfun);
    template<typename T>
    void RegisterFunctionFilter(IEntity* obj, int msgId, std::function<T * (NetworkIdentify*)> pGetObj, s
    ...
private:
    // message
    std::map<int, std::list<IMessageCallBack*>> _callbacks;
};
```

考源代码目录为 10_02_redis。MessageSystem 的新定义如下：

**图 10-5 MessageSystem性能优化**

在注册协议函数时，将协议回调函数与 MsgId 绑定起来，这样免去了中间环节。以 Account 类为例，新的协议注册

方式如下：


```cpp
void Account::Awake() {
    ...
    auto pMsgSystem = GetSystemManager()->GetMessageSystem();
    pMsgSystem->RegisterFunction(this, Proto::MsgId::C2L_AccountCheck, BindFunP1(this, &Account::HandleAc
    ...
}
```

Account 类需要注册协议时，不再为自己增加一个 MessageComponent 组件，而是直接调用 MessageSystem::


```cpp
void MessageSystem::RegisterFunction(IEntity* obj, int msgId, MsgCallbackFun cbfun) {
    auto iter = _callbacks.find(msgId);
    if (iter == _callbacks.end()) {
        _callbacks.insert(std::make_pair(msgId, std::list<IMessageCallBack*>()));
    }
    const auto pCallback = obj->AddComponent<MessageCallBack> (std::move(cbfun));
    _callbacks[msgId].push_back(pCallback);
}
```

RegisterFunction 进行注册，该函数的实现如下：

取消 MessageComponent 组件之后，在 MessageSystem 中提供了两个函数：一个是 RegisterFunction;另一个是


```cpp
template <typename T>
void MessageSystem::RegisterFunctionFilter(IEntity* obj, int msgId, std:: function<T * (NetworkIdentify*)
    auto iter = _callbacks.find(msgId);
    if (iter == _callbacks.end()) {
        _callbacks.insert(std::make_pair(msgId, std::list<IMessageCallBack*>()));
    }
    auto pCallback = obj->AddComponent<MessageCallBackFilter<T>>();
    pCallback->GetFilterObj = std::move(getObj);
    pCallback->HandleFunction = std::move(fun);
    _callbacks[msgId].push_back(pCallback);
}
```

RegisterFunctionFilter。下面来看一下注册函数：

在 RegisterFunctionFilter 函数中用了过滤的流程，注册协议时需要提供一个过滤函数 GetFilterObj。在 robots


```cpp
void RobotCollection::Awake() {
    auto pMsgSystem = GetSystemManager()->GetMessageSystem();
    pMsgSystem->RegisterFunctionFilter<Robot>(this, Proto::MsgId:: MI_NetworkConnected, BindFunP1(this, &
    ...
}
```

工程的代码中，在为 Robot 类绑定处理函数时使用了 RegisterFunctionFilter 函数，其注册代码如下：

收到 MI_NetworkConnected 网络连接协议时，RobotCollection::GetMsgObj 函数跟据 Packet 实例上的网络身份筛


```cpp
Robot* RobotCollection::GetMsgObj(NetworkIdentify* pIdentify) {
    auto objKey = pIdentify->GetObjectKey();
    if (objKey.KeyType != ObjectKeyType::Account)
        return nullptr;
    auto iter = _robots.find(objKey.KeyValue.KeyStr);
    if (iter == _robots.end())
        return nullptr;
    return iter->second;
}
```

选出目标 Robot 类。

对于每一个协议，GetMsgObj 函数在线程上只需要调用一次就可以精准找到 Robot 实例，从而极大地提高了性能。

1 0 . 3 . 3 　 C o n n e c t O b j 内 存 组 织

如果将 10_01_game 目录下的工程在 Windows 和 Linux 下进行对比，就会发现在 Windows 系统下

远远慢于在 Linux 系统下，接近慢了 10 倍。分析一下代码，除了网络层之外，几乎所有的代码都

是共用的。怎么网络底层中采用的 Select 模型会慢这么多？

在 robots 工程中，随着功能的增加经历了 3 次网络连接，第一次是 HTTP 请求，第二次是连接

到 login，第三次是登录 game 进程。网络层的负担加重之后，它的性能问题就凸显出来了。下面


```cpp
void Network::Select() {
    for (auto iter = _connects.begin(); iter != _connects.end(); ++iter) {
        ...
    }
    ...
    const int nfds = ::select(_fdMax + 1, &readfds, &writefds, &exceptfds, &timeout);
    if (nfds <= 0)
        return;
    auto iter = _connects.begin();
    while (iter != _connects.end()) {
        ...
    }
}
```

来看之前的 select 函数中的代码：

在 Select 函数调用中有两次循环：第一次是将所有的连接对象加入底层函数::select 需要

的集合中;第二次是分析底层函数::select 的返回。这两个循环都不可缺少，这两个循环是

Linux 下 epoll 函数不需要的，这可能是 Select 模式变慢的原因。


```cpp
std::map<SOCKET, ConnectObj*> _connects
```

在旧的 Network 类中，将连接对象放在一个字典中，代码如下：


```bash
#define MAX_CLIENT  10000
ConnectObj* _connects[MAX_CLIENT]{};
std::set<SOCKET> _sockets;
```

现在来优化这个数据，使用如下数据结构代替：

sockets 集合中存放着当前所有有效的 Socket 值，_connects 组件以数据的方式提供

ConnectObj 对象的访问。也就是说，不再使用字典，而是使用数组的方式来访问连接对象。考虑

一种情况，我们登录 1000 个账号，按巅峰值来计算，最多时 ConnectObj 只有 1000 个，将字典换成

数组，能大幅度提高整体的性能吗？

框架是按帧运行的，也就是说某些函数是每一帧都要运行的。运行完一帧用的时间越少，其

性能就越高。下面用数据来说话，先来看图 10-6 中新数据的内存图。

**图 10-6 ConnectObj内存列表**

```text
ConnectObj pool   ┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐┌───┐
                  └───┘└───┘└─▲─┘└───┘└───┘└───┘└───┘└───┘
                              │
                          Index:1024
                              │
ConnectObj list   ┌─────┐┌───┐┌─▲─┐┌───┐┌───┐┌───┐┌───┐┌───┐
                  │ null││   ││   ││   ││   ││   ││   ││   │
                  └─────┘└───┘└───┘└───┘└───┘└───┘└───┘└───┘
                              ▲
                          Socket 1024
                              │
                     ┌────────────────────┐
                     │ CreateConnectObj   │
                     └────────────────────┘
```

connects 数组在初始化时都是空指针，创建 Socket 时将 Socket 值作为它的下标，因为在

Windows 下 Socket 分配值是随机的，所以使用了 10000 长度的数组。将 Socket 值作为下标之后，依

然从对象池中分配一个对象，同时在 connects 数组对应的位置设置指针地址，以达到内存地址一

次寻址就能找到对象，而不再需要 Find 操作，从而提升效率。在编码中，遇到类似的问题都可以

采用这种处理方式。

这个优化在 10_02_redis 目录中完成。现在可以比较一下 10_01_game 目录中工程的性能和

10_02_redis 目录中工程的性能，同时登录 1000 个账号，会发现不论是在 Windows 还是在 Linux 下

都有一定的提升。这两个工程的功能完全一样，只是在数据结构上做了一些优化工作。

---

## 图表转写 · 批次 10

> 这一段是提取时按 PDF 页序插进来的图表转录，**与本章正文交错**，
> 其中 `### pXXX_0.jpg` 是配图的文字转录，图页裁图在 `_raw/B_img/`。

> 来源：《网络多人在线游戏开发实战教程》（彭放）
> 图题编号依据文字层；标题以图片实际内容为准（文字层中文图题在 PDF 中已损坏，不可复原）

1 0 . 4

多 进 程 登 录 协 议 回 顾

截至目前的工程，我们介绍了如何从众多 login 中选择一个进程进行账号验证，同时阐

述了如何从众多 game 中选择一个合适的进程进行登录的方案。

将所有细节抛到一边，先从协议入手，梳理一下最终的登录方案需要多少个协议。协

议定义文件位于 10_02_redis 目录的 proto_id.proto 文件中。要完成角色登录，需要经历以

下几个阶段：


```protobuf
C2L_AccountCheck                  = 1000;            // 1. 验证账号
C2L_AccountCheckRs                = 1001;
MI_AccountQueryOnlineToRedis      = 1002;            // 2. 验证账号在线
MI_AccountQueryOnlineToRedisRs    = 1003;
```

第一个阶段：登录阶段

在这个阶段中，客户端只需要发送 C2L_AccountCheck 协议验证账号，得到

C2L_AccountCheckRs 协议的返回，而服务器要做的事情却不是那么简单。

• （1）服务器需要判断当前的账号是否已经在本进程或其他的 login 中登录了，这里涉

及一个新问题——跨线程的数据查询。为了方便描述，我们采用 Redis 这个内存数据库。

• （2）服务器向 Redis 查询该账号是否已经登录，如果已在其他地方登录，就通知客户

端;否则，进行 HTTP 的账号验证。


```protobuf
L2DB_QueryPlayerList     = 1010;            // 1. 选择角色
L2DB_QueryPlayerListRs   = 1011;
L2C_PlayerList           = 1012;
C2L_CreatePlayer         = 1014;            // 2. 选择角色：如果没有角色，就请求创建角色
C2L_CreatePlayerRs       = 1015;
L2DB_CreatePlayer        = 1016;            // 3. 选择角色：创建角色
L2DB_CreatePlayerRs      = 1017;
C2L_SelectPlayer         = 1018;            // 4. 选择角色
C2L_SelectPlayerRs       = 1019;
```

第二个阶段：角色创建与选择阶段

当验证成功之后，进入第二个阶段，这个阶段的主要目的是创建与选择角色，这个阶

段有与数据库的交互。

• （1）所有的登录验证都完成，login 向 dbmgr 进程发起对账号所有角色的查询，将数据

通过 L2C_PlayerList 协议传递到客户端。

• （2）如果没有角色，客户端就会发起 C2L_CreatePlayer 创建角色，协议

C2L_CreatePlayer 中带上创建的一些指定的属性，例如名字、性别等。

• （3）如果有角色，客户端就会发起 C2L_SelectPlayer 选择一个角色。


```protobuf
MI_LoginTokenToRedis    = 1022;          // 1. 请求登录的 token
MI_LoginTokenToRedisRs  = 1023;
L2C_GameToken           = 1024;          // 2. 将 token 发送给客户端
```

第三个阶段：为跳转进程做准备

选择好角色之后，就可以进入真正的游戏世界了，在此之前需要做一些准备工作。

• （1）当所有验证都成功时，login 进程需要向 Redis 请求一个 token 用于登录指定 game

进程，这个 token 将在几秒之中消失。

• （2）将 token 发送到客户端，至此，login 的工作全部完成。

以上 3 个阶段是在 login 进程中完成的，整个 login 进程主要负责账号与登录部分的功

能。不要以为这部分的代码很简单，真正到了上线时，由于游戏需要接入的平台很多，这

部分的代码也是非常复杂的。


```protobuf
C2G_LoginByToken       = 1100;          // 1. 登录：客户端请求登录 game
C2G_LoginByTokenRs     = 1101;
MI_GameTokenToRedis    = 1102;          // 2. 登录： game 请求登录的 token
MI_GameTokenToRedisRs  = 1103;
```

第四个阶段：登录 game

• （1）客户端收到可以登录的 game 进程的信息之后，将 token 发送到 game 进程上要求登

录，会收到 C2G_LoginByTokenRs 消息，如果失败，就可能是 token 失效，客户端需要从

login 进程再次登录。

• （2）game 进程收到客户端要求登录的协议 C2G_LoginByToken 之后，查询这个 token 是

否是有效的，如果有效，就成功进入地图。

至此，整个登录流程就完成了。

1 0 . 5

总 结

本章引入了 Redis，完整实现了登录的整个过程，同时完善了框架中的进

程，加入了 game 进程和 space 进程，还引入了 libplayer 库，加入了 Player 基本

组件，使用 Redis 保存 token，完成了 token 的验证。

到本章为止，基础组件已经搭建得较为完善，第 11 章将进入真正的游戏世

界。
