---
title: "网络多人在线游戏开发实战教程 · 附录A 在CentOS上建立开发环境"
category: 教材原文
type: 原文章节稿
source_book: "网络多人在线游戏开发实战教程"
source_section: "附录A 在CentOS上建立开发环境"
tags:
  - 教材原文
  - 彭放教程
  - C++服务器
source: "《网络多人在线游戏开发实战教程》（彭放，机械工业出版社 2019，ISBN 9787111667926）附录A 在CentOS上建立开发环境 · md 稿提取（文字层 + 图表 OCR 回填），保留原文未净化"
created: 2026-10-07
skills:
  - obsidian-markdown
---

# 附录A 在CentOS上建立开发环境

> **原文章节稿 · 未经净化。** [[网络多人在线游戏开发实战教程|← 返回本书目录]]
> 提取自 PDF（文字层 + 图表视觉 OCR 回填，138 张图表）。**保留原书全部技术细节与代码，未做删减或改写**。各章末尾的「图表转写」是该章配图的文字转录；原始裁图在 `_raw/B_img/`。

## 本章小节

- [[#1. CentOS 安装器：存储分区设置页|1. CentOS 安装器：存储分区设置页]]
- [[#2. CentOS 安装器：New CentOS 7 Installation 挂载点面板|2. CentOS 安装器：New CentOS 7 Installation 挂载点面板]]
- [[#3. CentOS 安装器：SYSTEM 挂载点列表|3. CentOS 安装器：SYSTEM 挂载点列表]]
- [[#4. MobaXterm 设置：鼠标（Mouse）配置项|4. MobaXterm 设置：鼠标（Mouse）配置项]]
- [[#5. MobaXterm 设置：终端/外观设置面板|5. MobaXterm 设置：终端/外观设置面板]]
- [[#6. MobaXterm：新建 SSH 会话配置对话框|6. MobaXterm：新建 SSH 会话配置对话框]]
- [[#7. 常用文件与目录操作命令表（第 1 部分）|7. 常用文件与目录操作命令表（第 1 部分）]]
- [[#8. 常用文件与目录操作命令表（第 2 部分）|8. 常用文件与目录操作命令表（第 2 部分）]]
- [[#9. yum 安装与更新命令表（第 1 部分）|9. yum 安装与更新命令表（第 1 部分）]]
- [[#10. yum 查询命令表（续表）|10. yum 查询命令表（续表）]]
- [[#11. vim 插入模式启动界面|11. vim 插入模式启动界面]]
- [[#12. vim 插入模式状态栏|12. vim 插入模式状态栏]]
- [[#1. vim 中输入 `:wq` 保存并退出|1. vim 中输入 `:wq` 保存并退出]]
- [[#2. vim 基本命令表|2. vim 基本命令表]]
- [[#3. git 常用命令表|3. git 常用命令表]]
- [[#4. CMakeLists.txt 指令表|4. CMakeLists.txt 指令表]]
- [[#5. gcc 常用编译选项说明表|5. gcc 常用编译选项说明表]]
- [[#6. gdb 常用命令表|6. gdb 常用命令表]]

> **尚未净化** —— 本章还没有对应的详解笔记。

---

A . 1 　 安 装 C e n t O S  7

本书使用的 Linux 版本为 CentOS 7。本节将为初次接触 Linux 的读者阐明如何在 Windows 下安装一个虚拟的 CentOS 系

统，以方便读者更好地运行本书的 Demo。我们需要使用如下两个基本软件包：

1.VirtualBox （ Oracle 虚拟化开源产品之一）

VirtualBox 新版的下载地址为 https://www.virtualbox.org，下载一个新的 Windows 版本即可。同时，在下载界面

中需要下载扩展工具包 VirtualBox 6.0.10 Oracle VM VirtualBox Extension Pack。VirtualBox 安装完成之后，可以

直接执行扩展包，它会自行安装。

2.CentOS 系统

CentOS 是一个 Red Hat（红帽子）企业级 Linux 发行版本的社区发行版（同一源码编译，只是换掉了 Logo 与包管理

源地址）。CentOS 系统安装时推荐无图像界面，即命令行模式。有关 CentOS 本书就不再详细介绍了，如果习惯了

Windows 系统的界面，可能开始时会不太习惯，但是长久工作之后，相信你会被这种无窗口、命令行的模式所吸引，因

为它的确高效。

CentOS 7 系统包可以在阿里的开源网站下载，阿里开源网站的链接为 https://opsx.alibaba.com/mirror。在主界

面的右边单击“下载 OS 镜像”，这里可以下载 CentOS 的很多版本。虽然已经有了更高的版本，但是我们还是选择更为

稳定的 CentOS 7，选择一个新的最小版本，Minimal 即为最小版本。

A.1.1 创建虚拟机

安装完 VirtualBox，在菜单上选择“控制”→“新建”，弹出创建界面，单击下方的“专家模式”进行设置。需

要注意以下几点：

• （1）名称可以随意写，方便记住即可。这里我们设置为 CentOS Game Dev。

• （2）我们选择虚拟机的类型为 Linux，版本基于 Red Hat 64 位系统。

• （3）内存大小设置，根据机器的性能划分，如果你的计算机有 16GB 的内存，可以划分 4GB 给虚拟机使用，如果你

的计算机内存不够大，那么可以选择 1GB 或 2GB。

• （4）虚拟硬盘选择为“现在创建虚拟硬盘”。

单击“创建”按钮，弹出新界面，其配置说明如下：

• （1）文件大小设置。这个参数相当于正常系统的硬盘大小，根据你的需求可以随意修改，这里我们输入 80GB，意

味着这个虚拟空间最多可以装载 80GB 的数据。对于开发用的 Linux 而言，80GB 完全够用了。

• （2）虚拟硬盘文件类型，这里默认为 VDI，这是 VirtualBox 的默认格式。

• （3）存储方式选择“动态分配”，这意味着之前我们定义的 80GB 空间不是一次性分配给虚拟系统的，它用多少就

分配多少，占用空间的大小是自动增长的。而“固定大小”则是一开始就为我们创建 80GB 的硬盘空间。这两种分配方

式各有利弊，我们选择“动态分配”，能够节约硬盘空间。“固定大小”速度略快于“动态分配”，可以根据你的需

求进行选择。

单击“创建”按钮，创建界面消失之后，在 VirtualBox 主界面的左边会列出新建的虚拟机。在运行虚拟机之前，

我们需要先对它进行一些设置。选中虚拟机并右击，从快捷菜单中选择“设置”命令，会弹出设置界面。

• （1）单击“系统”的第二个标签“处理器”，选择一个合适的处理器数量。如果你拥有 8 个 CPU，可以选择 2 或 4

CPU。

• （2）单击“网络”选项。我们为系统设置网络为桥连接。

单击下方的 OK 按钮，回到主界面上，单击主界面右侧的“启动”按钮。第一次启动时，会先让我们选择启动盘。

单击右侧黄色的文件夹图标，选中之前下载的系统安装文件 CentOS 7 的 ISO 文件，单击“启动”按钮进行安装。

A.1.2 CentOS 的安装参数

启动之后会弹出一个新的窗口，出现 60 秒等待安装界面，随后是欢迎界面。如果你是第一次使用 VirtualBox，在

安装系统的过程中，可能会发现鼠标变成了黑色，被禁锢在 VirtualBox 的小窗口之中，不能移出这个窗口。这时，可

以按下键盘右侧的 Ctrl 键，鼠标会回到 Windows 系统中。

安装时默认的语言是英文，我们在安装中可以选择中文，在左侧下拉列表中滑动鼠标中键选择“中文”。单

击“继续”按钮之后，进入安装信息配置。如果选择中文，一定在记得在本地化语言支持中增加英语的支持，作为一

个后台服务器，我们基本不会用到中文，只会使用英文。下面以英文为例逐项进行配置。

• （1）最顶端本地化选择一共有 3 项，分别是日期、键盘和语言支持。因为我们选择的是英文界面，所以只需要修

改日期和时间，将其修改为 Asia/Shanghai 时区。

• （2）禁用 KDUMP。在系统这一部分禁用了 KDUMP，这是一个内核崩溃的反馈报告。因为我们的程序还不会撼动内

核，为了加快性能，取消了没有用的服务。

• （3）单击 Network&Host Name 可以设置网络。首先需要打开右上角的 Off/On 开关，再单击右下角的 Configure。单

击 IPV4 Settings 标签，选择“手动”，单击 Add 按钮增加一个 IP 配置，例如输入 192.168.0.120，这里 Gateway 根据当

前的网络环境情况可能略有不同。同时，设置 DNS 为 223.5.5.5 和 223.6.6.6，这是两个公用的 DNS。单击“保存”按钮

之后，回到之前的界面，看到的配置如图 A-1 所示。

图 A-1 网关配置

单击左上角的 Done 按钮，网络配置成功。这里需要解释一下，为什么必须为虚拟机设置一个 IP，而不是默认 IP。

试想一下，当你想连接服务器时，服务器是一个自动分配的 IP，应该去哪里找 IP 呢？

• （4）分区配置。基本配置完成之后，重头戏是分区的问题，如果你从来没有接触过 Linux 系统，可能会有一点不

适应。在 Windows 系统中，我们将一块硬盘分成 C 盘、D 盘、E 盘等，其中 C 盘是系统盘，可以装上系统或一些软件，而 D

盘或 E 盘可以存放数据。但 Linux 不是这样分区的，简单来说，Linux 是按功能分区的。下面来做一个简单的分区，单击

installation destination，进入分区界面，如图 A-2 所示。

选择 I will configure partitioning（我要配置分区），然后单击左上角的 Done 按钮，进入自主分区界面，如图

A-3 所示。选择 Standard Partition（标准分区），单击左下角的“+”按钮进行分区。

> [插图：简化的跨地图跳转时序。客户端发起跳转后，Server 在原地图上依次完成「发起Teleport」与「收到Teleport返回」，再由目标地图向上回传，最终向客户端下发进入地图协议。]

> [插图：完整的跨地图跳转流程，补上了 Proxy 不存在时的分支——Game 判定「代理地图是否存在」，存在则直接发起 Teleport；不存在则经 AppMgr 向 space 申请创建 World，space 创建完成后回传「创建成功广播」，通知原地图向客户端回传「目标地图创建成功消息」，再由目标地图同步玩家数据并回发 Teleport 协议、最终下发进入地图协议。]

> [插图：客户端带 Token 连接 Lobby 后，Lobby 加载玩家数据并查询目标代理地图是否已加载；已加载则直接下发跳转请求，未加载则向 AppMgr 请求加载，随后由「广播创建消息」触发本地 WorldProxy 的创建。]

> [插图：WorldProxy 收到跳转指令后分两路：一边检查并按需加载目标代理地图，一边向目标 World 请求同步玩家数据；目标 World 收到同步玩家数据后回传「同步玩家数据成功」，两条路径都满足才完成跳转。]

> [插图：客户端只知道固定 IP 端口（Login.test.com:8765），请求先进入 Nginx，再由 Nginx 按权重轮询分发到后端多台 Login 服务器。]

---

**图 A-1 网络配置**（原书该图无图题编号，按附录 A.1.2 正文「Network & Host Name」步骤对应的截图内容判定）

| 项目 | 值 |
| --- | --- |
| 连接名 | Ethernet (enp0s3) |
| 状态 | Connected |
| 开关 | ON |
| Hardware Address | 08:00:27:BF:A6:14 |
| Speed | 1000 Mb/s |
| IP Address | 192.168.0.120 |
| Subnet Mask | 255.255.255.0 |
| Default Route | 192.168.0.1 |
| DNS | 223.5.5.5 223.6.6.6 |

> [插图：CentOS 7 图形安装程序 Network & Host Name 步骤中配置完成后的网卡状态，显示手动指定的静态 IP 192.168.0.120、掩码 255.255.255.0、网关 192.168.0.1，以及 DNS 223.5.5.5 与 223.6.6.6。]

图 A-2 分区配置

图 A-3 标准分区

简单分区规划如下：

①“/boot”：直接在界面上输入“1024MB”，该分区主要用于系统内核镜像文件与启动引导文件，Linux 系统内

核非常小，没有 Windows 系统那样庞大，这里我们选择了 1024MB。

②“Swap”：8GB。交换区是 Linux 的虚拟内存，当内存不足时，数据被暂时放在这里，一般设为内存的 2 倍大小比

较合适，交换区对于用户来说是不可操作、不可见的。

③“/”：默认所有剩余空间。在挂载点选择“/”之后，期望容量不填时就会分配所有余下空间给它。

图 A-4 展示了一个总的分区图。分区完成之后，单击左上角的 Done 按钮完成，会弹出提示分区修改的窗口，单

击“接受更改”后回到主界面。现在所有配置都完成了，单击 Begin Installation 开始安装。安装中可以进行 Root 密

码设置。单击 ROOT PASSWORD 为 root 账号设置密码。等待安装完成，出现 Reboot 按钮，单击重启，即可完成安装工作。

图 A-4 所有分区

---

## 图表转写 · 批次 11（PDF 页 499–507）

> 这一段是提取时按 PDF 页序插进来的图表转录，**与本章正文交错**，
> 其中 `### pXXX_0.jpg` 是配图的文字转录，图页裁图在 `_raw/B_img/`。

> 本批次 12 张图均无文字层图题，编号由转写方按 PDF 页码顺序自拟（批次内序号 1–12）。

## 1. CentOS 安装器：存储分区设置页

```text
Other Storage Options
├─ Partitioning
│  ├─ ○ Automatically configure partitioning.
│  └─ ● I will configure partitioning.   ← 选中（单选按钮已点中）
│     ☐ I would like to make additional space available.（灰显不可选）
└─ Encryption
   ☐ Encrypt my data. You'll set a passphrase next.（灰显不可选）
```

> [插图：CentOS 7 安装器的 "Other Storage Options" 存储分区设置界面截图；因选择了手动分区，"I would like to make additional space available" 与 "Encrypt my data" 两项被灰显为不可用状态。]

## 2. CentOS 安装器：New CentOS 7 Installation 挂载点面板

```text
▼ New CentOS 7 Installation
  You haven't created any mount points for your CentOS 7 installation yet.
  You can:
    • Click here to create them automatically.        ← 超链接（下划线）
    • Create new mount points by clicking the '+' button.
  New mount points will use the following partitioning scheme:
  [ Standard Partition                        ▾ ]   ← 下拉框，鼠标指针悬停其上
```

> [插图：Anaconda 安装器挂载点配置面板，提示尚未创建任何挂载点，并可选择自动创建或手动用 "+" 按钮添加；分区方案下拉框当前为 "Standard Partition"。]

## 3. CentOS 安装器：SYSTEM 挂载点列表

| 挂载点 | 设备 | 大小 |
| --- | --- | --- |
| `/boot` | `sda1` | 1024 MiB |
| `/` | `sda3` | 71 GiB（当前选中行，右侧有 `>` 展开箭头） |
| `swap` | `sda2` | 8192 MiB |

A . 2 　 虚 拟 系 统 的 关 闭 与 备 份

因为系统是虚拟的，所以关闭与启动都必须在 VirtualBox 中完成：

1. 启动

在 VirtualBox 左侧列表中选中要启动的系统，单击右侧上方的“启动”图标即可启动，启动一个虚拟系统之后，

会弹出一个新的窗口。CentOS 启动相当快，提示输入登录名字，此时已经启动成功了。另外，还可以采用无界面启

动，选中要启动的虚拟机并右击，从快捷菜单中选择“启动”→“无界面启动”。

2. 关闭

要关闭虚拟机，直接关闭窗口即可，会弹出一个提示窗口，有 3 个选择：

·快速休眠：就是保持当前的所有数据，下次启动时，内存数据都不会发生改变。假设你启动了一个服务，再启

动时，这个服务还在。

·正常关闭：就是正常的关机。

·强制退出：相当于中断所有正在运行中的数据，拔掉电源强制关机。

这 3 种模式中一般使用“快速休眠”，这样启动和关闭虚拟机都很快。如果采用无界面启动，则关闭时右击，选择

快捷菜单中的“退出”，也会出现如上的 3 种选择。

3. 快照备份

VirtualBox 是一个很重要的功能，特别是对于新手来说，有一个非常好用的功能——快照备份。快照备份的作用

就是建立一个还原点，当我们需要安装一个不是特别确定的软件时，就可以先进行快照备份。当我们完成了一个基础

的系统优化时，也可以创建一个快照备份，以便在之后随时切换回最初的状态。

在主界面中选中虚拟机，右侧有一个“当前状态”。单击上面的“生成”按钮，就可以根据当前状态生成一个系

统快照备份，可以给它一个简单的名字进行备注。

4. 恢复

选中某个备份数据右击，从弹出的快捷菜单中选择“恢复备份”。特别是对于新手来说，在安装一个工具或库之

前，如果你拿不准，就可以备份当前状态;如果安装失败，或配置混乱到已经理不清头绪的时候，可以恢复之前的备

份，重新再试一次。

A . 3 　 终 端 工 具 X S h e l l

每次在 VirtualBox 中启动 CentOS，都会打开一个小小的窗口。一般情况下，我们不会直接在这个窗口上操作，都

是通过第三方工具进行操作。第三方工具有很多，例如 Putty、SecureCRT，本书中使用到的是 XShell，虽然 XShell 是

一个收费软件，但是它有一个免费版，完全够大家日常使用了。

连接 Linux 端的工具，使用的是 XShell 的免费版。XShell 的下载与安装这里不再详述。安装完成之后，有两个设置

需要说明：

• （1）在“工具”菜单中单击“选项”按钮，在“键盘与鼠标”标签中可以看到 XShell 对于鼠标中间按钮与向右按

钮的定义，中间按钮是复制当前选中的内容，而向右按钮是弹出菜单，如图 A-5 所示。如果与你的习惯不相符，那么可

以在这里修改。

图 A-5 XShell 鼠标设置

• （2）在“文件”菜单中单击“默认会话属性”，弹出属性设置界面，在“终端”→“键盘”选项中设置，如图 A-

6 所示。

图 A-6 XShell 键盘设置

当我们用 XShell 登录进入 Linux 后，在普通模式下对输入后的字符串进行退格删除操作没有问题。而在运行中，按

Delete 或 Backspace 键时会产生^H 等乱码问题。这个键盘设置会解决乱码的问题。

配置完成之后，再来看看 XShell 如何连接 CentOS 系统。在 VirtualBox 中启动完成 CentOS 系统之后，打开 XShell。

单击“文件”→“新建”菜单，或者单击工具栏的第一个带有“+”的图标，新建一个连接配置，如图 A-7 所示。

> [插图：手动分区界面中 SYSTEM 分组下的挂载点列表，/boot 1G、/ 71G（高亮选中）、swap 8G。]

## 4. MobaXterm 设置：鼠标（Mouse）配置项

| 配置项 | 取值 |
| --- | --- |
| 中间按钮(M) | 粘贴剪贴板内容。 |
| 向右按钮(R) | 打开弹出式菜单。 |
| 用[Ctrl + 鼠标左键单击]移动终端光标(L) | ☑ 已勾选 |
| 使用URL超链接(H) | ☐ 未勾选 |
| 　URL prefix | `http:// \| https:// \| ftp:// \| ssh:// \| telnet:// \|`（灰显） |
| 　[Ctrl + 单击]以打开超链接(K) | ☐ 未勾选（灰显） |

> [插图：MobaXterm 会话设置中的 Mouse 面板，含左右中键动作、Ctrl+左键移动光标开关，以及 URL 超链接开关与协议前缀列表。]

## 5. MobaXterm 设置：终端/外观设置面板

左侧设置树（当前展开部分）：

```text
├─ 代理
├─ 保持活动状态
├─ 终端
│  ├─ 键盘
│  ├─ VT 模式
│  └─ 高级
└─ 外观
   ├─ 窗口
   └─ 突出
```

右侧「键盘」面板内容：

| 项目 | 选项（单选，已选中项加粗标注） |
| --- | --- |
| DELETE 键序列 | ◉ **VT220 Del (Esc[3~)**<br>○ ASCII 127 (Ctrl+?)(A)<br>○ Backspace (Ctrl+H)(B) |
| BACKSPACE 键序列 | ○ VT220 Del (Esc[3~)<br>◉ **ASCII 127 (Ctrl+?)(I)**<br>○ Backspace (Ctrl+H)(K) |

元(Meta)键仿真：

> 终端作业时使用需要如Emacs等相关的Meta键的应用程序时,需要以下选项。

图 A-7 XShell 连接设置

在“连接”对话框中，填入之前配置的 CentOS 系统设置的 IP，然后单击“确定”关闭“新建会话属性”对话框。

第一次连接服务器时会有一个安全提示，选择“接受并保存”即可，随后输入账号 root 以及之前设置的密码。

Connecting to 192.168.0.120:22...

Connection established.


```bash
[root@localhost ~]#
```

To escape to local shell, press 'Ctrl+Alt+]'.

出现#提示符时，表示正常登录。至此，我们已经成功登录到了 CentOS 系统上。输入 cat/etc/redhat-release，我


```bash
[root@localhost ~]# cat /etc/redhat-release
CentOS Linux release 7.7.1908 (Core)
```

们可以查看到当前系统的内核版本为：

有些初次接触 Linux 系统的读者可能会有疑问，为什么不直接在 VirtualBox 弹出的窗口中登录，还要绕一圈到

XShell 中进行登录操作呢？当然，是可以直接登录的，但是原始窗口的操作非常不方便，而 XShell 这个终端工具比较

实用，本书我们使用的是虚拟机，服务器就在本地，而在实际情况下，我们使用的 Linux 系统可能在互联网上，所以最

好是习惯使用终端操作系统。

A.3.1 CentOS 命令说明

使用 XShell 进入系统之后，只有一个 cmd 提示符。除此之外，什么也没有了。你可能有些不习惯，因为它没有窗口

界面。并不是所有系统都需要有界面，CentOS 可以安装界面插件，但本书不讨论这个步骤，感兴趣的读者可以从网上

搜索此方面的内容。作为服务端使用时，并不需要界面。如果在 Windows 系统中有过 cmd（命令提示符）界面下工作的

经历，那么对于命令行应该一点也不陌生，常用命令见表 A-1。

表 A-1 Windows 系统常用命令表

> [插图：MobaXterm 设置对话框左树 + 右面板布局。删除键设为 VT220 Del（Esc[3~），退格键设为 ASCII 127（Ctrl+?），即编辑器常见的两种独立映射。]

## 6. MobaXterm：新建 SSH 会话配置对话框

左侧「类别(C)」树：

```text
├─ 连接
│  ├─ 用户身份验证
│  │  └─ 登录提示符
│  ├─ 登录脚本
│  ├─ SSH
│  │  ├─ 安全性
│  │  ├─ 隧道
│  │  └─ SFTP
│  ├─ TELNET
│  ├─ RLOGIN
│  ├─ SERIAL
│  ├─ 代理
│  └─ 保持活动状态
└─ 终端
```

右侧「连接 → 常规」表单：

| 字段 | 值 |
| --- | --- |
| 名称(N) | `192.168.0.120`（光标位于输入框末尾） |
| 协议(P) | `SSH` ▾ |
| 主机(H) | `192.168.0.120` |
| 端口号(O) | `22`（带微调箭头） |
| 说明(D) | （空，多行文本框） |

这些命令在 Linux 系统上大同小异，因为没有界面可供操作，所以需要通过大量的命令来达到目的。表 A-2 列出了

一些常用的命令，以方便读者查询。

表 A-2 Linux 系统常用命令表

这些命令全是小写的。值得注意的是，在 Linux 系统上是区分大小写的，也就是说如果我们新建一个 test 文件，还

可以再建一个 Test 文件，这种做法在 Windows 系统下是不可行的，但是 Linux 下完全可以。如果你有查看开源社区源代

码的习惯，会发现在服务端的代码，文件名绝大部分都是小写的，这绝对不是巧合。因为如果用大写的文件名，输入

或查找时还要按一下 Shift 键，而你一旦忘记大写的，系统还会告诉你找不到该文件。因此，在服务器编码时，我们绝


```bash
[root@localhost ~]# help
```

大部分情况下都是默认使用小写来命名的。更多的命令可以通过 help 命令查看。

当我们输入“help”命令时会列出常规的命令以及说明，这些 shell 命令是内部定义的。在 Linux 系统下，大部分


```bash
[root@localhost ~]# mkdir --help
[root@localhost network-scripts]# mkdir --help
```

命令都可以通过 help 来查看它的使用。例如 mkdir 命令：


```cpp
Create the DIRECTORY(ies), if they do not already exist.
```

Usage: mkdir [OPTION]... DIRECTORY...

Mandatory arguments to long options are mandatory for short options too.

-m, --mode=MODE   set file mode (as in chmod), not a=rwx - umask

-p, --parents     no error if existing, make parent directories as needed

……

在说明中指出了 mkdir 是用来创建文件夹的，它给出了 mkdir 的格式，还给出了选项参数的说明。初接触系统，我

们不需要了解太多命令，用到的时候可以在网页上搜索一下用法。随着使用的频度和时长增加，大部分命令都会慢慢

掌握。

A . 4 　 初 始 化 系 统

在开始使用之前，我们需要为这个新的系统做一些初始化与优化处理，同时为它安装一些必要的服务。CentOS 安

装软件比起 Windows 系统要方便得多，其中有一个安装服务名为 yum，其全称为 Yellow Dog Update Modified，作用是

对软件包进行管理，能够从指定的服务器下自动下载包并安装，如果安装的包有依赖包，那么还会自动安装依赖包。

如果你第一次接触到 Linux，不太明白也没有关系，本书所有第三方包安装时都给出了相应的步骤。

CentOS 自带的仓库对于国内的用户来说可能存在网速上的问题，我们选择阿里仓库镜像。本书源代码库中的 Linux


```bash
[root@localhost ~]# yum -y install lrzsz
```

目录下有一个名为 init_c7.sh 的脚本，将它上传到自己的服务器。先安装一个命令：

> [插图：MobaXterm 新建会话对话框，以主机 IP 作为会话名，协议 SSH，端口 22。]

## 7. 常用文件与目录操作命令表（第 1 部分）

| 命　令 | 说　明 |
| --- | --- |
| `dir` | 查看当前目录下的文件结构 |
| `cd xx` | 进入 xx 目录 |
| `cd ..` | 回到上层目录 |

## 8. 常用文件与目录操作命令表（第 2 部分）

| 命　令 | 说　明 |
| --- | --- |
| `ls` | 查看当前目录下的文件结构 |
| `cd xx` | 进入 xx 目录 |
| `cd ..` | 回到上层目录 |
| `rm -rf xx` | 删除 xx 目录 |
| `mkdir xx` | 在当前目录下新建一个名为 xx 的目录 |

## 9. yum 安装与更新命令表（第 1 部分）

| 命　令 | 说　明 |
| --- | --- |
| `yum install <keywords>` | 在系统中安装一个或多个软件包 |
| `yum update` | 更新当前系统所有的包 |

## 10. yum 查询命令表（续表）

| 命　令 | 说　明 |
| --- | --- |
| `yum list installed <keywords>` | 列出一个或一组软件包。例如：`yum list installed \| grep java` 列出了 Java 的安装包。`grep` 是一个关键字，它是一个文本搜索工具。`installed` 表示本地已安装。如果我们开发时要用到 Java，这时可以使用该命令，查看本地是否已经安装了 Java |
| `yum search <keywords>` | 在软件包详细信息中搜索指定字符串 |
| `yum info <keywords>` | 在软件包中查看某个具体的包的信息 |

## 11. vim 插入模式启动界面

```text
┌ 标签页：● 1 192.168.0.120  ×   |  +  |        ‹ ›  ▾ ┐
├──────────────────────────────────────────────────────┤
│  █                                                   │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
├──────────────────────────────────────────────────────┤
│ "test" 0L, 0C                          0,0-1    All │  ← 插入模式
└──────────────────────────────────────────────────────┘
```

> [插图：在 MobaXterm 标签 `1 192.168.0.120` 中用 vim 打开空文件 `test`，左下状态栏显示 `"test" 0L, 0C`，右下显示 `0,0-1` 与 `All`；编辑器内为 9 个 `~` 空行占位符。]

## 12. vim 插入模式状态栏

```text
┌ 标签页：● 1 192.168.0.120  ×   |  +  |        ‹ ›  ▾ ┐
├──────────────────────────────────────────────────────┤
│  █                                                   │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
│  ~                                                  │
├──────────────────────────────────────────────────────┤
│ -- INSERT --                            0,1    All │
└──────────────────────────────────────────────────────┘
```


```bash
[root@localhost ~]# ls
```

将 init_c7.sh 拖到 XShell 窗口上，就可以自动执行 rz 上传命令，上传完成之后，查看一下目录：

anaconda-ks.cfg init_c7.sh

输入“ls”查看目录情况，这时就会看到有一个新的文件 init_c7.sh。这时，这个 sh 还是不可执行状态，我们修


```bash
[root@localhost ~]# chmod a+x init_c7.sh
```

改它的执行权限：

有关 chmod 命令的详细情况，感兴趣的话可以上网查阅一些相关资料。执行 init_c7 这个文件，输入如下命令后按


```bash
[root@localhost ~]# ./init_c7.sh
```

回车键：

因为在脚本中有一个更新当前系统的操作，所以需要一定的完成时间。在这期间，我们看一下这个脚本的内容：

#!/bin/bash

#

#

# 下载阿里云的 yum 源，替换  /etc/yum.repos.d/CentOS-7.repo 文件

curl -so /etc/yum.repos.d/CentOS-7.repo HTTP://mirrors.aliyun.com/repo/CentOS-7.repo

# 安装需要用到的基础包

yum -y install bash-completion vim lrzsz chrony git yum-utils epel-release

# 下载阿里云的 epel 源

curl -o /etc/yum.repos.d/epel.repo HTTP://mirrors.aliyun.com/repo/epel-7.repo

# 更新整个系统

yum -y update

# 关闭不用的  seLinux

sed -ri 's/(SELinux=).*/\1disabled/g' /etc/seLinux/config

# 设置时区为亚洲 / 上海

\rm /etc/localtime

ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime

# 修改系统限制用户的打开文件数与创建进程数


```bash
# Default limit for number of user's processes to prevent
# accidental fork bombs.
# See rhbz #432903 for reasoning.
*          soft    nproc     unlimited
```

cat > /etc/security/limits.d/20-nproc.conf <<EOF

root       soft    nproc     unlimited

EOF


```cpp
*     -   nproc    65535
*     -   nofile    655350
```

cat >> /etc/security/limits.conf <<EOF

EOF


```cpp
systemctl list-unit-files --state enabled|awk '{print $1}'|xargs -i systemctl disable {}
```

# 关闭所有默认系统服务

# 开启用到的服务 ( 计划任务、系统日志、 ssh 远程登录、时间同步 )

systemctl enable crond rsyslog sshd chronyd network

这个脚本的前三行是关于 yum 的，当我们需要某个库时，可以直接在这些仓库（源）中查找，然后安装。因为 yum

默认的仓库地址位于境外，这里把它替换成了阿里云的地址，阿里有一个仓库的镜像，下载起来速度更快。可以访问

https://opsx.alibaba.com/mirror 查看这个源中的内容。

“yum-y update”这一行，是对当前系统中已经安装过的库进行更新。第一次执行这个脚本用的时间会比较久，

因为要更新系统与库。随后在这个脚本中，我们更换了一下时区，安装了一个 chrony 的库，以便同步更新时间，并关


```bash
[root@localhost ~]# systemctl list-unit-files --type service --state enabled
```

闭了一些不用的服务。脚本执行完成之后，可以查看初始化了哪些服务：

查看当前运行的服务，我们可以用命令 systemctl status 生成一棵进程树。

如果你初次接触 CentOS，可能会有些焦虑，如此多的命令怎么可能记得住。没必要一次性记住，需要使用时可以

在网络上搜索一下。更多的 Linux 命令可以一边使用一边学习，本书的重点在编码上，所以不需要过多担心。我们完成

了系统的初始化之后，一定要重启系统，让所有的效果生效。

特别说明，如果在升级系统时遇到有 Network 网络部分的更新，那么初始化时的 IP 有可能会失效。这时，需要手动


```bash
[root@localhost ~]# cd /etc/sysconfig/network-scripts/
[root@localhost network-scripts]# ls
```

重新修改 IP 地址。网络配置在“/etc/sysconfig/network-scripts/”目录下：

ifcfg-enp0s3 ……

配置文件在第一个文件 ifcfg-enp0s3 中，这个 enp0s3 名字可能会不一样，有时也可能是 eth0 等其他名字，但文件

内容是一致的。使用 vim 就可以修改 IP 了，如果你不会用 vim，翻到 A.5.2 节有一个简单的说明。

A . 5 　 C e n t O S 常 用 库 说 明

本节将介绍几个在开发过程中常用的基础库，分别是 yum、Vim 和 samba。

A.5.1 yum

在 Linux 系统上 yum 无疑是一个重要的命令。前面提到过 yum 可以为我们安装编码所需要的库，也可以安装一些工具

软件，例如 gcc、vim、tomcat 等。

常用 yum 命令见表 A-3。如果记不住 yum 的命令，也没有关系。在 Linux 下工作，我们要学习的第一件事情就是使用

help，可以使用 yum help。help 会列出我们记不住的所有命令，help 命令几乎可以覆盖所有 Linux 下的命令，即使是开

源的库也会实现“-help”“-h”或“--help”命令，这几乎是一条不成文的规则。我们可以通过 help 来看看 yum 的命

令列表。


```bash
[root@localhost ~]# yum help
```

表 A-3 yum 常用命令表

List of Commands:

check          检查  RPM 数据库问题

check-update   检查是否有可用的软件包更新

clean          删除缓存数据

……

因为太长了，这里就不逐一列举了。举一个简单例子来说明如何使用 yum 安装执行文件或库，假如我们要安装一个

名为 graphviz 的工具，可以采用以下几步：


```bash
[root@localhost ~]# yum search graphviz
```

• （1）用 search 命令查看是否有该工具：

……

graphviz-devel.i686 : Development package for graphviz

> [插图：同一 vim 会话中按 `i` 键后进入插入模式，左下状态栏由 `"test" 0L, 0C` 变为 `-- INSERT --`，光标行列由 `0,0-1` 变为 `0,1`，`All` 保持不变（当前不做搜索高亮）。]

……

graphviz-python.x86_64 : Python extension for graphviz

……

graphviz.i686 : Graph Visualization Tools

……

search 命令列出了所有关于 graphviz 的库或执行文件，graphviz-devel 是一个开发包，graphviz.i686 是可执行文

件。当我们只需要工具，不需要开发包时，可以直接下载可执行文件。


```bash
[root@localhost ~]# yum install graphviz
```

• （2）使用 install 命令安装：

执行 install 命令后会提示是否安装，输入“y”，直至安装完成。


```bash
[root@localhost ~]# find /usr/ -name graphviz
```

• （3）使用 find 命令查看安装路径：

/usr/lib64/graphviz

/usr/share/graphviz

当我们安装完成，想要看看它到底安装在哪里，有哪些安装文件时，可以使用 find 命令，格式为“find path 选项

参数”。在上例中，我们使用了-name 选项，即为名字查找。更多的选项可以用“find-help”查看。至此，我们已经

了解了在 CentOS 下 yum 如何安装需要的库了。需要说明的是，并不是所有的 Linux 系统都有 yum，在 Ubuntu 系统下是采用

的是 apt-get 命令。

A.5.2 Vim

除了 yum 之外，另一个需要有一定了解的是 Vim。Vim 是一个著名的编辑器。在初始化系统时已经安装好了 Vim。我


```bash
[root@localhost ~]# vim test
```

们以一个例子来简单介绍 Vim 的使用。

在命令行输入“vim test”，看到的界面如图 A-8 所示，表示在当前目录打开一个名为 test 的文件，如果没有该文

件会在内存中建立一个缓存文件。

图 A-8 Vim 空白文档

这是一个空文件。在当前这个状态下，我们称为命令行模式，在该模式下是没有办法输入文字的，所以命令行就

是等待你下达命令。按下 i 键，这时界面会发生变化，如图 A-9 所示。

图 A-9 Vim 插入状态

注意最下方状态栏的显示信息，这时代表可以在文件中插入数据。在插入数据模式下，上下键可以移动光标，

PageUp 和 PageDown 两个键可以翻页。输入完成之后，按下 Esc 键退出 INSERT 模式，回到命令行模式。

输入“：wq”，如图 A-10 所示，在命令行的最下方就会出现输入的命令。wq=quit and write 即退出并写入，直接

按回车键回到目录界面。使用“ls”命令查看，我们会发现多了一个 test 文件，它没有后缀，因为我们没有给它加后

缀，不过没有关系。再次用“vim test”打开，会看到与退出时一样的内容。

图 A-10 Vim 保存退出

值得一提的是，在 Linux 系统下不是靠文件后缀来确定文件类型的，这一点与 Windows 系统非常不同。在 Windows 系

统下，后缀.exe 表示是一个可执行文件，但在 Linux 系统下，即使没有后缀，它依然是一个可执行文件，但在 Windows

下如果修改了.exe 后缀，它就不能再执行了。表 A-4 列出了几个常用的命令。

表 A-4 Vim 常用命令表

Vim 除了命令之外，还有很多快捷操作方式，其用法多样，需要一段时间的学习。本节简单地阐述了 Vim，因为后

面需要对 Linux 的一些文件做出修改，希望这种修改步骤不会成为你执行本书例子时的障碍，所以这里简短说明了 Vim

---

## 图表转写 · 批次 12（PDF 页 507–518）

> 这一段是提取时按 PDF 页序插进来的图表转录，**与本章正文交错**，
> 其中 `### pXXX_0.jpg` 是配图的文字转录，图页裁图在 `_raw/B_img/`。

> 本批次 6 张图在文字层均无图题（PDF 未给出编号），序号由转写方按 PDF 页码顺序自拟（批次内 1–6）。
> 内容全部依据 `B_img/` 中对应位图逐张判读；5 张为命令/选项对照表（含 1 张 vim 命令表），1 张为 vim 终端截图。

## 1. vim 中输入 `:wq` 保存并退出

```text
┌ 标签页：● 1 192.168.0.120  ×   |  +  |              ‹ ›  ▾ ┐
├──────────────────────────────────────────────────────────┤
│ test                                                      │
│ test                                                      │
│ test                                                      │
│ ~                                                         │
│ ~                                                         │
│ ~                                                         │
│ ~                                                         │
│ :wq█                                                      │
└──────────────────────────────────────────────────────────┘
```

> [插图：MobaXterm 标签 `1 192.168.0.120` 中的 vim 编辑窗口。文件中已输入 3 行 `test` 内容，下方为 4 个 `~` 空行占位符，最后一行输入命令 `:wq`（光标紧跟其后），用于保存并退出。截图被裁剪，未包含底部状态行。]

## 2. vim 基本命令表

| 命　令 | 说　明 |
| --- | --- |
| `Esc` 键 | 退出插入模式，切换到命令模式 |
| `i` 键 | 进入插入模式 |
| `:q` | 如果无修改，就直接退出 |
| `:w` | 保存文件 |
| `:wq` | 保存，同时退出 |
| `:q!` | 修改过文件，又不想储存，使用!符号强制退出 |

的使用。如果你曾经用过 Vim，当然更好，如果没有用过 Vim，可以适当查看一些资料加以了解，慢慢你就会入门，并

且喜爱这种命令行模式的操作，这是一种完全可以脱离鼠标的操作方式，可以提高操作效率。

如果你还不是很了解 Vim，也不需要过分担心，本书所有使用 Vim 进行编辑的地方都会有步骤提示。在 Linux 上，除

了 Vim 编辑器之外，用于编码的编辑器可以使用 Emacs，Emacs 上手比较慢，因为它基本靠快捷键操作，熟练使用会加快

编码速度。

A.5.3 Samba

为了方便查看 CentOS 系统上的文件，我们可以安装一个名为 Samba 的工具软件。Samba 的作用是共享目录，它只能

使用在局域网中，方便 Windows 系统与 Linux 系统交互。直观的功能就是可以在 Windows 系统上查看文件目录，实现文件


```bash
[root@localhost ~]# yum -y install samba
```

共享。我们还是使用 yum 进行安装，其命令如下：

安装完成之后，我们需要对 Samba 进行配置。在配置之前，需要有一个 Samba 账号，可以使用 pdbedit 命令来创建


```bash
[root@localhost ~]# pdbedit -a root
```

Samba 账号，在创建账号时，需要注意 Samba 账号必须先有系统用户的账号。下面将系统的账号 root 映射到 Samba 中：

new password:

retype new password:

Unix username:        root

……


```bash
[root@localhost ~]# systemctl start smb
```

提示输入密码之后，账号就创建成功了。现在，启动 Samba，使用如下命令：

在 Windows 系统的窗口中，在文件资源管理器的地址栏中输入“\\192.168.0.120”，访问 192.168.0.120 就可以看

到 root 目录。进入 root 目录就可以看到之前上传的 init_c7.sh 文件。


```bash
[root@localhost ~]# systemctl enable smb
```

执行如下命令，开机自动启动 Samba：

Created symlink from /etc/systemd/system/multi-user.target.wants/smb.service to /usr/lib/systemd/system/s

现在可以方便地在 Windows 系统下操作 CentOS 系统的文件了。

更多的 Samba 的配置可以在网上查询一下，其配置文件位于“/etc/samba/smb.conf”。

A.5.4 Git

Git 是一个开源的分布式版本控制工具，本小节详细说明在 Linux 上获取源代码的步骤。如果你已经有相关经验，

可跳过此处所讲的内容。


```bash
[root@localhost ~]# yum install -y git
```

• （1）安装 Git，如果已发装，就跳过安装。


```bash
[root@localhost ~]# git clone git 地址  ./game
```

• （2）获取源代码库：

Git 的 clone 命令是将一个指定的 Git 库获取到本地，我们指定获取目录为“./game”，即当前目录的 Game 子目录

下。

• （3）编译。在 CentOS 下，每个工程中都有 make-all.sh 文件，用于批量执行代码编译命令。如在执行过程中遇

到“./make-all.sh：行 2：$'\r'：未找到命令”这样的错误，是因为两个系统下按回车键不同造成的，请使

用“dos2unix make-all.sh”修正问题。

• （4）提交代码步骤。Git 代码提交步骤和 SVN 有些不一样，在 SVN 中，如果执行 commit，这些 commit 会立即上传到

源代码服务器上。Git 在本地有一个源代码仓库，可以理解为它先在本地克隆了一个一模一样的仓库。执行 commit 时，

修改代码被提交到了本地仓库。如果想要更新源代码仓库，那么还需要使用 push 命令。


```bash
[root@localhost game]# git config --global user.email "xxx@xxx.com"
[root@localhost game]# git config --global user.name "xxx"
```

首次进行 commit 操作，需要先进行全局配置：

该配置将在提交代码时用于显示提交者的信息，只需配置一次。


```bash
[root@localhost game]# git config --global push.default simple
[root@localhost game]# git config --global credential.helper store
[root@localhost game]# git push origin master
```

首次 push 操作也需要进行全局配置：

在执行以上命令时可能会需要输入账号名与密码。“git push origin master”这条命令是将远端的 master 主分

支与本地的 origin 分支关联起来，方便后面“git push”的使用。如果只是浏览仓库代码，那么可以跳过这一步

骤。“credential.helper”会将我们输入的 Git 凭证保存下来，不用每次 push 到远端的时候都输入。在执行 commit 提


```bash
[root@localhost game]# git commit --status
```

交之前，可以先查看当前提交的状态：

# 位于分支  master

# 尚未暂存以备提交的变更

#   （使用  "git add <file>..." 更新要提交的内容）

#   （使用  "git checkout -- <file>..." 丢弃工作区的改动）

该命令会列出当前被修改的所有文件，并做出操作提示，如果有需要提交的文件，使用“git add”命令可以将它


```bash
[root@localhost game]# git commit -m "modify"
```

加入待提交列表中。“git checkout”命令重新从库中取出文件覆盖当前文件，表示修改被丢弃，最后提交。

参数“-m”表示 message，提交说明。commit 完成之后，你修改的代码已经进入本地 origin 分支中了。这时，它还


```bash
[root@localhost game]# git push
```

没有进入远端的 master 分支，使用 push 命令将数据推送到远端：

并不是每一次 commit 之后都需要 push 操作，如果你做一个较大的版本改动需要花费三五天时间，可以每天 commit

到本地仓库中，最终完成时再进行 push 操作。

• （5）更多命令。这里在表 A-5 中列举 Git 的常用命令，以便随时查阅。

表 A-5 Git 常用命令

Git 的命令相当多，一时记不住也没有关系，在本书的例子中使用到它时会逐一进行提示。更多的命令说明可以使


```bash
[root@localhost game]# git --help
```

用 help 命令查看。


```bash
[root@localhost game]# git commit -help
```

如果我们需要看一个子命令的格式，以 commit 为例，可以使用以下命令：

A . 6 　 C e n t O S 编 译 工 具

要在 CentOS 上搭建一个编译环境，除了之前的一些基础库之外，还需要安装专业的库文件。

A.6.1 GCC

开始进行 Linux 编码之前，我们需要安装一个编译工具 GCC。GCC 是目前被大多数 Linux 操作系统认可的标准编译

器，这个工具就相当于 Windows 端的 Visual Studio 工具，不同之处在于 GCC 没有界面，它是一种靠命令完成编译的工

具。Visual Studio 工具本质上最终也是靠一系列的命令完成编译，只是它将命令部分隐藏，而配置部分是由界面来实

现的。

> **关键点 1：下载GCC**


```bash
[root@localhost ~]# yum info gcc
```

查看 yum 库中自带的 GCC 版本，还不到 5.0：

可安装的软件包：

名称： gcc

架构： x86_64

版本： 4.8.5

……

本书中使用的是 C++14 标准，当前 yum 仓库中的 GCC 不适合我们使用，所以手动安装 GCC。要下载 GCC 版本，先安装一

个下载工具 wget。wget 是 Linux 上的一个下载工具，支持断点续传。我们需要用它来下载 GCC 的安装包。

> [插图：vim 常用命令对照表，涵盖模式切换与保存退出的 6 条命令。]

## 3. git 常用命令表

| 命　令 | 说　明 |
| --- | --- |
| `git pull` | 从远程仓将新的数据拉到本地仓库，类似于 SVN 的升级操作 |
| `git clone` 地址 目录 | 命令 clone 就和它的意思一样，克隆一个源代码库到本地目录下 |
| `git clean -f` | 删除当前目录下所有没有进入库管理的文件。如果有些本地文件不在库中，但又不想删除它，可以修改`.gitignore`文件，将该文件加入 |
| `git clean -n` | 使用 clean 之后，可能会误删除一些有用的文件，可以使用 clean -n 预览一下将要删除的文件 |
| `git status` | 查看当前文件的变更操作 |
| `git checkout` | 检出一个分支或一个路径到当前工作仓库中。如果我们在使用中不小心误除了文件、目录或分支，可以使用该命令重新获取 |
| `git diff` | 在提交 commit 之前还可以使用 diff 命令查看一下到底修改了哪些内容 |
| `git add` | 将修改过的文件添加到待 commit 列表中。`git add -A` 可批量添加 |


```bash
[root@localhost ~]# yum install -y wget
```

wget 安装完成之后，我们需要从 GCC 官方下载安装包。打开 HTTP：//ftp.gnu.org/gnu/gcc/这个网页，可以看到当


```bash
[root@localhost ~]# wget -c HTTP://ftp.gnu.org/gnu/gcc/gcc-9.1.0/gcc-9.1.0. tar.gz
```

前官方的所有版本，选择一个比较新的版本 gcc-9.1.0，使用 wget 开始下载。

若输入命令时出现：“正在解析主机 ftp.gnu.org（ftp.gnu.org）...失败：未知的名称或服务”类似的信息，则

可能是之前配置的 DNS 无法连接，使用 vim 工具修改/etc/resolv.conf 文件，将 223.5.5.5 和 223.6.6.6 替换为 8.8.8.8 和


```bash
[root@localhost ~]# ls
```

8.8.4.4 这组 DNS 服务器。下载完成之后，使用“ls”命令查看，多了一个 gcc-9.1.0.tar.gz 文件。

anaconda-ks.cfg  gcc-9.1.0.tar.gz  init_c7.sh

gcc-9.1.0.tar.gz 是一个压缩文件，在 Windows 系统下，我们习惯使用“.rar”或“.zip”后缀的压缩文件，tar

是 Linux 系统上常用的压缩工具。

> **关键点 2：依赖库**

下载好 GCC 之后，还需要安装两个依赖工具。

• （1）低版本 GCC

下载完成之后，从 yum 中安装一个低版本 GCC 来进行编译。因为我们下载的 GCC9 的版本也是源代码，源代码是需要


```bash
[root@localhost build]# yum install -y gcc gcc-C++
[root@localhost build]# gcc -v
```

编译的，所以下载低版本的 GCC 来编译它。

……

GCC 版本  4.8.5 20150623 (Red Hat 4.8.5-36) (GCC)

GCC 提供了标准 C 的编译，GCC-C++提供了 C++标准的编译。安装完成之后，我们看看 GCC 的版本。

• （2）bzip 解压


```bash
[root@localhost ~]# yum -y install bzip2
```

在编译 GCC 之前，我们还需要安装 bzip 工具。

> **关键点 3：配置编译**


```bash
[root@localhost ~]# tar xf gcc-9.1.0.tar.gz
[root@localhost ~]# ls
```

一切都准备好之后，开始对下载的版本进行编译。分成以下几个步骤：

anaconda-ks.cfg  gcc-9.1.0  gcc-9.1.0.tar.gz  init_c7.sh


```bash
[root@localhost ~]# cd gcc-9.1.0/
[root@localhost gcc-9.1.0]# ./contrib/download_prerequisites
```

解压之后，当前目录多了一个 gcc-9.1.0 子目录。进入子目录后，下载 gcc 需要的依赖包：

下载依赖包的时间是由本地的网络决定的，GCC 的下载和编译的时间相当长，需要耐心等待。执行完成之后会显示

All prerequisites downloaded successfully。接下来，我们在 GCC 目录下新建一个名为 build 的子目录，进入该目录


```bash
[root@localhost build]# mkdir build; cd build
[root@localhost build]# ../configure --prefix=/usr/local/gcc-9.1.0 --enable-languages=c,C++ --disable-mul
```

中，执行 configure 配置。

我们将安装目录定为“--prefix=/usr/local/gcc-9.1.0”。“--disable-multilib”是指定我们仅编译当前的 64

位版本，不产生多版本，这样可以节约编译时间。

这一步执行完成之后，Makefile 文件已经被创建出来了。在 Linux 系统下，我们要对 C++文件进行编译，重要的配


```bash
[root@localhost build]# make && make install
```

置文件就是 Makefile。后续会进一步介绍如何生成 Makefile，现在只需要编译执行它就可以了。输入以下命令：


```bash
[root@localhost ~]# /usr/local/gcc-9.1.0/bin/gcc -v
```

gcc 编译的时长相当长，大约需要 30 分钟到 1 小时不等，或许更长。完成之后，查看一下版本号：

显示版本号为 9.1.0（GCC），即安装成功。

> **关键点 4：环境变量**

现在我们的系统中安装了两个 GCC。使用默认的 gcc 命令，得到的版本依然是低版本。但是我们并不想使用这个低


```bash
[root@localhost ~]# vim ~/.bash_profile
```

版本的 GCC，所以还需要在用户环境中配置高版本的 GCC 的路径。


```cpp
PATH=/usr/local/gcc-9.1.0/bin/:$PATH:$HOME/bin
export PATH
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/gcc-9.1.0/lib64/
export LD_LIBRARY_PATH
```

打开文件之后，按 i 键，进入插入模式，修改文件如下：

文件“.bash_profile”修改了当前账户的环境配置，我们将 GCC-9.1.0 的 bin 目录加入到 path 路径中。在 Windows

系统中增加环境变量时，用的分隔符为;（分号），在 Linux 下，用的分隔符为：（冒号）。PATH 是执行路径的环境变

量名，LD_LIBRARY_PATH 是动态库的环境变量名，当有程序需要动态库时，就会在 LD_LIBRARY_PATH 指定的路径中查

找。我们在这个变量中增加了 GCC-9.1.0 的新的库目录。而当我们执行某个程序时，则在 PATH 这个目录下查找。配置完


```bash
[root@localhost ~]# gcc -v
```

成之后，退出并重新进入 XShell 终端，查看版本：

现在用的 GCC 已经变成了新安装的 GCC 高版本了。

A.6.2 CMake3

除了基本的编译器 GCC 之外，在 Linux 下编译 C++文件还涉及 Makefile 文件。Makefile 文件的作用就是把源码编译成

中间码，再编译为可执行程序。Windows 下使用的 Visual Studio 也是同样的编译步骤，Visual Studio 把中间过程工具

化了，写代码的人不清楚这个中间过程。

Linux 下没有类似 Visual Studio 这种可视化工具，所以需要自己写 Makefile 文件。Makefile 文件对于初学者来说


```bash
[root@localhost ~]# yum install -y cmake3
[root@localhost ~]# cmake3 -version
```

比较烧脑，所以用工具 CMake 来自动生成 Makefile。

cmake3 version 3.13.5

执行如上命令，安装 CMake3。下面我们创建一个简单的任务，查看在 Linux 下，CMake3 和 GCC 是如何进行代码编译


```bash
#include <cstdio>
int main( int argc, char *argv[] ) {
    printf("Hello, world!\n");
```

的。打开本书源代码库中的 00_01_gcc 目录，该目录中有 CMakeLists.txt 和 main.cpp 两个文件。main.cpp 的代码如下：


```cpp
    return 0;
}
```

代码非常简单，打印一句“Hello world！”。使用 CMake 编译源代码需要如下步骤：


```bash
[root@localhost ~]# cd ./game/00_01_gcc/
[root@localhost 00_01_gcc]# ls
```

• （1）进入源代码目录：

CMakeLists.txt  main.cpp

查看源代码目录，可以看到有两个文件：一个是用于 CMake 的 CMakeLists.txt;另一个是源代码文件。


```bash
[root@localhost 00_01_gcc]# cmake3 ./
```

• （2）生成 Makefile 文件：

-- The C compiler identification is GNU 4.8.5

-- The CXX compiler identification is GNU 4.8.5

……


```bash
[root@localhost 00_01_gcc]# vim ~/.bash_profile
```

执行 cmake3 时，提示了 CXX 的编译器为系统默认的 4.8 版本，要指定安装高版本的 GCC，需要进行指定。


```cpp
export CC=/usr/local/gcc-9.1.0/bin/gcc
export CXX=/usr/local/gcc-9.1.0/bin/g++
```

按 i 键，进入插入模式，增加两行代码：

编辑完成之后，按 Esc 键，退出编辑模式，输入“：wq”保存退出。注意是半角符号。关闭 XShell 连接，再次打


```bash
[root@localhost 00_01_gcc]# cmake3 ./
```

开。使用“rm-f Makefile”命令删除已生成的 Makefile 文件。再次执行 cmake3 命令：

-- The C compiler identification is GNU 9.1.0

-- The CXX compiler identification is GNU 9.1.0


```bash
[root@localhost 00_01_gcc]# ls
```

……

CMakeCache.txt  CMakeFiles  cmake_install.cmake  CMakeLists.txt  main.cpp  Makefile

完成之后，再次使用 ls 命令浏览一下目录，会发现多了一些临时文件，其中就有 GCC 需要的

Makefile。“cmake3./”命令指定了要 CMake 的目录是当前目录，如果需要指定的目录位于上层目录，则使

用“cmake3../xxx”。

在这些生成的中间文件中，其中重要的就是 Makefile，Makefile 是 GCC 编译的基础，Makefile 文件可以手动写出

来，它定义了一系列的规则来指定哪些文件需要编译、编译的前后顺序等，在网上可以搜索到很多教你写 Makefile 文

件的教程。如果对此感兴趣，可以查看资料，这里不再详述。总之，我们使用 CMake 生成了 Makefile。


```bash
[root@localhost 00_01_gcc]# make
```

• （3）使用 make 命令生成执行文件：

Scanning dependencies of target test

[ 50%] Building CXX object CMakeFiles/test.dir/main.cpp.o

[100%] Linking CXX executable test

[100%] Built target test

生成可执行文件需要用到 make 命令，使用 make 命令会去查找当前目录中的 Makefile 文件，并执行 Makefile 中的每

一条命令，最终生成目标文件。执行完成之后，多了一个名为 test 的文件，它没有后缀。前面已经讲过，在 Linux 系统

下不是依照文件后缀来确定文件类型的。现在来执行一下看看效果。

• （4）运行：


```bash
[root@localhost 00_01_gcc]# ./test
```

Hello, world!

运行结果打印了“Hello，world！”。假设我们在 CMakeLists.txt 中把最后一行修改成了

add_executable（test.exe${SRCS}），使用“rm-f CMakeCache.txt”删除 CMake 的中间文件，CMake 将会重新执行，

再次执行 make 将会重新生成一个 test.exe 文件。或者将它修改为“add_executable（test.test${SRCS}）”，则会生


```bash
[root@localhost 01_testgcc]# ./test.test
```

成一个带.test 后缀的文件，它依然是一个可执行文件。

Hello, world!

以上步骤完成了一个项目的配置与编译，打开 CMakeLists.txt，我们简单分析一下这个文件每一行的作用，如表

A-6 所示。

表 A-6 CMakeLists 常用命令表

在 CMakeList.txt 文件中，我们使用到诸如 aux_source_directory、link_directories、set 这样的命令，这些命

令都是 CMake 提供的，通过使用 CMake，我们可以方便地在 Linux 中编译出想要的可执行文件。CMake 中有一系列的变

量，都可以通过 set 来赋值。简单说明一下 CMake 中可能使用到的其他命令：

• （1）CMAKE_RUNTIME_OUTPUT_DIRECTORY：输出可执行文件的目录。

类似这样的输出目录在 CMAKE 中有 3 个，分别是：

·CMAKE_RUNTIME_OUTPUT_DIRECTORY，表示输出可执行文件的目录。

> [插图：git 常用命令对照表，共 8 条，覆盖拉取、克隆、清理、状态查看、检出、差异对比与暂存。]

## 4. CMakeLists.txt 指令表

| 命　令 | 说　明 |
| --- | --- |
| `cmake_minimum_required(VERSION 2.8)` | 因为每个 CMake 版本不同，可以提供的功能也不同，这里限制了 CMake 的最低版本不能低于 2.8 版本 |
| `project(test)` | 指定工程名字 |
| `aux_source_directory(. SRCS)` | 将当前目录"./"保存在 SRCS 变量中，作为源码目录。如果有多个源码目录，可以多次声明。这个过程与在 VS 界面上把文件拖到工程中是一样的 |
| `include_directories(/usr/include)` | 指定 include 目录。因为本例很简单，没有我们自己的 include 目录，所地这里使用了系统的 include 目录 |
| `set(CMAKE_CXX_FLAGS "-Wall -std=c++14")` | 基础编译选项，这里使用了 C++14 标准。C++11 与 C++14、C++17 都有不同的语法 |
| `set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -O0 -g -ggdb")` | 在 DEBUG 模式下的编译选项。这里增加了 gdb，在 DEBUG 模式下可调试代码。<br>-O0 不要优化代码，方便调试。注意第一个是字母 O，第二个才是 Zero(0) |
| `set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")` | 在 RELEASE 模式下的编译选项，去掉了多余的 DEBUG 信息。其中 "-O3" 是最高程度优化代码 |
| `link_directories(/usr/local/lib)` | 指定 link 的文件目录，即在这个目录下寻找库文件，如果有多个库文件目录，就可以多次声明。本例很简单，只指定了系统的库目录 |
| `add_executable(test ${SRCS})` | 生成一个名为 test 的执行文件，指定源代码目录变量在 SRCS 中 |

·CMAKE_LIBRARY_OUTPUT_DIRECTORY，表示输出动态库目录。

·CMAKE_ARCHIVE_OUTPUT_DIRECTORY，表示输出静态库目录。

例如，set（CMAKE_ARCHIVE_OUTPUT_DIRECTORY"../libs"）表示要把静态文件放到上一层目录的 libs 目录中。当

然，也可以不指定输出目录，这时输出的文件在当前目录下。

• （2）CMAKE_BUILD_TYPE：编译类型，常规的值是 Debug、Release。

例如 set（CMAKE_BUILD_TYPE Debug）表示要编译的是一个 Debug 版本。

• （3）CMAKE_CXX_FLAGS：因为我们编的是 C++，C++的编译选项名在 CMake 中为 CMAKE_CXX_FLAGS。编译参数这个值

在 CMake 中的模板为 CMAKE_<LANG>_FLAGS，如果这里是纯 C 语言，就是 CMAKE_C_FLAGS。这个参数用于指定一些编译参

数。

除此之外，还有一些扩展的属性，例如 CMAKE_CXX_FLAGS_DEBUG，表示 Debug 版本下的参数;

CMAKE_CXX_FLAGS_RELEASE 则表示 Release 版本下的参数。使用 Debug 还是使用 Release 是由指定的 CMAKE_BUILD_TYPE 来

决定的。

因为我们使用的是 C++，所以特别说明一下 C++的编译选项，即 CMAKE_CXX_FLAGS 中的值。

不论我们使用 CMake 还是使用 Makefile，在 GNU 编译器集合（GNU Compiler Collection，GCC）中都有两个环境变

量，分别是 CFLAGS 与 CXXFLAGS。CFLAGS 用于 C 代码，CXXFLAGS 用于 C++代码。在使用 CMake 时，环境变量被缓存到

CMAKE_CXX_FLAGS/CMAKE_C_FLAGS 中，如果我们对 CMAKE_CXX_FLAGS/CMAKE_C_FLAGS 做了定义，那么环境变量就会被忽

视。在上面的例子中，我们没有使用环境变量，而直接对 CMAKE_CXX_FLAGS 进行赋值，即这一句：

set（CMAKE_CXX_FLAGS"-Wall-std=C++11"）;如果要使用环境变量，可以这样：

set（CMAKE_CXX_FLAGS"$ENV{CXXFLAGS}-Wall-std=C++11"），$ENV{CXXFLAGS}取得环境变量。更多的选项见表 A-7。

表 A-7 编译选项表

关于 CMake 更多的资料可以在 https://cmake.org/documentation/上查看。如果是第一次接触 CMake，这一章的信

息量可能过大，我们再总结一下一个基本的 CMakeLists.txt 需要做些什么：

• （1）指定源码目录，可以是一个或多个目录。

• （2）指定 include 目录，可以是一个或多个目录。

• （3）指定 lib 库目录，可以是一个或多个目录。

• （4）指定输出目录，不同类型输出目录不相同，也可以不指定。

• （5）指定编译类型。

• （6）指定编译选项、预处理值等。

• （7）执行生成命令，生成可执行文件、静态库或动态库。

> [插图：一份完整的最小 CMakeLists.txt 逐行释义表，涵盖最低版本声明、工程命名、源码目录收集、头文件与库目录搜索路径、DEBUG/RELEASE 两套编译选项以及可执行目标生成。]

## 5. gcc 常用编译选项说明表

| 编译选项 | 说　明 |
| --- | --- |
| `-Wall` | 在编译时加上可以让编译器对你的代码提出尽可能多的警告 |
| `-std=C++14` | 以 C++14 的标准来编译源代码。C++14 与 C++11 或更早的 C++ 标准语法不同 |
| `-O0`、`-O3` | 优化程度调到最高为-O3，最低为-O0，即不做优化。注意，-O 是 OPT 的 O，而不是 0，-O0 第二个字符才是 0 |
| `-DNDEBUG` | 在 Release 时使用，表示不进行调试编译，生成的可执行文件没有调试信息，可以加快运行速度 |
| `-g -ggdb` | 生成 gdb 需要的调试信息 |
| `-D_DEBUG` | 这是自定义的一个预编译宏。在 Windows 系统的 Debug 模式下定义了_DEBUG 宏，以完成一些 DEBUG 的调试功能。为了统一性，我们在 Linux 下也定义一个相同的宏，一般来说在自定义宏前加-D，GCC 就可以识别这个宏 |

• （8）库连接。

细心的读者会发现，其实 CMake 和我们在 Visual Studio 下做的事情相差无几，Visual Studio 是一个可视化的界

面，而 CMake 是命令行。

本节用一个简单的例子来测试安装的 GCC 和 CMake 是否可以正常工作，并对 CMake 有了一个大概的了解。如果还不是

太明白，也无须担心，本书的所有源代码已经写好了 CMakeList.txt 文件，结合本节所给出的知识点完全可以看懂每一

个 CMakeList.txt 文件。在有特殊用法时，使用一些其他的配置也会根据上下文加以说明。

A.6.3 gdb

现在可以用 CMake 生成可执行文件了。有一个新的问题，没有 Visual Studio 的可视界面，我们怎么在 Linux 下调试

代码呢？有一些纯服务端开发的程序，只在 Linux 上运行，它有很多 Linux 的特性，根本无法使用界面工具进行调试。

即使源代码兼容 Windows 或 Linux，因为其运行环境的不同、编辑器的不同，即使是相同的代码，有时也会有一点细微

的差别，所以对于 Linux 系统下的调试是作为服务端程序的一个必备条件。如果还不熟悉，本小节准备了一些简单的例

子供初学者翻阅;如果你已经熟悉 gdb 操作，可以跳过本小节。

> **关键点 1：安装gdb**


```bash
[root@localhost ~]# yum install -y gdb
```

在 Linux 下使用 gdb 来进行调试，gdb 是 Linux 上的标配调试器。可以使用命令进行安装：

在调试代码时一般会定位到某个文件的某一行，但是由于 Windows 和 Linux 的换行符不一致，因此会造成行偏差。


```bash
[root@localhost ~]# yum install -y dos2unix
```

我们安装一个小工具 doc2unit。

如果在调试时发现某个文件（例如 network.cpp）的换行符不对，可以执行如下命令，将换行符转换为 Linux 下的


```bash
[root@localhost network]# dos2unix network.cpp
```

换行符。

> **关键点 2：调试代码步骤**

我们使用本书源代码 00_01_gcc 目录中的例子来说明如何用 gdb 调试代码。在执行文件前加上 gdb 的关键字，就可以


```bash
[root@localhost 00_01_gcc]# gdb test
GNU gdb (GDB) Red Hat Enterprise Linux 7.6.1-114.el7
```

开始一个可执行文件的调试：

……


```cpp
(gdb)
```

Reading symbols from /root/game/00_01_gcc/test...(no debugging symbols found)...done.

注意第一个提示（no debugging symbols found），原因是 test 中没有 Debug 调试信息，这是什么原因？我们生成

test 时没有指定 Debug 模式。gdb 必须在 Debug 模式下才能良好运行。使用 vim 编辑 CMakeLists.txt 文件，命令为：vim

CMakeLists.txt，在第三行处增加设置：set（CMAKE_BUILD_TYPE Debug），表示以 Debug 来 build 工程。删除


```bash
[root@localhost 00_01_gcc]# rm -rf CMakeCache.txt
[root@localhost 00_01_gcc]# cmake3 ./
[root@localhost 00_01_gcc]# make
```

CMakeCache.txt 文件，重新编译。

在（gdb）提示符之后输入“r”，出现了“auto-load safe-path”警告，每次 gdb 启动时都会执


```bash
[root@localhost 00_01_gcc]# vim /root/.gdbinit
```

行“.gdbinit”文件，我们创建一个放到 root 用户的根目录下。

认真看一下提示，.gdbinit 的文件位置在不同的机器上可能不同。在“.gdbinit”文件中加入一行“add-auto-

load-safe-path/usr/local/gcc-9.1.0/lib64/libstdC++.so.6.0.26-gdb.py”，保存退出。

再次使用“gdb test”命令，前面列出的是 gdb 相关的信息，在提示符（gdb）之后输入“r”命令，意味着让它继

续运行 Run。

(gdb) r

Starting program: /root/game/00_01_gcc/test

Hello, world!

[Inferior 1 (process 2113) exited normally]

Missing separate debuginfos, use: debuginfo-install glibc-2.17-260.el7_6.6.x86_64

因为没有断点，所以程序直接运行，输出了“Hello，world！”，随后有一个 debufinfos 缺失的提示。我们按照

以下步骤修改 debuginfo 缺失问题：


```bash
[root@localhost 00_01_gcc]# vim /etc/yum.repos.d/CentOS-Debuginfo.repo
```

步骤 01　使用 Vim 修改“/etc/yum.repos.d/CentOS-Debuginfo.repo”文件使 enable=1。完整命令如下：

步骤 02　执行“yum install yum-utils”。在初始化 CentOS 的脚本中，已经安装过这个包，如果你没有执行过初


```bash
[root@localhost 00_01_gcc]# yum install yum-utils
```

始化脚本，就安装一次，如果本地已安装，执行时就会跳过。完整命令如下：

步骤 03　执行上面提示中 use：后面的部分“debuginfo-install glibc-2.17-260.el7_6.6.x86_64”。完整命令


```bash
[root@localhost 00_01_gcc]# debuginfo-install glibc-2.17-260.el7_6.6.x86_64
```

如下：

注意，这个命令不是绝对的，根据不同的版本，提示信息不一样，需要根据自己机器的提示信息来执行命令。安


```bash
[root@localhost 00_01_gcc]# gdb test
```

装完成之后，再次执行 gdb 调试：

……

Reading symbols from /root/game/00_01_gcc/test...done.

(gdb) r

Starting program: /root/game/00_01_gcc/test

Hello, world!


```cpp
(gdb)
```

[Inferior 1 (process 1589) exited normally]

可以看到，这一次打印输出了“Hello，world！”之后，打印了进程退出的消息，没有什么异常打印，也没有警

告。好了，我们在 gdb 中完成了一次完整的程序执行。下面介绍怎么打断点与查看数据。

> **关键点 3：调试命令**

程序运行到断点时，可以用 gdb 来查看栈上的信息。如果我们正在用 gdb 运行一个可执行程序，程序遇到宕机时，

gdb 会主动在宕机处停下来，查看栈信息也可以获得更多的宕机信息。首先列出 gdb 调试时常用的命令，见表 A-8。

表 A-8 gdb 命令列表


```bash
[root@localhost 00_01_gcc]# gdb test
```

我们还是以 00_01_gcc 为例，使用 gdb 来调试。首先用 gdb 启动 test：

……

Reading symbols from /root/game/00_01_gcc/test...done.

步骤 01　我们先用 l 命令查看一下代码。

(gdb) l


```cpp
3     int main( int argc, char *argv[] ) {
4         printf("Hello, world!\n");
5         return 0;
6     }
```

1     #include <cstdio>

步骤 02　在第 4 行加一个断点。

(gdb) break 4

Breakpoint 1 at 0x4006d1: file /root/game/00_01_gcc/main.cpp, line 4.

步骤 03　输入“r”开始调试，程序运行到第 4 行时会中断。

(gdb) r


```cpp
Breakpoint 1, main (argc=1, argv=0x7fffffffe578) at /root/game/00_01_gcc/main.cpp:4
4    printf("Hello, world!\n");
(gdb)
```

Starting program: /root/game/00_01_gcc/test

步骤 04　向下单步执行“n”。

(gdb) n

Hello, world!

> [插图：第 4 节 CMakeLists.txt 中出现的编译选项逐条释义：警告开关、C++ 标准选择、优化级别、Release 裁剪宏、gdb 调试信息与自定义宏。]

## 6. gdb 常用命令表

| 命　令 | 说　明 |
| --- | --- |
| `l` | 查看当前行后面 10 行的代码 |
| `l` 行号 | 查看行号前后 10 行的代码 |
| `l` 起始行号 结束行号 | 查看起始到结束行的代码 |
| `break` 文件名:行号 | 在给定的行号上打一个断点 |
| `break` 函数名 | 为给定的函数打一个断点 |
| `r` | 开始调试 |
| `n`（=next） | 单步调试 |
| `s` | 单步调试，进入函数 |
| `c`（= continue） | 继续 |
| `clear` 文件名:行号 | 清除一个 break |
| `p` 变量 | 查看变量 |
| `backtrace` | 打印全部栈的调用信息，按 Ctrl+C 组合键可终止打印 |
| `info local` | 显示当前栈局部变量的值 |

> [插图：gdb 调试命令对照表，共 13 条，分为源码查看（`l`）、断点设置（`break`/`clear`）、执行控制（`r`/`n`/`s`/`c`）与信息查看（`p`/`backtrace`/`info local`）四类。]


```cpp
5    return 0;
```

在单步跟踪时，执行了第 4 行代码，同时打印出了第 5 行代码的源代码。

步骤 05　如果我们不想再跟踪，或者需要程序直接运行到下一个断点，这时执行“c”继续向下执行：

(gdb) c

Continuing.


```cpp
(gdb)
```

[Inferior 1 (process 1460) exited normally]

本例中后续没有再设置断点，程序就完成退出了，输入“q”退出 gdb，一个简单的调试就完成了。

> **关键点 4：跟踪变量**


```bash
[root@localhost 01_testgcc]# vim main.cpp
#include <cstdio>
int main( int argc, char *argv[] ) {
    int i = 1;
    printf("Hello, world!\n");
    i = 2;
    return 0;
}
```

我们修改一下代码跟踪变量。使用 vim 修改 main.cpp 文件如下：

修改完成之后直接 make，因为没有增加或减少文件数量，Makefile 不需要重新生成。GCC 会给出一个警告，虽然定

义了一个 i 变量，但是没有使用它。再来看看 gdb 的表现：

步骤 01　输入 l 命令查看代码。

(gdb) l


```cpp
3     int main( int argc, char *argv[] ) {
4         int i = 1;
5         printf("Hello, world!\n");
6         i = 2;
7         return 0;
8     }
```

1     #include <cstdio>

步骤 02　在第 5 行、第 7 行打断点。

(gdb) break 5

Breakpoint 1 at 0x4006d8: file /root/game/00_01_gcc/main.cpp, line 5.

(gdb) break 7

Breakpoint 2 at 0x4006e9: file /root/game/00_01_gcc/main.cpp, line 7.

步骤 03　输入 r 命令执行程序，在第 5 行时打断点。

(gdb) r


```cpp
Breakpoint 1, main (argc=1, argv=0x7fffffffe578) at /root/game/00_01_gcc/main.cpp:5
5    printf("Hello, world!\n");
```

Starting program: /root/game/00_01_gcc/test

步骤 04　查看变量。


```cpp
i = 1
```

(gdb) info local

(gdb) p i

$1 = 1

我们可以使用 info 或 p 命令。info local 显示所有本地变量。p 则指定变量名。此时，输入“info local”查看当

前 local 的情况，i 被赋值为 1 了。输入“c”继续运行到下一个断点处：

(gdb) c

Continuing.


```cpp
Breakpoint 2, main (argc=1, argv=0x7fffffffe578) at /root/game/00_01_gcc/main.cpp:7
7 return 0;
```

Hello, world!


```cpp
i = 2
```

(gdb) info local

在第 7 行断点处，再次输入“info local”，发现 i 被赋值为 2 了。

这个过程和在 Visual Studio 窗口界面上查看变量的值的过程是一样的，只是 Visual Studio 有界面，我们使用的

是命令。

> **关键点 5：堆栈**

再次修改源代码查看堆栈的情况。如果你实在不习惯 vim 式的修改，可以在 samba 共享的目录中打开 main.cpp，在


```bash
[root@localhost 01_testgcc]# vim main.cpp
#include <cstdio>
void fun() {
    printf("Hello, world!\n");
}
int main( int argc, char *argv[] ) {
    int i = 1;
    fun();
    i = 2;
    return 0;
}
```

Windows 系统下修改文件。

在源代码中增加了一个 fun 函数，保存后再次执行 make 编译，进入 gdb 调试：

步骤 01　查看源代码文件。

(gdb) l


```cpp
2     void fun() {
3         printf("Hello, world!\n");
4     }
5     int main( int argc, char *argv[] ) {
6         int i = 1;
7         fun();
8         i = 2;
9         return 0;
10     }
```

1     #include <cstdio>

步骤 02　在函数内加入一个断点。

(gdb) break 3

Breakpoint 1 at 0x4006f6: file /root/game/00_01_gcc/main.cpp, line 3.

步骤 03　执行到断点位置。

(gdb) r


```cpp
Breakpoint 1, fun () at /root/game/00_01_gcc/main.cpp:3
3    printf("Hello, world!\n");
```

Starting program: /root/game/00_01_gcc/test

步骤 04　查看堆栈。

断点停下来之后，我们执行 backtrace 命令，即可看到当前的堆栈信息。当前只有两层堆栈，我们在当前堆栈下查

看局部变量，因为没有局部变量，显示为“No locals”。

(gdb) backtrace

#0  fun () at /root/game/00_01_gcc/main.cpp:3

#1  0x000000000040071e in main (argc=1, argv=0x7fffffffe578) at /root/game/00_01_gcc/main.cpp:7

(gdb) info local

No locals.

同时可以使用 up n、down n 命令来移动堆栈，其中 n 表示前进或后退的层数。退回到上一层。

(gdb) up 1


```cpp
7        fun();
```

#1  0x000000000040071e in main (argc=1, argv=0x7fffffffe578) at /root/game/00_01_gcc/main.cpp:7


```cpp
i = 1
```

(gdb) info local

可以看到上一层的调用位于 main.cpp 的第 7 行，再打印“info local”的时候，变量 i 的值被打印出来了。继续执

行输入“c”，退出输入“q”。好了，到目前为止，我们使用了几个简单的小例子，初步探索了 gdb 的使用。

除了上面这种调试方式外，gdb 还可以对一个正在运行中的程序进行调试，简单可描述为两步：先找到这个程序的

PID，再执行 gdb attach PID。对一个正在运行中的程序调试是我们经常会遇到的事情，刚开始并没有以 gdb 的方式启

动，但是程序在中途出现了错误，这时也需要调试。我们用一个例子来说明这个情况：


```bash
[root@localhost 00_01_gcc]# vim main.cpp
#include <cstdio>
int main( int argc, char *argv[] ) {
    int i = 0;
    while(true){
        i++;
    }
    return 0;
}
```

步骤 01　修改 main.cpp 文件如下：


```bash
[root@localhost 00_01_gcc]# ./test
```

退出 vim，执行 make 生成新的 test 文件。执行 test 文件：

按回车键之后，就会发现 test 一直没有返回，当然不会返回，我们写了一个死循环。


```bash
[root@localhost ~]# ps -ef|grep test
```

步骤 02　在 XShell 上再打开一个终端连接到虚拟机上。

root      1948  1881 99 18:06 pts/0    00:02:42 ./test

root      1976  1952  0 18:09 pts/1    00:00:00 grep --color=auto test

此时输入“ps-ef|grep test”命令，查看有没有名为 test 的进程，ps 命令是 Process Status 的缩写。第一个就是

刚才运行的 test 进程。显示数据的前 3 项比较重要，分别是 UID、PID 和 PPID。UID 是账号 ID，PID 是进程 ID，而 PPID 是该

进程的父进程 ID。现在找到了 test 的 PID 为 1948。


```bash
[root@localhost ~]# gdb attach 1948
```

步骤 03　执行 gdb。

……

Attaching to process 1948

……


```cpp
5    i++;
```

0x0000000000400698 in main (argc=1, argv=0x7fff1c0173c8) at /root/game/00_01_gcc/main.cpp:5


```cpp
i = 20783
```

(gdb) info local

当我们跟进进程时，它正好执行到了 main.cpp 的第 5 行。这时这个循环已经执行了 2 万多次了。在使用 attach 命令

退出时，需要使用 detach 命令将进程与 gdb 剥离。这时可以使用各种命令查看线程或进程信息，这里不再逐一介绍了。

> **关键点 6：Core**

在某些情况下，在 Linux 系统中运行程序是直接执行的，没有用到 gdb，但是程序发生了宕机，这时可以查看内核


```bash
[root@localhost ~]# ulimit -c
```

生成的 Core 文件来查看宕机时的信息。首先确认是否打开了 Core 文件的开关。

命令“ulimit-c”，如果结果为 0，就表示关闭了 Core 生成。

执行“ulimit-c unlimited”，打开 Core 文件生成，生成的 Core 文件大小无限制。如果需要限制“ulimit-c


```bash
[root@localhost ~]# ulimit -c unlimited
[root@localhost ~]# ulimit -c
```

size”，size 的单位是 Kb。

unlimited

在终端执行上面的命令，仅在本次会话中有效果。如果要长期打开 Core，就需要修改 profile 文件。执行如下命


```bash
[root@localhost ~]# echo "ulimit -c unlimited" >> /etc/profile
[root@localhost ~]# source /etc/profile
[root@localhost ~]# ulimit -c
```

令：

unlimited

以上 3 个命令实现的功能是，将命令写入 profile 文件中重新加载，让它立即生效。下面还用一个例子来说明 Core


```bash
[root@localhost 00_02_gdb]# cmake3 ./
[root@localhost 00_02_gdb]# make
```

文件的用途。使用的工程为本书源代码库中的 00_02_gdb 工程，用以下两步进行编译：

在执行 make 命令时，GCC 会抛出一个警告，忽视这个警告信息。Make 之后，生成可执行文件 core_null_d。此处

的“_d”是故意加上的，以标志该执行文件为 debug 模式。执行文件生成之后，开始查看 Core 文件的步骤：


```bash
[root@localhost 00_02_gdb]# ./core_null_d
```

步骤 01　执行 core_null_d，会出现一个段错误。


```bash
[root@localhost 00_02_gdb]# ls
```

段错误 ( 吐核 )

CMakeCache.txt  CMakeFiles  cmake_install.cmake  CMakeLists.txt  core.2485  core_null_d  main.cpp  Makefi

使用 ls 查看，在本地生成一个 core.XXXX 的文件，XXXX 部分是 PID，所以每次可能不同。


```bash
[root@localhost 00_02_gdb]# gdb core_null_d core.2485
```

步骤 02　使用 gdb 命令，其格式为：gdb 可执行文件 core 文件。

……

Program terminated with signal 11, Segmentation fault.


```cpp
6         *pChar = '0';
```

#0  0x00000000004006ed in main () at /root/game/00_02_gdb/main.cpp:6

可以看到 gdb 信息中列出了宕机的代码位置，就在 main.cpp 的第 6 行。

步骤 03　如果上一步不够详细，可以使用 bt（backtrace）命令列出堆栈：

(gdb) bt

#0  0x00000000004006ed in main () at /root/game/00_02_gdb/main.cpp:6

这一行就是出错的行，对应代码是*pChar = '0'。因为 pChar 指针是一个空指针，所以这里抛出了异常。

CMake、gdb 都是 Linux 编程中必备的知识点，由于篇幅有限，有一个大致的印象即可，需要深入学习时可以阅读一

些相关的资料。

A.6.4 安装数据库环境

本书要完成一个完整的服务端框架体系，我们需要用到数据库。在 CentOS 的仓库中使用的是 MariaDB。MariaDB 是

MySQL 的一个分支，这个分支形成的原因是 MySQL 被 Oracle 收购之后，可能面临着闭源。MariaDB 就是 MySQL 闭源时的替

代品，它依然是一个免费开源的数据库，由 MySQL 原作者开发维护。MariaDB 的命令与 MySQL 基本是一致的，在性能上官


```bash
[root@localhost ~]# yum install -y mariadb-server mariadb mariadb-devel
[root@localhost ~]# systemctl start mariadb    // 启动服务
[root@localhost ~]# mysql                      // 执行
```

方认为 MariaDB 可能更优于原 MySQL。执行安装与启动命令：

Server version: 5.5.60-mariadb mariadb Server

mariadb [(none)]>

……

出现提示符说明已经安装成功了。我们在 MariaDB 提示符输入“exit”，命令退回到 Shell 界面下，对 MySQL 进行配

置。

> **关键点 1：配置**


```bash
[root@localhost ~]# mysqlL_secure_installation
```

在使用 MariaDB 之前，需要对 MariaDB 进行配置。执行如下命令：

• （1）Enter current password for root（enter for none）：输入当前密码，当前默认的密码为空，所以这里

直接按回车键。

• （2）Set root password？[y/n]：是否设置 root 密码，输入“y”，然后输入密码。注意，输入密码时在 Linux 端

是看不到任何表达的，也就是说不会有****这样的字符，输入完成后按回车键再次确认。

• （3）Remove anonymous users？[y/n]：是否删除匿名用户，输入“y”。

• （4）Disallow root login remotely？[y/n]：禁止 root 远程登录，输入“n”。在例子中需要远程登录，所以这

里选择了“n”。

• （5）Remove test database and access to it？[y/n]：是否删除测试数据库以及权限，输入“y”。

• （6）Reload privilege tables now？[y/n]：是否重新加载权限表，输入“y”。

执行了以上 6 步，Shell 就会提示你成功完成。

> **关键点 2：开放端口**


```bash
[root@localhost ~]# firewall-cmd --zone=public --add-port=3306/TCP --permanent
```

我们还需要将 3306 端口开放出来，并重新加载防火墙。如果没有防火墙，那么可以省略这一步。

输入 firewall-cmd 命令，如果提示如下：

• （1）FirewallD is not running：表示没有防火墙，后续步骤可省略。


```bash
[root@localhost ~]# firewall-cmd --reload
```

• （2）success：需要重启防火墙。命令如下：

> **关键点 3：权限**


```bash
[root@localhost ~]# mysql -u root -p
```

再次进入 MySQL，这次因为有了密码，所以需要输入账号与密码：

Enter password:


```cpp
mariadb [(none)]> grant all on *.* to 'root'@'%' identified by '123456';
Query OK, 0 rows affected (0.00 sec)
mariadb [(none)]> flush privileges;
Query OK, 0 rows affected (0.00 sec)
```

输入密码之后，为 root 用户授权，分为两步：

第一条命令是授权，其中 123456 为用户密码;第二条命令让授权生效。现在，我们可以在 Windows 端用 navicat 这

类的软件连接一下 DB，测试连接是否能够成功。

> **关键点 4：字符集**


```cpp
mariadb [(none)]> show variables like 'ch%';
```

使用数据库时，字符集是一个比较敏感的问题，我们查看一下默认的字符集：

+--------------------------+----------------------------+

| Variable_name            | Value                        |

+--------------------------+----------------------------+

| character_set_client     | utf8                       |

| character_set_connection | utf8                       |

| character_set_database   | latin1                     |

| character_set_filesystem | binary                     |

| character_set_results    | utf8                       |

| character_set_server     | latin1                     |

| character_set_system     | utf8                       |

| character_sets_dir       | /usr/share/MySQL/charsets/ |


```cpp
8 rows in set (0.00 sec)
```

+--------------------------+----------------------------+

数据库默认的 database 和 server 的字符集都是 latin，但是在程序中需要 utf8 字符集。在创建数据库时需要注意，


```cpp
mariadb [(none)]> show variables like 'collation%';
```

后面会在程序中将数据库修改为 utf8。再来看一下 collation：

+----------------------+-------------------+

| Variable_name          | Value               |

+----------------------+-------------------+

| collation_connection | utf8_general_ci    |

| collation_database    | latin1_swedish_ci |

| collation_server      | latin1_swedish_ci |


```cpp
3 rows in set (0.00 sec)
```

+----------------------+-------------------+

ci 为 case insensitive 的缩写，即对大小写不敏感。也就是说，Test 账号与 test 账号是同一个账号。

> **关键点 5：开机启动**


```bash
[root@localhost ~]#  systemctl enable mariadb
```

一切配置完成之后，我们将数据库服务加入开机启动服务中：

Created symlink from /etc/systemd/system/multi-user.target.wants/ mariadb.service to /usr/lib/systemd/sys


```bash
[root@localhost ~]# systemctl list-unit-files --type service --state enabled
```

如果不确定是否已经开机列表中，那么可以查看一下：

UNIT FILE       STATE

……

mariadb.service enabled

Enabled 表示 MariaDB 的服务已经在开机列表中了。
