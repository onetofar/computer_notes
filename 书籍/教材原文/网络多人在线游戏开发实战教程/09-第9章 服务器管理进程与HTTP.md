---
title: "网络多人在线游戏开发实战教程 · 第9章 服务器管理进程与HTTP"
category: 教材原文
type: 原文章节稿
source_book: "网络多人在线游戏开发实战教程"
source_section: "第9章 服务器管理进程与HTTP"
tags:
  - 教材原文
  - 彭放教程
  - C++服务器
source: "《网络多人在线游戏开发实战教程》（彭放，机械工业出版社 2019，ISBN 9787111667926）第9章 服务器管理进程与HTTP · md 稿提取（文字层 + 图表 OCR 回填），保留原文未净化"
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 第9章 服务器管理进程与HTTP

> **原文章节稿 · 未经净化。** [[网络多人在线游戏开发实战教程|← 返回本书目录]]
> 提取自 PDF（文字层 + 图表视觉 OCR 回填，138 张图表）。**保留原书全部技术细节与代码，未做删减或改写**。各章末尾的「图表转写」是该章配图的文字转录；原始裁图在 `_raw/B_img/`。

## 本章小节

- [[#9.1 启动多个login进程|9.1 启动多个login进程]]
- [[#9.2 appmgr进程|9.2 appmgr进程]]
- [[#9.3 HTTP|9.3 HTTP]]
- [[#9.4 使用Mongoose分析HTTP格式|9.4 使用Mongoose分析HTTP格式]]
- [[#9.5 为Packet定义新的网络标识|9.5 为Packet定义新的网络标识]]
- [[#9.6 HTTP分块|9.6 HTTP分块]]
- [[#9.7 机器人测试批量登录|9.7 机器人测试批量登录]]
- [[#9.8 总结|9.8 总结]]

> **尚未净化** —— 本章还没有对应的详解笔记。

---

在最初的构想中，我们的框架有多个 login 进程，它是服务器的第一道屏

障，玩家只有登录成功才可能进入游戏。当有多个 login 的时候就涉及一个问

题，客户端到底该从哪个 login 登录呢？这是本章要解决的问题。本章包括以

下内容：

·提出一个多进程登录方案。

·HTTP 分析。

·新的网络标识。


## 9.1 启动多个login进程

到目前为止，框架还只有一个 login 进程，单 login 极易形成瓶颈，所以本

节需要为整个架构设置多个 login 进程。要架设多个 login，首先需要进行多个

login 的配置，打开 engine.yaml 文件，重新配置如下：

login:

thread_num: 2

url_login: HTTP://192.168.1.120/member_login_t.php

url_method: "get"

apps:

- id: 101

ip: 127.0.0.1

port: 5401

- id: 102

ip: 127.0.0.1

port: 5402

在 login 属性之下配置了一个 apps，这是一个列表类，YAML 语法用到

了“-”。现在配置了两个 login 信息，其中属性 id（libserver 库中 ServerApp

类中的 AppId 属性）是为了区分到底启动的是哪一个 login。现在，启动 login

时需要带上参数，例如“./login.exe-sid=101”是启动 AppId 为 101 的 login。

本节的源代码位于目录 09_01_appmgr 中，在 ServerApp 初始化时，

ServerApp::Initialize 函数对命令行参数进行了分析。分析出来的 AppId 保

存到全局的 Global 类中，以方便读取。现在有了多个 login，那么问题来了，

对于客户端来说，登录游戏时应该使用哪一个 login 呢？

解决这个问题有两种方案：一种方案是从运维的角度来解决，利用 Nginx

的功能，这种方式在后面讨论;另一种方案是通过游戏的逻辑数据来解决，做

一个简单的登录策略，每个玩家登录时，可以通过某种方式得知玩家在线最少

的 login 的 IP 和端口。

现在登录问题进一步演变为该如何得到最小负载的 login。我们向哪个服

务器发送这个请求可以得到答案呢？为了解决这个问题，需要新建一个工程，

用来维护所有的 login，将其称为 appmgr。除了当前的 login 进程之外，appmgr

将来还会维护 game 进程。


## 9.2 appmgr进程

在之前的例子中，robots 进程登录服务器时是通过一个具体的 IP 和端口来进行网络连接的，但

是在新的流程中，因为有无数个 login 进程，所以并没有一个具体的 IP 和端口，而且随着游戏的进

展，这些 IP 和端口还可能动态地增减。在此基础上，需要有一个 appmgr 进程，如图 9-1 所示，这个

进程的作用是动态收集 login 的信息，再以某种方式通知客户端。

**图 9-1 appmgr登录数据**

login 进程启动之后会定期向 appmgr 发送自己当前的状态，这样 appmgr 就被动地收集到了所有

login 的数据。而客户端登录时，只需要向 appmgr 询问即可得到最小负载的 login 信息。

本节源代码位于 09_01_appmgr 目录中，打开 appmgr 工程的 login_sync_component.h 文件，其中

定义了收集组件，名为 LoginSyncComponent。该组件的主要作用是收集所有 login 进程的同步信


```cpp
class LoginSyncComponent :public SyncComponent, public IAwakeSystem<> {
public:
    void Awake() override;
    void BackToPool() override;
};
class SyncComponent :public Entity<SyncComponent> {
public:
    void AppInfoSyncHandle(Packet* pPacket);
    bool GetOneApp(APP_TYPE appType, AppInfo& info);
    ...
protected:
    std::map<int, AppInfo> _apps;
};
```

息，其类定义和基类定义如下：

基类组件 SyncComponent 将收集到的数据保存在字典 std::map<int，AppInfo>中，Key 值为

AppId，Value 值为结构 AppInfo，AppInfo 的定义如下：


```cpp
struct AppInfo {
    APP_TYPE AppType;
    int AppId;
    std::string Ip;
    int Port;
    int Online;
    SOCKET Socket;
};
```

目前功能比较简单，这个结构也非常简单，AppInfo 结构中只有一个 Online 属性，用于记录在

这个 AppId 对应的进程中有多少玩家在线。

LoginSyncComponent::Awake 初始化函数中注册了 login 同步消息 MI_AppInfoSync 的处理函

数。对于 LoginSyncComponent 组件来说，它并不关心谁发送了数据，只关心 MI_AppInfoSync 协议本

身的内容，谁发送来的并不重要。当 LoginSyncComponent 收到 MI_AppInfoSync 协议时，交给它的基


```cpp
void SyncComponent::AppInfoSyncHandle(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::AppInfoSync>();
    const auto iter = _apps.find(proto.app_id());
    if (iter == _apps.end()) {
        AppInfo syncAppInfo;
        syncAppInfo.Parse(proto);
        syncAppInfo.Socket = pPacket->GetSocket();
        _apps[syncAppInfo.AppId] = syncAppInfo;
    } else {
        const int appId = proto.app_id();
        _apps[appId].Online = proto.online();
        _apps[appId].Socket = pPacket->GetSocket();
    }
}
```

类处理。处理该协议的函数实现如下：

该协议中会收集 AppType、AppId 以及当前的在线人数，LoginSyncComponent 将这些数据缓存起

来，以备需要时使用。

MI_AppInfoSync 协议是由 login 进程发出的，在 login 工程的 Account 类中。Account 类增加了一


```cpp
void Account::Awake() {
    AddTimer(0, 10, true, 2, BindFunP0(this, &Account::SyncAppInfoToAppMgr));
    ...
}
void Account::SyncAppInfoToAppMgr() {
    Proto::AppInfoSync protoSync;
    protoSync.set_app_id(Global::GetInstance()->GetCurAppId());
    protoSync.set_app_type((int)Global::GetInstance()->GetCurAppType());
    protoSync.set_online(_playerMgr.Count());
    MessageSystemHelp::SendPacket(Proto::MsgId::MI_AppInfoSync, protoSync, APP_APPMGR);
}
```

个定时器，每隔几秒就将自己的信息发送出去。

在这个定时器中，每 10 秒调用一次 SyncAppInfoToAppMgr 函数。每隔 10 秒，Account 类根据当前

玩家的数据向 appmgr 进程发送在线情况。协议数据最终被送到了 LoginSyncComponent 组件中。

打开 09_01_appmgr 目录，编译后运行 allinone 工程，来看看收到了些什么数据。在工程中新写

了一个 app 指令，利用 app-info 查看当前进程中 app 的情况。

app -info

[DEBUG] -  appId:   0 type:allinone online:0

当我们使用 robots 进程登录一个玩家之后，在 allinone 终端输入“app-info”，情况发生了一

些变化。

[DEBUG] -  appId:   0 type:allinone online:1

可以拆分进程再测试一次。启动 dbmgr、appmgr 和两个 login 进程，用 robots 登录一个玩家到其

中一个进程上。注意启动 login 时需要参数“./login-sid=101”。在 appmgr 终端输入“app-

info”可以看到如下信息：

[DEBUG] - appId: 101 type:login online:1

[DEBUG] - appId: 102 type:login online:0

启动了两个 login，登录一个账号之后，AppId 为 101 的 login 进程上就会有一个玩家在线。玩家

进入 id 为 101 的 login 之后，数据会被通知到 appmgr 进程上。

现在有了数据，但还是没有解决多服务器登录的问题，因为 robots 总是连接到 AppId 为 101 的进

程上。

前面完成了数据的收集工作，接下来要解决的问题是：客户端通过什么方式从 appmgr 中知道

login 的这些状态呢？鉴于客户端与 appmgr 进程只有一次性的通信，这里我们采用 HTTP 弱连接方

式。具体方法是，appmgr 进程提供一个 HTTP 端口——例如 appmgr 的 IP 为 192.168.0.100，开放一个

HTTP 服务——端口为 8081，提供一个 192.168.0.100：8081/login 请求，该请求返回一个 JSON 数

据，类似{"ip"："192.168.0.100"，"port"：5002}，让调用者知道当前在 192.168.0.100 的 5002 端

口上有一个可以进行连接的 login 进程。

总之，在登录时，robots 进程开始并不知道它能够连接哪一个 login，它会先向 appmgr 发起一

个 HTTP 请求，appmgr 根据当前所有 login 的动态信息给它一个合适的用于登录的 IP 和端口，robots

进程收到之后再进行网络连接。

鉴于此，现在需要在框架中加入 HTTP 的处理，提供 HTTP 服务。

这样做有两个好处：第一，HTTP 是短连接，用完就释放，比 TCP 快，同时 HTTP 本身有自己的协

议体，不需要再创建协议结构;第二，除了用在登录上外，提供 HTTP 接口还可以用于查看现有服务

器的状态。之前我们提供了一些控制台指令来查看服务器的当前状态，但在真正运营的时候不能每

一次都用指令来查看当前数据，所以必须有一种方式可以将这些数据提供给外部的第三方显示，这

里第三方工具可能是一个后台的 Web 工具，使用 HTTP 就可以方便地展示在网页上，而不需要登录后

台输入指令。

要提供 HTTP 服务，就不得不从 HTTP 开始分析。这个协议是我们日常使用的，其实非常简单。


## 9.3 HTTP

首先需要了解 HTTP 是怎么回事。我们每天的生活都离不开 HTTP，如网上浏览、网上购物都在使用

HTTP。不要觉得这个协议很神秘，以我们现在的知识储备量完全可以解决它。在第 1 章介绍过网络协

议，那么 HTTP 和我们自定义的协议有什么区别呢？游戏逻辑中的协议是由我们自己定义格式的，而

HTTP 是一种全球都需要遵循的规范。

本书中自定义协议的格式是由一个长度字段加上 PacketHead 结构和一个二进制流三部分组成的。

HTTP 的格式大同小异。它没有长度字段，由协议头加上协议体所组成。HTTP 的头部长度比我们自定义

的头部结构更长。在游戏逻辑中，协议都是采用二进制的方式传输的，但 HTTP 的内容采用的是文本格

式。下面用实例来说明一个常规的 HTTP 请求协议的头部结构。步骤如下：

• （1）打开 09_02_http 目录，修改 log4_help.h 文件，打开 LOG_HTTP_OPEN 宏。

• （2）使用根目录下的 make-all.sh 脚本编译工程。

• （3）进入 bin 目录，执行“./allinoned”。


```bash
[root@localhost ~]# curl http://127.0.0.1:7071/login
{
    "ip" : "127.0.0.1",
    "port" : 5401,
    "returncode" : 0
}
```

• （4）新开一个终端，使用 curl 命令查看 http://127.0.0.1:7071/login 请求。其结果如下：

在本例中，我们打开了一个新的端口 7071，用来接收 HTTP 相关的请求。在另一个终端使用 curl 命

令向服务端发起一个请求。服务端收到请求后，打印出来的头部数据如下：

GET /login http/1.1

User-Agent: curl/7.29.0

Host: 127.0.0.1:7071

Accept: */*

（空行 \r\n ）

众所周知，HTTP 是一个文本格式的协议，上面的打印结果就是证明。不需要任何转换，只需要把

收到的数据打印出来，我们就可以看到 HTTP 的数据结构。它就是一个文本。如果用浏览器访问 7071 端

口上的 login 请求，头部数据的打印输出就不一样了：

GET /login http/1.1

Host: 127.0.0.1:7071

User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:69.0) Gecko/20100101 Firefox/69.0

Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8

Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2

Accept-Encoding: gzip, deflate

DNT: 1

Connection: keep-alive

Upgrade-Insecure-Requests: 1

（空行 \r\n ）

多了很多头部属性数据，但是总的来说，其格式并没有改变。HTTP 的格式如下：

第一行： 1. 请求方式 (GET) 2. 空格  3. 方法名 (/login) 4. 空格  5.HTTP 版本  6. 回车符 \r\n

第二行： 1. 头部属性名  2. 冒号  3. 头部属性值  4. 回车符 \r\n

...

第 N 行： 1. 头部属性名  2. 冒号  3. 头部属性值  4. 回车符 \r\n

头结束：回车符 \r\n

从第二行开始，可以叠加多个属性，头部的结束行必须是回车符。也就是说，在分析协议时，发

现有连续两个\r\n，头部数据就结束了。

常用的 HTTP 请求方式有两种：一种是 GET，另一种是 POST。当我们在浏览器中输入网页地址或单

击一个链接的时候，一般使用的是 GET 方法，旨在从服务器获取一些数据。而当我们登录或者填写表

单的时候，一般使用的是 POST 方法，旨在将这些数据推送到服务器上。

读者一定很好奇，请求被发送到了 HTTP 端口上，那么浏览器是如何显示数据的呢？

在框架中，当我们向服务端发送了验证账号协议之后，服务器会返回给客户端一个验证账号结果

协议。HTTP 请求是一样的道理，服务端收到 HTTP 请求之后，向连接的 Socket（套接字）写入了数据，

这些写入的数据被显示到了浏览器上。

这里就涉及 HTTP 返回数据协议格式，在服务端可以看到输出信息：

http/1.1 200 OK

Connection: close

Content-Type: application/json; charset=utf-8

Content-Length:58


```cpp
{
    "ip" : "127.0.0.1",
    "port" : 5401,
    "returncode" : 0
}
```

（空行 \r\n ）

HTTP 返回协议的头部格式如下：

第一行： 1.HTTP 版本  2. 空格  3. 状态码  4. 状态码描述  5. 回车符 \r\n

第二行： 1. 头部属性名  2. 冒号  3. 头部属性值  4. 回车符 \r\n

...

第 N 行： 1. 头部属性名  2. 冒号  3. 头部属性值  4. 回车符 \r\n

头结束：回车符 \r\n

返回协议的第一行与请求协议略有不同，接下来几行和请求协议一样，都是属性名：属性值的结

构，直到两个\r\n 结束。如果将头部数据打印出来，那么这两个\r\n 表现为一个空行。随后进入协议

体。协议体就是真正在浏览器中显示的数据。


```cpp
{ "ip" : "192.168.0.120",  "port" : 5401, "returncode" : 0 }
```

在浏览器中输入“http://192.168.0.120:7071/login”，该请求在浏览器上显示为：

从这个显示可见，真正的数据部分也就是除开头之外的部分。在返回协议中，属性 Connection：

close 表示主动要求断开连接。Content-Length 的属性值得我们注意，它的值就是整个协议体的长

度。

在某些情况下，Content-Length 属性不存在，这时的返回数据可能是按块传递的。当返回数据非

常大，不便于计算长度时，就可以按块来传递数据。假设有 100KB 的网页数据，这 100KB 的数据可能是

几次传递到浏览器上的，但头部数据只传递一次，后面收到的数据格式为：长度+协议体。直到收到

两个\r\n 结束协议体，表示整个数据接收完成。

简而言之，按块传递时，发送数据是分成几次发送的，但是头部数据只有一次。后面我们会详细

讲解这一部分的内容。

在返回协议中，另一个属性 Content-Type 表示这个协议体中的格式是什么，它的值非常多，如果

填上 text/plain 就表示纯文本，text/html 是常规网页的显示数据，表示协议体中是一个 HTML 格式的

文本，本例中返回的是 application/json，表示内容是 JSON 格式的数据。简单了解了 HTTP 的格式后，

下面在框架中实现一个 HTTP 服务。


## 9.4 使用Mongoose分析HTTP格式

由于已经有非常多 HTTP 的第三方开源工程，本书不会再重新编写一个库。框架中采用的第三方库为

Mongoose，其源代码地址为 https://github.com/cesanta/mongoose。该库非常简单，但如果全部采用

Mongoose 库的逻辑，与框架逻辑结合起来就会比较混乱，所以没有完整地使用这个库，只使用了两个文

件：mongoose.h 和 mongoose.c。这两个文件的作用是分析 HTTP 请求协议的格式。

将这两个文件直接引入 libserver 中，框架并不使用 Mongoose 服务器，只是根据现有的框架，使用

它的函数来减少 HTTP 的分析工作，毕竟 Mongoose 库就是用于 HTTP 服务的，在分析方面肯定做得比我们更

好，考虑得更全面。

本节和后面关于 HTTP 的章节，示例都在 09_02_http 目录中。既然要开放 HTTP 服务，必然需要一个监

听类，依然还是使用 NetworkListen 类。为了区分游戏逻辑层和 HTTP 监听的不同，在 network_type.h 文


```cpp
enum class NetworkType {
    None = 0,
    TcpListen = 1 << 0,
    TcpConnector = 1 << 1,
    HttpListen = 1 << 2,
    HttpConnector = 1 << 3,
};
```

件中新定义了一个枚举：


```cpp
class NetworkListen : public IAwakeSystem<std::string, int>, public IAwakeSystem<int, int> {
public:
    void Awake(std::string ip, int port) override;
    void Awake(int appType, int appId) override;
    ...
}
```

下面是 NetworkListen 类的定义：

现在，NetworkListen 类有两种创建方式：当以<int，int>参数创建时，表示输入两个 int 类型的数

据，分析的是 AppType 与 AppId，创建出来的 NetworkListen 实例是 TCP 监听，即游戏逻辑需要使用的监

听。

当以<string，int>参数创建时，表示输入的是 IP 与端口，创建出来的 NetworkListen 实例用于 HTTP

的监听。下面来详细分析 NetworkListen 如何实现对 HTTP 的处理。


### 9.4.1 HTTP类型


```cpp
SOCKET Network::CreateSocket() const {
    _sock_init();
    SOCKET socket;
    if (_networkType == NetworkType::HttpListen || _networkType == NetworkType::HttpConnector)
        socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    else
        socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ...
    return socket;
}
```

不论是游戏的端口监听还是 HTTP 的端口监听，要接收数据都需要创建一个 Socket，其调用代码如下：

在创建 Socket 的时候，TCP 监听使用了::socket（AF_INET，SOCK_STREAM，IPPROTO_TCP），注意最

后一个参数 IPPROTO_TCP。也就是说，要用 TCP 的方式来创建一个 Socket。HTTP 监听时使用的是

IPPROTO_IP，是一种很简单的传输方式，也是一种无状态的传输方式。

使用 IPPROTO_TCP 时，TCP 底层有一套机制保证传递到网络上的数据是有序的且不遗漏，但 IPPROTO_IP

不是这样，IPPROTO_IP 没有 TCP 的 3 次握手，但这种方式更快。

假设在这里 HTTP 监听或 HTTP 连接也采用 IPPROTO_TCP，可不可以？当然可以。HTTP 协议体与它的传输

方式没有任何关系，完全可以使用 IPPROTO_TCP 来传输它，只是作为一个一次性连接没有必要而已。HTTP

在网络传输中丢掉就丢掉，丢掉超时可以再发，所以使用更为简单的 IPPROTO_IP。

如果读者曾经看过一些 Web 开发相关的图书，会提到 Session、Cookie 之类的内容，这并不是 HTTP 底层

特有的，而是我们使用的框架逻辑附加给它的。本书中的示例并没有 Session 和 Cookie 的概念，我们实现

的是比较简单的 HTTP 服务，而 Session 和 Cookie 是业界约定俗成的，导致大部分源代码都以 Session 作为

Web 连接的处理类。


### 9.4.2 接收HTTP数据

创建好监听之后，当收到 HTTP 请求时，首先会在网络层产生一个 Socket 描述符，这和 TCP 监听没有什么两样。当底


```cpp
Packet* RecvNetworkBuffer::GetPacket() {
    auto pNetwork = _pConnectObj->GetParent<Network>();
    auto iType = pNetwork->GetNetworkType();
    if (iType == NetworkType::HttpConnector || iType == NetworkType::HttpListen)
        return GetHttpPacket();
    return GetTcpPacket();
}
```

层有数据需要读取，封装为 Packet 时，分析数据写了一个 GetHttpPacket 的分支：

GetTcpPacket 函数用于 TCP 监听模式下，取出来的数据是自定义的协议。函数 GetHttpPacket 则是对于 HTTP 的分


```cpp
Packet* RecvNetworkBuffer::GetHttpPacket() {
    if (_endIndex < _beginIndex) {
        // 有异常，关闭网络
        _pConnectObj->Close();
        LOG_ERROR("http recv invalid.");
        return nullptr;
    }
    const unsigned int recvBufLength = _endIndex - _beginIndex;
    const auto pNetwork = _pConnectObj->GetParent<Network>();
    const auto iType = pNetwork->GetNetworkType();
    const bool isConnector = iType == NetworkType::HttpConnector;
    // 分析出头部的数据
    http_message hm;
    const unsigned int headerLen = mg_parse_http(_buffer + _beginIndex, _endIndex - _beginIndex, &hm, !is
    if (headerLen <= 0)
        return nullptr;
    unsigned int bodyLen = 0;
    const auto mgBody = mg_get_http_header(&hm, "Content-Length");
    if (mgBody != nullptr) {
        bodyLen = atoi(mgBody->p);
        // 整个包的长度不够，再等一等
        if (bodyLen > 0 && (recvBufLength < (bodyLen + headerLen)))
            return nullptr;
    }
    bool isChunked = false;
    const auto mgTransferEncoding = mg_get_http_header(&hm, "Transfer-Encoding");
    if (mgTransferEncoding != nullptr && mg_vcasecmp(mgTransferEncoding, "chunked") == 0) {
        isChunked = true;
        // 后面的数据还没有到达
        if (recvBufLength == headerLen)
            return nullptr;
        bodyLen = mg_http_get_request_len(_buffer + _beginIndex + headerLen, recvBufLength - headerLen);
        if (bodyLen <= 0)
            return nullptr;
        bodyLen = _endIndex - _beginIndex - headerLen;
    }
    // 数据接收完毕之后创建一个 Packet 类
    Packet* pPacket = MessageSystemHelp::ParseHttp(_pConnectObj,_buffer + _beginIndex + headerLen, bodyLe
    // 从网络缓存中删除已读取的数据
    RemoveDate(bodyLen + headerLen);
    return pPacket;
}
```

析，将网络层数据转换成内存中使用的 Packet 类。下面来看看具体实现：

上面的代码看上去比较枯燥，涉及缓存中字节的读取，使用的是第三方库函数，分别说明如下：

• （1）mg_parse_http 函数，从一个 buffer 中提出 HTTP 的头部信息，结构为 http_message。

• （2）mg_get_http_header 函数，从头部信息中取得某个属性的值。

• （3）mg_http_get_request_len 函数，从传入的字符串中找到两个\r\n，返回长度。

取得 HTTP 头部信息之后，如果有长度属性 Content-Length，就按长度读出数据;如果没有长度属性，就需要检查

数据是不是按块发送的。按块发送时，头部属性中可以读出 Transfer-Encoding 属性。如果该属性值为 chunked，就表

示协议体是按块发送的，需要收集所有块数据，再将这些数据组织到 Packet 中，并发送给其他组件。当数据接收完成


```cpp
Packet* MessageSystemHelp::ParseHttp(NetworkIdentify* pIdentify, const char* s, unsigned int bodyLen, con
    Proto::HTTP proto;
    if (bodyLen > 0) {
        if (isChunked) {
            const auto end = s + bodyLen;
            mg_str tmp;
            while (true) {
                const int len = std::atoi(s);
                if (len == 0)
                    break;
                s = mg_skip(s, end, "\r\n", &tmp);
                s = mg_skip(s, end, "\r\n", &tmp);
                proto.mutable_body()->append(tmp.p, tmp.len);
            }
        } else {
            proto.mutable_body()->append(s, bodyLen);
        }
    }
    Proto::MsgId msgId = Proto::MsgId::MI_HttpRequestBad;
    if (hm->method.len > 0) {
        // 请求
        do {
            if (mg_vcasecmp(&hm->method, "GET") == 0) {
                if (mg_vcasecmp(&hm->uri, "/login") == 0) {
                    msgId = Proto::MsgId::MI_HttpRequestLogin;
                    break;
                }
            }
        } while (false);
    } else {
        proto.set_status_code(hm->resp_code);
        msgId = Proto::MsgId::MI_HttpOuterResponse;
    }
    auto pPacket = CreatePacket(msgId, pIdentify);
    pPacket->SerializeToBuffer(proto);
    return pPacket;
}
```

之后，调用 MessageSystemHelp::ParseHttp 函数生成供框架使用的 Packet 类。下面是它的实现：

在分析请求时，当前仅处理了 login 请求，其他所有请求都是非法的。在框架中，为了使 HTTP 与自定义协议一致，


```protobuf
message Http {
    string body = 1;
    int32 status_code = 2;
}
```

定义了一个 Proto::Http 结构来暂存数据。定义如下：

综上所述，收到数据时，使用 Mongoose 库提供的函数分析 http_message 结构。该结构中的数据有两种情况，可能

是一个请求数据头，也可能是一个响应返回数据头。

当收到的数据是一个外部请求时，其 http_message::method::len 值一定大于 0。即使在浏览器中输入的是

127.0.0.1：7071，没有带任何后续方法，这个值也是大于 0 的，因为 method 是以“/”开头的，空方法 method 的值

为“/”，method::len 等于 1。

当收到的数据是响应返回数据时，说明是框架主动向外部的 HTTP 服务器发起的一个请求返回的数据，例如验证账

号时，我们需要向 Nginx 发送一个请求，收到返回时，http_message::resp_code 中就有当前返回的状态值，这个状

态值是有标准的，404 是大家比较熟悉的一个值，表示找不到这个请求的处理方式，无法处理。如果一切数据正常，状

态值就是 200。这些值是 HTTP 的常规值，是约定俗成、不可改变的。网络层只负责将这些数据分析出来，至于数据是请

求还是响应返回，返回的是成功还是失败，作为网络层来说并不关心，只需要将它发送到框架中，再由框架中的其他

组件去处理。

回到 RecvNetworkBuffer::GetPacket 函数中，无论它分析出来的 Packet 数据是用于 HTTP 还是逻辑层面的协议，

最终这个 Packet 会进入 ThreadMgr::DispatchPacket 函数对协议进行分发。


### 9.4.3 处理HTTP数据的协议号

既然将 HTTP 数据打包成了 Packet 数据，那么这些 HTTP 的 Packet 必然有协议


```protobuf
// HTTP listen 的请求（外部请求）
MI_HttpBegin           = 10000;
MI_HttpInnerResponse = 10001;                // 响应数据
MI_HttpRequestBad     = 10002;
MI_HttpRequestLogin  = 10003;
MI_HttpEnd             = 10499;
// HTTP connector 的消息（内部请求，外部返回）
MI_HttpOuterRequest      = 10500;           // 内部向外请求
MI_HttpOuterResponse     = 10501;           // 外部响应数据
```

号。打开 proto_id.proto 的定义文件：

工程中将协议号分成了两种：

·情况 1：从框架外部向框架发起请求，例如收到 127.0.0.1：7071/login

消息时，它对应的协议号为 MI_HttpRequestLogin，除此之外，没有处理的都

是 MI_HttpRequestBad 消息，返回 404 编码。当有请求被框架捕捉到时，处理完

成之后会向网络层发送一个返回数据，这时返回包的 Packet 协议号为

MI_HttpInnerResponse，所有的请求返回的消息协议号都是

MI_HttpInnerResponse。

·情况 2：框架向外部 HTTP 服务发起请求，例如 Nginx 上有一个验证用户的

请求，这时使用 MI_HttpOuterRequest 和 MI_HttpOuterResponse 分别作为请求

与返回协议编号。只有一个协议号，一旦我们有多个请求同时发送，它们会冲

突吗？答案是不会。即使只有一个协议号，但是 Socket 却不同，所以处理的

ConnectObj 也是不同的。

下面来澄清“请求”这个概念，如果我们的服务器端口为 8081，外部访问

127.0.0.1：8081/login 就是一个请求。在源代码中只实现了 login 请求，其他

的请求都被认为是一个 Bad 请求。如果访问一个网站时输入一个随机的地址，

例如 www.baidu.com/fdfsf，百度会提示我们访问的页面不存在，这就是一个

Bad 请求，因为百度服务器上根本没有处理随机输入的 fdfsf 请求。

总之，通过 NetworkType 的类型可以区分出逻辑协议与 HTTP，且为此生成

了不同的 Packet 类，这些 Packet 类会广播到所有线程中，关心协议的组件会去

处理。

下面我们通过实例进一步理解这个流程。


### 9.4.4 收到HTTP请求是如何响应的

当收到外部发起的 login 请求时，这个请求被封装成 Packet，其 MsgId 为 MI_HttpRequestLogin。处理组件类是


```cpp
void LoginSyncComponent::Awake() {
    ...
    // http
    pMsgCallBack->RegisterFuntion(Proto::MsgId::MI_HttpRequestLogin, BindFunP1(this, &LoginSyncComponent:
}
```

LoginSyncComponent，存放在 appmgr 工程的 login_sync_component.h 文件中。

为什么选择 LoginSyncComponent 来处理 login 请求呢？很显然，因为 login 请求涉及的数据在 LoginSyncComponent

类中。该类中包含当前全部有效的 login 进程数据。在上面的代码中，当类从对象池中被唤醒时，在 Awake 函数中设置


```cpp
void LoginSyncComponent::HandleHttpRequestLogin(Packet* pPacket) {
    Json::Value responseObj;
    AppInfo info;
    if (!GetOneApp(APP_LOGIN, info)) {
        responseObj["returncode"] = 4;
        responseObj["ip"] = "";
        responseObj["port"] = 0;
    } else {
        responseObj["returncode"] = 0;
        responseObj["ip"] = info.Ip;
        responseObj["port"] = info.Port;
    }
    std::stringstream jsonStream;
    _jsonWriter->write(responseObj, &jsonStream);
    MessageSystemHelp::SendHttpResponse(pPacket, jsonStream.str().c_str(), jsonStream.str().length());
}
bool SyncComponent::GetOneApp(APP_TYPE appType, AppInfo& info) {
    if (_apps.size() == 0) {
        LOG_ERROR("GetApp failed. no more. appType:" << GetAppName(appType));
        return false;
    }
    // 找到第一个同类型数据
    auto iter = std::find_if(_apps.begin(), _apps.end(), [&appType](auto pair){
            return (pair.second.AppType & appType) != 0;
        });
    if (iter == _apps.end()) {
        LOG_ERROR("GetApp failed. no more. appType:" << appType);
        return false;
    }
    // 遍历后面的数据，找到最小值
    auto min = iter->second.Online;
    int appId = iter->first;
    for (;iter != _apps.end();++iter) {
        if (min == 0)
            break;
        if ((iter->second.AppType & appType) == 0)
            continue;
        if (iter->second.Online < min) {
            min = iter->second.Online;
            appId = iter->first;
        }
    }
    // 数据加 1 ，以避免瞬间落在同一个 App 上，下次同步数据会将其覆盖为真实值
    _apps[appId].Online += 1;
    info = _apps[appId];
    return true;
}
```

了 MI_HttpRequestLogin 的处理函数为 HandleHttpRequestLogin，实现代码如下：

从基类的 GetOneApp 函数中取到一个真实的 login 进程的数据，将数据压入 Json::Value 结构中，封装为 JSON 并转


```cpp
void MessageSystemHelp::SendHttpResponse(NetworkIdentify* pIdentify, const char* content, int size) {
    SendHttpResponseBase(pIdentify, 200, content, size);
}
void MessageSystemHelp::SendHttpResponseBase(NetworkIdentify* pIdentify, int status_code, const char* con
    auto pNetworkLocator = ThreadMgr::GetInstance()->GetEntitySystem()->GetComponent<NetworkLocator>();
    auto pNetwork = pNetworkLocator->GetListen(NetworkType::HttpListen);
```

换为串，最后调用 MessageSystemHelp::SendHttpResponse 函数向网络层发送数据。其实现代码如下：


```cpp
    if (pNetwork == nullptr) {
        LOG_ERROR("can't find network. http send failed.");
        return;
    }
```


```cpp
    Packet* pPacket = CreatePacket(Proto::MsgId::MI_HttpInnerResponse, pIdentify);
    std::stringstream buffer;
    buffer << "http/1.1 " << status_code << " " << mg_status_message(status_code) << "\r\n";
    buffer << "Connection: close\r\n";
    buffer << "Content-Type: application/json; charset=utf-8\r\n";
    buffer << "Content-Length:" << size << "\r\n\r\n";
    if (size > 0) {
        buffer.write(content, size);
    }
    pPacket->SerializeToBuffer(buffer.str().c_str(), buffer.tellp());
    pNetwork->SendPacket(pPacket);
}
```

发送数据的时候，先找到 NetworkType::HttpListen 的网络实例，再组织一个返回格式，然后将数据写入。在写

入数据时，可以看到完全是一个文本的写入规则。本例的返回数据比较简单，采用 Content-Length 指定长度的方式。


### 9.4.5 发送HTTP返回数据流程

在发送返回数据时，依然是将返回数据写入一个 Packet 中，其 MsgId 为 MI_HttpInnerResponse，

但这个 Packet 中的 Buffer 数据与之前的所有 Buffer 数据不同，以往向其中写入的是一个 protobuf 的

结构，但在 HTTP 返回中写的是一个字符串。Packet 最终会发送到网络底层的缓冲区中。下面是底层


```cpp
void SendNetworkBuffer::AddPacket(Packet* pPacket) {
    const auto dataLength = pPacket->GetDataLength();
    TotalSizeType totalSize = dataLength + sizeof(PacketHead) + sizeof(TotalSizeType);
    // 长度不够，扩容
    while (GetEmptySize() < totalSize) {
        ReAllocBuffer();
        //std::cout << "send buffer::Realloc. _bufferSize:" << _bufferSize << std::endl;
    }
    // 对于 HTTP 来说没有自定义头
    const auto msgId = pPacket->GetMsgId();
    if (!NetworkHelp::IsHttpMsg(msgId)) {
        // 1. 整体长度
        MemcpyToBuffer(reinterpret_cast<char*>(&totalSize), sizeof(TotalSizeType));
        // 2. 头部
        PacketHead head;
        head.MsgId = pPacket->GetMsgId();
        MemcpyToBuffer(reinterpret_cast<char*>(&head), sizeof(PacketHead));
    }
    // 3. 数据
    MemcpyToBuffer(pPacket->GetBuffer(), pPacket->GetDataLength());
}
```

写入 HTTP 结构的 Packet 和逻辑 Packet 的代码：

当协议是 HTTP 时，上面的代码做了分支的处理，它没有自定义的头部数据了。HTTP 的头在

Packet 类的 Buffer 中已经写好了，所以直接发送即可，这样它就会发送到浏览器上。整个处理请求

数据的流程如图 9-2 所示。

**图 9-2 清楚地呈现了HTTP数据在框架中的走向。简单来说，是先读取数据生成请求Packet，**

Packet 到达处理函数之后，再将返回数据组织成一个返回 Packet，返回 Packet 传递回网络层发送给

对方。这样就形成了一个完整的 HTTP 响应。

**图 9-2 HTTP请求流程**


## 9.5 为Packet定义新的网络标识

在本例中，除了增加 HTTP 部分外，还有一个网络层的变化。在前面的示例

中，为了让读者更容易理解网络层，我们仅采用 Socket 值来标识一个网络，简

化了网络层的一些数据。

使用 Socket 单一值来标识一个网络通道其实是有不确定性的。既然给了一

个通行牌，每人一个，为什么会有不确定性呢？它的不确定性主要归于异步。

当我们使用多线程一瞬间密集向一个端口发起连接请求时，Socket 值可能重

用。说得更具体一点，当上千个连接同时发送某个网络端口时，创建 Socket 会

有一个报错，Windows 系统下是 10035，Linux 系统下是 35，对于这个错误，大

部分情况下认为不是错误，原因是对端口处理不过来了，对于发来的请求没有

回应，但这个通道并没有关闭，是一个等待状态。当对端处理完成时，它会向

连接端发起一个写指令。这时，这个通道就打开了。

在我们的框架中，当大量机器人密集向服务器发起 Socket 连接，又是多线

程连接时，在 Windows 下非常容易出现 Socket 同值的情况，因为 Windows 下的

Socket 值是随机的。也就是说，两个 Robot 类可能共用了一个 Socket 值，听上

去似乎不可能，可以做一个实验，用 2000 个线程同时连接一个端口，就会发现

有概率创建相同值的 Socket。

假设有一个 Socket 值为 1001，被两个 Robot 类分配到了。对于网络层来

说，当第一个对象连接时，服务器处理不过来，返回了 35 错误，这个请求在服

务端可能已经被抛弃了，但 Robot A 认为可以等待，并没有关闭这个 Socket，

第二个 Robot B 刚好又分配到了 1001，它向服务器发起了一个连接请求，这时

服务器同意了，这样 Robot A 和 Robot B 都拿到了这个号码牌，它们都认为自己

已经连接成功。A 和 B 轮询了同一个 Socket 值，注意通道只有一个。这是一种逻

辑上的错误，这种情况在 Linux 和 Windows 上都可能出现。

即使没有出现上述情况，还有另一种情况。假设 Robot A 使用值为 1001 的

Socket 进行登录，在这个过程中，某些原因导致底层网络中断，这时底层网络

会向逻辑层发送一个断开消息，在这个消息还在消息队列中等待下一帧处理的

时候，逻辑层在同一帧对 1001 发送了一条数据，这条数据放在了发送队列中待

发送。好巧不巧，这时有一个新来的玩家 Robot B 登录了，正好重用了 1001。

这将产生什么样的后果呢？很有可能在下一帧，这个新上线的 Robot B 将莫明

其妙地收到一条数据，这条数据原本是想发给 Robot A 的。发送 Packet 包中，

标记的 Socket 为 1001，发送给 Robot B 并没有错。因为值为 1001 的 Socket 已经

重新被分配给 Robot B 了，但实际上我们想要的效果并不是这样的。

综上所述，单纯依靠 Socket 来标识一个玩家是行不通的，需要使用一种新

的网络身份法则。在这个新的网络身份中，我们不再使用单一的 Socket 值来定

义一个网络通道。框架中定义了一个新的 NetworkIdentify 结构，该定义在

09_02_http 工程的 socket_object.h 文件中。在这个结构中有两个关键值：

SocketKey 和 ObjectKey。相当于绳子的两边，一边绑定 Socket 数据，另一边绑

定逻辑层的对象数据。一个 Packet 到达网络层需要对这两个值进行判断。

针对上面的情况，假如 Robot A 已经断开，新来 Robot B，即使 SocketKey

与 Packet 中的 SocketKey 完全一致，但 ObjectKey 必然与旧的 ObjectKey 不一


```cpp
struct NetworkIdentify {
public:
    NetworkIdentify() = default;
    NetworkIdentify(SocketKey socketKey, ObjectKey objKey);
    SocketKey GetSocketKey() const { return _socketKey; }
    ObjectKey GetObjectKey() const { return _objKey; }
protected:
    SocketKey _socketKey{ INVALID_SOCKET, NetworkType::None };
    ObjectKey _objKey{ ObjectKeyType::None , {0, ""} };
};
```

致，只要有不一致，Packet 就需要被丢弃。结构 NetworkIdentify 定义如下：

框架中至少有 4 种不同类型的 NetworkType，SocketKey 结构标识出


```cpp
struct SocketKey {
    SOCKET Socket;
    NetworkType NetType;
    ...
};
struct ObjectKey {
    ObjectKeyType KeyType{ ObjectKeyType::None };
    ObjectKeyValue KeyValue{ 0, "" };
    ...
}
```

NetworkType，方便找到目标。SocketKey 结构和 ObjectKey 结构定义如下：

ObjectKey 结构的使用是多样的，其枚举如下：


```cpp
enum class ObjectKeyType {
    None = Proto::NetworkObjectKeyType::ObjectKeyTypeNone,
    Account = Proto::NetworkObjectKeyType::ObjectKeyTypeAccount,
    App = Proto::NetworkObjectKeyType::ObjectKeyTypeApp,
};
```

在使用枚举时，采用了在 protobuf 中定义的枚举，有 3 个值：None、

Account 和 App。如果是 Robot 类或客户端与服务端的连接，就为

ObjectKeyType::Account。除了玩家之外，需要进行网络连接的还有可能是

App 之间的连接。下面用实例代码来说明新机制。


### 9.5.1 使用网络标识创建一个连接

在前面的工程中，在线程中创建 Robot 对象时，每个 Robot 对象都是一个 NetworkConnector，每个

NetworkConnector 都有一个 Update 事件需要处理，毕竟每帧都需要查看数据。如果有 1000 个 Robot 登录，就有 1000 个对

象，这 1000 个对象要执行 1000 次::select 或::epoll 操作。

再来看一看 NetworkListen 对象是如何处理的，因为所有的对象都来自监听端口，所以所在的连接在监听上的对象

都在一个 NetworkListen 中，即使有 1000 个连接，每一帧也仅执行一次::select 或::epoll 函数。

鉴于此，在 09_02_http 目录中重构了 NetworkConnector 类，希望它能够像 NetworkListen 对象一样，即使有 1000 个

对象，每一帧也只执行一次::select 或::epoll 函数。将 NetworkConnector 类改为一个容器，而不再是一个

NetworkConnector 类实例对应一个网络通道。在这个容器中，每个 ConnectObj 都是一个连接通道，进程内所有的对外

连接都由 NetworkConnector 类维护。

按照之前的流程，在创建 NetworkConnector 时就会将需要连接的 IP 和端口传递给类实例，现在它变成了容纳多个

连接的容器。一个指定的连接是如何产生的呢？

连接一般都是主动的，不像监听产生的通道是在监听类中被动产生的。在工程中有两种主动连接：

·一种是 login 进程需要主动连接到 appmgr 进程上。

·另一种是像 Robot 类，要求连接到某个指定的 login 进程上。

这两种连接方式不同，采用了不同的处理流程。login 进程到 appmgr 之间的连接是可控的，是预先已经十分明确


```cpp
pThreadMgr->CreateComponent<NetworkConnector>(ConnectThread, false, (int)NetworkType::TcpConnector, (int)
```

的，所以采用的是初始化创建。打开 login 工程中的 main.cpp 文件，创建 NetworkConnector 类的代码如下：

在创建 NetworkType::TcpConnector 类型的 NetworkConnector 类时，参数已经说明了 NetworkConnector 要优先创

建两个 App 连接，分别指向 appmgr 和 dbmgr 两个进程。

再来看看 Robot 的主动连接，采用的是协议方式。当 Robot 需要进行一个连接时，无论是对 login 还是 HTTP 的连接都


```protobuf
message NetworkConnect {
    int32 network_type = 1;
    NetworkObjectKey key = 2;
    string ip = 3;
    int32 port = 4;
}
```

需要发起一个协议 MI_NetworkConnect。对应的协议体结构为 Proto::NetworkConnect，结构定义如下：

该结构的第一个属性 network_type 指定连接的类型是进行 TCP 连接还是进行 HTTP 连接，属性 key 给出连接的

ObjectKey。对于 Robot 来说，它需要给出一个类似{ObjectKeyType::Account，{0，Account}}这样的键（Key）。最

后在这个协议中还要给出连接对方的地址与端口，毕竟没有网络地址怎么实现网络连接呢。


```cpp
void NetworkConnector::HandleNetworkConnect(Packet* pPacket) {
    auto proto = pPacket->ParseToProto<Proto::NetworkConnect>();
    if (proto.network_type() != (int)_networkType)
        return;
    ObjectKey key;
    key.ParseFromProto(proto.key());
    ConnectDetail* pDetail = new ConnectDetail(key, proto.ip(), proto.port());
    _connecting.AddObj(pDetail);
}
```

处理协议 MI_NetworkConnect 的是 NetworkConnector 类。来看看它对协议的处理实现：

NetworkConnector 类在 login 工程中有两个实例：一个用来处理 HTTP;另一个用来处理游戏逻辑协议。处理 HTTP 的

NetworkConnector 实例，主要任务是向外部（本书中是 Nginx）发送 HTTP 请求;而处理游戏逻辑 TCP 协议的

NetworkConnector 实例，主要任务是处理 login 主动向外发起的 TCP 连接，例如连接到 dbmgr 和连接到 appmgr 的网络连

接。

因此，开始处理函数时就对 NetworkType 进行了判断，与自己不符合的协议不进行处理。随后，框架中增加了一个

ConnectDetail 结构用于尝试连接，每一个想要创建连接的请求，其数据都会存在于一个 ConnectDetail 结构中，等待

下一帧处理。下一帧，ConnectDetail 结构被删除，一个新的 ConnectObj 生成。尝试连接的代码在每帧中处理如下：


```cpp
void NetworkConnector::Update() {
    // 有新的连接请求
    if (_connecting.CanSwap())
        _connecting.Swap(nullptr);
    // 创建新的连接
    if (!_connecting.GetReaderCache()->empty()) {
        auto pReader = _connecting.GetReaderCache();
        for (auto iter = pReader->begin(); iter != pReader->end(); ++iter) {
            if (Connect(iter->second)) {
                _connecting.RemoveObj(iter->first);
            }
        }
    }
    Epoll();
    OnNetworkUpdate();
}
```

在这段代码中没有加锁，因为协议处理与 Update 操作是串行的，所以没有必要加锁。当 NetworkConnector 在执行

Update 函数时，发现有了新的连接数据，ConnectDetail 实例就会被取出来，调用 Connect 函数尝试连接，给它创建一

个 ConnectObj。

关于 NetworkConnector::Connect（ConnectDetail*pDetail）这个函数的具体实现，这里就不再详细讲解了，

可参见源代码。总之，它让一个请求转换成一个处于 Connecting 状态的 ConnectObj。

需要注意的是，在之前的逻辑中，认为一个 ConnectObj 一定是一个可用的网络连接，但现在它不是了。它有了一


```cpp
enum class ConnectStateType {
    None,
    Connecting,
    Connected,
};
```

个状态：

对于 NetworkConnector 类来说，它维护的 ConnectObj 数据大部分先是处于 Connecting（连接中）状态，随后才是

Connected（已连接）状态。而对于 NetworkListen 实例来说，它维护的 ConnectObj 数据全部是 Connected 状态，因为一

个通过::accept 函数得到的通道一般都是已经完成了 3 次握手的连接。因此，在 NetworkListen 类中的 ConnectObj 对

象不存在 Connecting 这个状态。

不论是 NetworkListen 还是 NetworkConnector，如果产生了断线，这个 ConnectObj 就被删除了。一个 ConnectObj 被

销毁之后，断线协议一定会发送到逻辑上层。如果它是一个玩家，就有相应的下线处理。如果这个连接的 ObjectType

表明它是一个服务端进程之间的连接，那么一个重连接协议将被发起，协议号依然是 MI_NetworkConnect，具体的代码

可参见 NetworkLocator 组件。


### 9.5.2 使用网络标识发送数据

为了适应新的网络身份机制，需要改造 ConnectObj 连接对象。首先它一定是一个 NetworkIdentify，拥有网络标

识，一头绑定了网络，另一头绑定了一个逻辑层的对象。在创建 ConnectObj 时需要输入 NetworkIdentify 相关数据。


```cpp
class ConnectObj : public Entity<ConnectObj>, public NetworkIdentify, public IAwakeFromPoolSystem<SOCKET,
public:
    void Awake(SOCKET socket, NetworkType networkType, ObjectKey key, ConnectStateType state) override;
```

ConnectObj 类的唤醒函数定义如下：


```cpp
}
```

...

同时，还为 ConnectObj 准备了一个状态。这一点不难理解，连接是一个异步过程，只有确认已经连接成功了，才

认为这个通道是可用的。当网络层传来可读或可写时，这个通道就成功创建了，这时会调用函数


```cpp
void ConnectObj::ChangeStateToConnected() {
    _state = ConnectStateType::Connected;
    if (GetObjectKey().KeyType == ObjectKeyType::App) {
        auto pLocator = ThreadMgr::GetInstance()->GetEntitySystem()->GetComponent<NetworkLocator>();
        pLocator->AddNetworkIdentify(GetObjectKey().KeyValue.KeyInt64, GetSocketKey(), GetObjectKey());
    } else {
        // 通知逻辑层连接成功了
        MessageSystemHelp::DispatchPacket(Proto::MsgId::MI_NetworkConnected, this);
    }
}
```

ChangeStateToConnected 来改变 ConnectObj 的状态。

一旦成功连接，需要向逻辑层发送一个 MI_NetworkConnected 协议，通知之前给我们 ObjectKey 的对象连接成功

了，同时将当前的 SocketKey 赋值给 ObjectKey 的对象。还是以 Robot 为例，看看 MI_NetworkConnected 协议的处理。

Robot 类中对于该协议的处理函数为 HandleNetworkConnected，在 Robot 类从对象池中被唤醒时，进行了协议号的处理


```cpp
void Robot::Awake(std::string account) {
    // message
    auto pMsgCallBack = new MessageCallBackFunctionFilterObj<Robot>();
    AddComponent<MessageComponent>(pMsgCallBack);
    pMsgCallBack->GetPacketObject = [this](NetworkIdentify* pIdentify) {
        if (_objKey == pIdentify->GetObjectKey())
            return this;
        return static_cast<Robot*>(nullptr);
    };
    pMsgCallBack->RegisterFunctionWithObj(Proto::MsgId::MI_NetworkConnected, BindFunP2(this, &Robot::Hand
    ...
}
void Robot::HandleNetworkConnected(Robot* pRobot, Packet* pPacket) {
    if (_socketKey.NetType != NetworkType::None && _socketKey.Socket != INVALID_SOCKET) {
        // 有新的连接后，旧的连接需要关闭
        MessageSystemHelp::DispatchPacket(Proto::MsgId:: MI_NetworkRequestDisconnect, this);
    }
    _socketKey = pPacket->GetSocketKey();
}
```

函数的注册，其注册与实现代码如下：

Robot 在生成时注册了 MI_NetworkConnected 协议的处理函数。当收到连接成功之后，会将生成的 SocketKey 赋值给

自己本地的 SocketKey。在注册这个函数时，注意使用的是 RegisterFunctionWithObj 方法。这是一个过滤流程，一般

情况下，协议是无差别发送给所有线程的，但是 RegisterFunctionWithObj 需要调用 GetPacketObject 函数来过滤对

象，以避免收到不需要的数据。上面的代码中有对于 GetPacketObject 的实现，当 ObjectKey 一致时，就是我们需要接

收 Packet 的对象了。

对于 Robot 的状态类来说，一开始没有连接成功之前，它都是处于正在连接 RobotStateHttpConnecting 状态，这是

等待 HTTP 连接的状态。Robot 类模拟了客户端的操作，首先会发起一个 HTTP 请求，得到一个可以登录的 login 进程信

息，连接成功之后，再发起真正的登录操作。在等待 HTTP 连接的状态下，每一帧都在监听是否连接成功，其处理函数


```cpp
RobotStateType RobotStateHttpConnecting::OnUpdate() {
    auto socketKey = _pParentObj->GetSocketKey();
    if (socketKey.Socket != INVALID_SOCKET) {
        if (socketKey.NetType != NetworkType::HttpConnector) {
```

在状态类中，实现如下：


```cpp
            LOG_ERROR("error.");
        }
        MessageSystemHelp::SendHttpRequest(_pParentObj, _ip, _port, _method, nullptr);
        return RobotStateType::HTTP_Connectted;
    }
    return GetState();
}
```

当 Robot::HandleNetworkConnected 收到网络层的消息，更改了自己的 SocketKey 时，

RobotStateHttpConnectting 状态马上就会反应过来，发现 Socket 已经不是一个非法值了。这时，此状态认为已经连接

成功，会向网络通道发起一个 HTTP 请求。调用的是函数 MessageSystemHelp::SendHttpRequest，在这个请求中没有

参数，最后一个值为 nullptr。同时，Robot 的状态变成了 RobotStateType::http_Connectted。


### 9.5.3 向外部请求HTTP数据

前面我们讲了如何响应从外部发起的 HTTP 请求，现在讨论一下向外部的 HTTP 服务发送一个请求的流程。这个流程

在验证账号和 Robot 类向 appmgr 请求 login 进程信息时都会用到。


```cpp
MessageSystemHelp::SendHttpRequest(_pParentObj, _ip, _port, _method, nullptr);
```

以 Robot 类为例向 appmgr 请求数据时，调用代码如下：


```cpp
void MessageSystemHelp::SendHttpRequest(NetworkIdentify* pIdentify, std::string ip, const int port, const
    Packet* pPacket = CreatePacket(Proto::MsgId::MI_HttpOuterRequest, pIdentify);
    std::stringstream buffer;
    buffer << "GET " << method;
    if (pParams != nullptr) {
        buffer << "?";
        for (auto iter = pParams->begin(); iter != pParams->end(); ++iter) {
            buffer << iter->first << "=" << iter->second << "&";
        }
    }
```


```cpp
    buffer << " HTTP/1.1" << "\r\n";
    buffer << "Host: " << ip.c_str() << ":" << port << "\r\n";
    buffer << "Content-Type: application/json; charset=utf-8" << "\r\n";
    buffer << "Accept: */*,text/*,text/html" << "\r\n";
    buffer << "\r\n";
```


```cpp
    pPacket->SerializeToBuffer(buffer.str().c_str(), buffer.tellp());
    SendPacket(pPacket);
}
```

该函数的实现如下：

在该函数中写了一个 HTTP 格式的请求协议，并将数据发送出去。当 ConnectObj 收到返回协议时，数据封装成

MI_HttpOuterResponse 协议号的 Packet，这个 Packet 在 ConnectObj 中生成，附加上了 ConnectObj 类的 NetworkIdentify


```cpp
void Robot::HandleHttpOuterResponse(Robot* pRobot, Packet* pPacket) {
    auto protoHttp = pPacket->ParseToProto<Proto::HTTP>();
    const int code = protoHttp.status_code();
    if (code == 200) {
        const auto response = protoHttp.body();
        Json::CharReaderBuilder readerBuilder;
        Json::CharReader* jsonReader = readerBuilder.newCharReader();
        Json::Value value;
        jsonCPP_STRING errs;
        const bool ok = jsonReader->parse(response.data(), response.data() + response.size(), &value, &er
        if (ok && errs.empty()) {
            const auto code = value["returncode"].asInt();
            if (code == Proto::LoginHttpReturnCode::LHRC_OK) {
                _loginIp = value["ip"].asString();
                _loginPort = value["port"].asInt();
                ChangeState(RobotStateType::Login_Connectting);
            }
        } else {
            LOG_ERROR("json parse failed. " << response.c_str());
        }
        delete jsonReader;
    } else {
        LOG_ERROR("http response error:" << code << "\r\n" << protoHttp.body().c_str());
    }
}
```

信息，该信息只会与唯一一个 Robot 对应。在 Robot 中对于这个协议的处理如下：

收到正确的数据后，Robot 会得到正确的 login 进程的 IP 地址，此时调用了 ChangeState（RobotStateType::

Login_Connectting），Robot 类的状态从 HTTP 阶段修改为与 login 正在连接的状态。之后的流程就不再介绍了，回到了

之前熟悉的工程流程上。流程图如图 9-3 所示，总结了这些步骤。

**图 9-3 Robot登录流程**

现在，启动 robots 进程，批量登录 1000 个账号，这 1000 个账号的网络处理都会集中到一个 NetworkConnector 中处

理。

除了 Robot 类之外，login 进程在验证账号时向 Nginx 发送了一个 PHP 请求，用于模拟平台账号验证。之前是使用

curl 第三方库实现的。现在框架实现了自己的 HTTP 处理，可以不再需要 curl 库了。其实现原理和 Robot 类向服务端发送

HTTP 请求一致。简而言之，当 Account 类收到 C2L_AccountCheck 账号验证协议后，发起一个 MI_NetworkConnect 协议想

要创建一个连接，在协议的 ObjectKey 标志中带上自己的标识。当这个请求成功之后，会收到来自底层的

MI_NetworkConnected 协议，表示连接成功，随后向这个网络标识发送一个 HTTP 的验证请求。请求发送之后会收到

MI_HttpOuterResponse 协议，得到验证的结果。具体功能参见源代码，这里不再阐述。


## 9.6 HTTP分块

在前面的章节中阐述了 HTTP 的协议格式与框架中的 HTTP 数据的收发流程。本节我们回到 HTTP 格式上，再来说

明一下 HTTP 的块数据传递是怎么回事。

单从代码来看很难厘清它的格式。为了测试数据分块，在 robots 工程中写了一个测试类 TestHttpLogin，本

节的源代码依然在 09_02_http 目录中。

启动 robots 进程，启动完成之后输入“http-check test test”，这个指令发起了一个验证账号的 HTTP 请

求。这个 HTTP 请求是 Nginx 下的一个 PHP 接口。修改 engine.yaml 文件，确保配置中的 IP 正确。其调用之后的请求

数据如下：

http -check test test

connected. socket:6 networkType:HttpConnector connect type:Account value:test

GET /member_login.php?account=test&password=test& http/1.1

Host: 192.168.0.120:80

Content-Type: application/json; charset=utf-8

Accept: */*,text/*,text/html

返回数据如下：

http/1.1 200 OK

Server: nginx/1.12.2

Content-Type: text/html; charset=utf-8

Transfer-Encoding: chunked

Connection: keep-alive

X-Powered-By: PHP/5.4.16

( 回车空行）


```cpp
{"returncode":2}
```

10( 长度 )

0( 长度 )

( 回车空行）

从打印输出的数据来看，在请求返回数据中并没有 Content-Length 属性，没有长度也就不知道请求返回数据

有多长，但有 Transfer-Encoding：chunked 属性说明数据是按块传递的。要理解这个原理，可以举一个例子，我

们刷新网页时会先弹出一部分内容，过一会儿再弹出全部内容。

基于用户体验，在 HTTP 中有一个机制，不依赖头部的长度也可以快速知道传输的内容。也就是说，HTTP 头结

束之后，即在/r/n/r/n 之后的数据是一个数据串，这个串的格式是：长度+/r/n+数据+/r/n。长度值不包括它结

尾的结束符（\r\n），也不包括分块数据结尾的结束符。最后一个分块长度值必须为 0，对应的分块数据没有内

容，表示协议结束。

如果一个网页有 100KB，发送请求之后返回了头部，这时头部就有属性 Transfer-Encoding：chunked。表 9-1

整理了每一步的数据内容，以帮助理解这个过程。

表 9-1 HTTP 分块数据

当然，在真正的网络传输过程中，并不是真的一次一次这样规整地收到数据，有可能第二段数据与第三段数

据一起到达网络缓冲区，也有可能到达网络缓冲区的只有第二段一半的数据。鉴于此，回到代码中，在


```cpp
bool isChunked = false;
const auto mgTransferEncoding = mg_get_http_header(&hm, "Transfer-Encoding");
if (mgTransferEncoding != nullptr && mg_vcasecmp(mgTransferEncoding, "chunked") == 0) {
    isChunked = true;
    // 后面的数据还没有到达
    if (recvBufLength == headerLen)
        return nullptr;
    bodyLen = mg_http_get_request_len(_buffer + _beginIndex + headerLen, recvBufLength - headerLen);
    if (bodyLen <= 0)
        return nullptr;
    bodyLen = _endIndex - _beginIndex - headerLen;
}
```

RecvNetworkBuffer::GetHttpPacket 函数分析数据时有一个等待长度的操作：

在这一段代码中，mg_http_get_request_len 函数本来是用来判断 request 头部分长度的，但是我们用这个函

数来判断块数据是否接收完成。它们有一个共同的标志就是以双/r/n 或 0\r\n 结束。一定要等到结束符到来，这

个块的返回结果才是完整的，只有所有块都接收完成了，生成的 Packet 才是完整的。


## 9.7 机器人测试批量登录

在本章中解决了两个问题：

• （1）HTTP 的监听数据如何处理外部对服务端的请求并正确返回。

• （2）HTTP 的请求数据如何处理向外部 HTTP 服务发送请求并正确返回。

关于 HTTP 的内容就写到这里，鉴于之前关于网络的内容已经有很多了，这部分尽量减少网络相关

内容，以避免枯燥，但是 09_02_http 目录中的项目与之前的项目在网络机制上非常不同，如果没有理

解，不妨调试一下源代码。不同之处主要表现在两个方向：

• （1）网络识别不再是 Socket，而是 NetworkIndentify 结构。

• （2）在网络内存组织上的不同。旧工程中的每个连接都要生成一个 NetworkConnector，而最新工

程是放在一个 NetworkConnector 中的。这两者之间的比较可以对网络模型有一个更加深刻的认识。一

个进程中的两个线程，如果数据要共享的话，就必须加锁，但是对于网络通道来说，只要 Socket 是给

定的，那么它在任何一个线程都可以读出数据。即使是监听 Socket，也与其监听对象的通道是独立

的，即一方的关闭与异常不会影响另一方。

为了证明这个观点，我们可以做一个测试，修改一下监听的 NetworkListen::Update，在头部增


```cpp
void NetworkListen::Update() {
    if (!_connects.empty() && _masterSocket != INVALID_SOCKET) {
        DeleteEvent(_epfd, _masterSocket);
        _sock_close(_masterSocket);
        _masterSocket = INVALID_SOCKET;
    }
    _mainSocketEventIndex = -1;
    epoll();
    ...
}
```

加几行代码：

在头部的 if 中，我们写了一个条件判断，如果 connects 集合不为空，就将监听端口从 epoll 的队列

中移除，并将它关闭。也就是说，只要有一个网络通道打开，我们不再关心监听端口的数据。即使不

关心监听端口，但已打开的通道却并没有受限，还是可以正常地收发数据的。这时，再启动一个

robots 进程来进行登录，却无法登录成功了，因为监听端口已经关闭，数据没有在逻辑层处理了。

这个例子说明，无论是::epoll 函数还是::select 函数中传入的每一个 Socket 值都是独立的，

它们之间没有因果关系。即使将一个监听的 Socket 和一个主动连接生成的 Socket 放在一起调用::

epoll 函数或::select 函数，也是完全没有问题的。可能网络通道生成的方式不同，但是一旦生成，

在读取或写入数据时就是完全相同并且独立的。

按照这个思路，是不是可以将连接类和监听类合并起来呢？我们将这个问题留给读者自己思考，

可以在 09_02_http 的项目中动手试一试。希望通过本章的介绍，读者能对网络底层收发有更加深刻的

认识。

下面对 Robot 类的登录步骤进行简短的总结，以梳理分布式登录的流程。在之前的工程中，login

进程只有一个，Robot 类直接连接到 login 进程上。现在 login 有了多个进程，登录流程需要做出调整，

首先向 appmgr 发起一个 HTTP 请求，得到 login 进程的登录地址。着重看一下新增部分的代码，首先需要

重新定义 Robot 类的状态，Robot 类增加了 Http_Connecting 和 Http_Connected 两个新的状态。在

Http_Connecting 态下，Robot 需要向服务端发送一个 HTTP 连接，如果连接成功就进入 Http_Connected

状态，发起 HTTP 请求得到一个 JSON 数据，分析出登录的 login 进程的 IP 地址，再发起 TCP 连接。Robot 的


```cpp
enum class RobotStateType {
    None = -1,
    HTTP_Connecting = 0,            // HTTP 请求
    HTTP_Connected,
```

状态在 libserver 工程中的 robot_state_type.h 文件中：

Login_Connecting,               // 正在连接  Login

Login_Connected,                // 连接成功

Login_Logined,                  // 登录成功


```cpp
};
```

...


```cpp
void RobotStateHttpConnecting::OnEnterState() {
    Proto::NetworkConnect protoConn;
    protoConn.set_network_type((int)NetworkType::HttpConnector);
    ObjectKey key{ ObjectKeyType::Account, {0, _pParentObj->GetAccount()} };
    key.SerializeToProto(protoConn.mutable_key());
    const auto pYaml = ComponentHelp::GetYaml();
    const auto pCommonConfig = dynamic_cast<CommonConfig*>(pYaml->GetConfig(APP_APPMGR));
    protoConn.set_ip(pCommonConfig->Ip.c_str());
    protoConn.set_port(pCommonConfig->HttpPort);
    _ip = pCommonConfig->Ip;
    _port = pCommonConfig->HttpPort;
    _method = "/login";
    MessageSystemHelp::DispatchPacket(Proto::MsgId::MI_NetworkConnect, protoConn, nullptr);
}
```

一旦 Robot 实例生成，它的初始状态是一个 Http_Connecting。状态的进入函数事件实现如下：

状态类从 engine.yaml 配置文件中取读了 appmgr 的 HTTP 地址与端口，同时创建一个新的 HTTP 连接。

engine.yaml 这个配置本来是在服务器上的，因为 Robot 和服务端的进程是放在一起的，所以直接读取

了。在真实游戏部署的时候，一般采用域名的方式来获取登录进程数据。

发送连接请求之后，Robot 就只有等待了。当连接成功时，NetworkConnector 类会发送一个

MI_NetworkConnected 协议。Robot 处理这个协议时，将重置自己的 SocketKey。一旦状态检查到

SocketKey 发生了改变，就会马上做出反应，将状态改为 Http_Connected，且将请求数据发送出去。

HTTP 返回数据是 JSON 类型的，如果解析成功，Robot 就会进入 Login_Connecting 状态。

关闭之前打开的 LOG_HTTP_OPEN 宏，重新编译一下工程，来看看 Robot 进程测试 1000 个账号登录的


```bash
[root@localhost bin]# ./robotsd
```

效率。

...

Http-Connecting over. time:0.153479s

Http-Connected over. time:0.919222s

Login-Connecting over. time:1.40633s

Login-Connected over. time:2.04906s

Login-SelectPlayer over. time:3.07127s

经过几次反复的测试，登录 1000 个账号的时间在 3 秒之内，上面的数据给出了每一个阶段消耗的时

间。数据是在 Linux 下测试的，比起之前的登录流程增加了 1000 个 HTTP 数据的请求。


## 9.8 总结

本章中，我们为框架增加了 HTTP 的格式分析，提供了 HTTP 服务。在分布式

登录中，启动了多个 login 进程，这些进程会定时将自己的当前状态发送到

appmgr 进程中，appmgr 提供了 HTTP 服务，客户端可以通过 HTTP 请求得到一个用

于登录的 login 进程的信息，以实现登录。

现在，我们已经从多个 login 中选择了一个进程登录，第 10 章将进入 game

游戏逻辑进程。
