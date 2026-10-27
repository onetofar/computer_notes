---
title: "4-2 WSL 退役与 Linux 迁移:开发环境搬去 Fedora,推理留在 Windows"
category: 大模型部署
tags:
  - WSL
  - Fedora
  - 环境迁移
  - 本地推理
  - 依赖管理
  - pkg-config
difficulty: 进阶
source: "2026-10-28 WSL2 销毁 + Fedora 44 笔记本迁移收尾(实测: HEAD 6efad4e / 43 产物 / 工作区干净)"
---

# WSL 退役与 Linux 迁移:开发环境搬去 Fedora,推理留在 Windows

> 这篇回答两个问题:**为什么把 WSL 整个删掉**、**为什么"开发"和"推理"要分到两台机器上**。
> 推理侧的选型与参数见 [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1]];WSL 里 vLLM 踩过的坑见 [[3-1 3080 12G 实战部署：Qwen3-8B-AWQ + vLLM 参数取舍实录|3-1]]。

## 结论先行

**不是 WSL 不好,是它同时承担了两件目标冲突的事。**

| 它承担的职责 | 它擅长的地方 | 它付出的代价 |
|---|---|---|
| 跑 GPU 推理(vLLM) | 能拿到 CUDA | 多一层文件系统 + 多一层网络 + 工具链自己拼,故障面翻倍 |
| 当 C++ 服务器开发环境 | 有 Linux 语义(epoll/POSIX) | 它只是**转发层**,不是真 Linux;排障时永远要先问"是 Linux 的问题还是 WSL 的问题" |

拆开之后各归各位:**推理留在真 Windows(原生 CUDA,无中间层),开发搬去真 Linux(Fedora 笔记本,ssh 直连)**。两边都用原生,中间那层消失。

---

## 一、WSL 为什么不再适合跑推理

这四条都是 [[3-1 3080 12G 实战部署：Qwen3-8B-AWQ + vLLM 参数取舍实录|3-1]] 里实测撞出来的,不是推测:

### ① 网络层:mirrored 模式下回环不通

现象:Linux 侧内核明明在 LISTEN,`127.0.0.1:8000` 仍 `Connection refused`;Windows 侧 `localhost` 也不通;只有 WSL 的 eth0 IP(`192.168.1.5`)可达。

后果:客户端配置、启停脚本、健康探测全部要绕 IP;而 IP 随 Windows 主机 DHCP 变化,脚本里得动态取 `hostname -I`。

### ② 工具链层:编译器与头文件版本错配

现象:`#error "CUDA compiler and CUDA toolkit headers are incompatible"`——pip 装的 nvcc 是 13.4.92,头文件却是 13.0.x。

后果:为了绕开一个 JIT,**自己拼了一整套 pip CUDA 组件**(`CUDA_HOME` 指向 `site-packages/nvidia/cu13`)。这种"环境靠自己拼出来"的配置,换台机器就不可复现。

### ③ 文件系统层:权重存两遍

模型权重躺在 `ext4.vhdx` 里——那是 Windows 上的一个**大文件**。要在 Windows 侧看它、备份它、或者让 Windows 服务读它,就得再复制一份。删掉 WSL 时那 5.7G 权重跟着 vhdx 一起消失,这就是代价的具体形态。

### ④ 启动链路层:一次启动穿四层

`桌面 .bat` → `wsl.exe -u root` → `bash 脚本` → `python venv` → `vLLM 多进程树`。任何一层挂了,报错都在上一层看不见。

Windows 原生 Ollama 的对等链路是:`start-ollama.ps1` → `ollama serve`。**少三层,就少三类故障。**

> 💡 判据:**中间层的价值必须大于它的故障面**。WSL 曾经值这个价(Windows 上没有别的 Linux);当一台真 Linux 就在局域网里时,它不值了。

---

## 二、为什么开发环境要搬去真 Linux

C++ 服务器开发要的是 Linux 语义:epoll、POSIX 线程、`/proc`、信号语义、`ldd`/`pkg-config` 这套工具链的真实行为。这些在 WSL 里"看起来一样",但**排障时多了一层不确定性**:一个链接错误到底是代码问题、发行版问题,还是 WSL 转发层的问题?

搬到一台独立机器后,还有一个额外收益:**它是一台可以随时断电重启的机器**。WSL 与 Windows 宿主共享生命周期,宿主睡眠/更新会把开发环境一起拖走。

### 迁移清单:搬了什么、没搬什么

| 项目 | 处置 | 理由 |
|---|---|---|
| `~/dev/game-server-engine` | ✅ 搬 | 主仓库,工作副本状态完整保留 |
| `~/dev/onetofar_web` | ✅ 搬 | **11 项未提交改动**一并搬走——这类改动只能搬工作副本,clone 拿不到 |
| `~/.claude`(11 个 skills + 9 个 commands) | ✅ 搬 | 工作流资产 |
| `~/.vscode-server`(2.2G) | ✅ 搬 | 省掉重新下载;server commit 与客户端 1.140.0 对齐 |
| `vllm-env`(8.2G) | ❌ 不搬 | 虚拟环境与平台绑定,搬过去也是废的;且笔记本没有独显 |
| `Qwen3-8B-AWQ` 权重(5.7G) | ❌ 不搬 | 理由同上;推理改在 Windows 跑 |
| `java_project` / `models` / `docker-data` | ❌ 不搬 | 已被 C++ 方向取代 |
| `docs/` | ❌ 舍弃 | 网站要大改,7 月文档已过时,搬过去只是搬负担 |

> 💡 **"全搬"是伪需求**。迁移的价值在"决定不搬什么"——搬过去的每一份都是要维护的。

### 机器分工(明确不重叠)

| 机器 | 角色 | 不做什么 |
|---|---|---|
| Windows 桌面(7800X3D + 3080 12G) | **本地推理节点** + 日常主力 | 不做 Linux 编译调试 |
| Fedora 44 笔记本(i9-12900H / Iris Xe **核显** / 31 GiB) | **C++ 编译调试 + Agent 客户端** | **不部署模型权重、不做本地推理**(核显没有 CUDA 意义) |
| 云端 API | 长上下文、重推理任务 | 不承担离线场景 |

这条分工是硬约束不是偏好:**笔记本是核显,推理跑不动**;而推理留在 Windows 是因为那里有独显。反过来编译调试放笔记本,是因为那里有真 Linux。

---

## 三、依赖必须走系统包管理器

迁移时最容易偷懒的一步是"免 sudo 绕过"。这次明确否掉了土办法:

| 做法 | 结果 |
|---|---|
| **dnf 系统装 `-devel` 包**(选) | `.pc` 落在 `/usr/lib64/pkgconfig`,CMake 自动发现,`ldd` 全指 `/lib64` |
| `dnf download` + `rpm2cpio` 解到 `~/opt` 用户前缀 | ❌ 踩坑:`libibverbs.pc` 压根没有 `prefix=` 行 → 解析成 `/include` → CMake 报 `Imported target "PkgConfig::PCAP" includes non-existent path` |

根因:**`.pc` 文件里写的是绝对路径**,换个前缀就要改一堆 `.pc`,而且改不全就会有一个包在某处悄悄指错。
免 sudo 的诱惑很大,但代价是"这套环境不受包管理器追踪",以后 `dnf` 不知道它的存在。

### 实测证据(2026-10-28 复查)

```
libpcap=1.10.7            dir=/usr/lib64/pkgconfig
libevent=2.1.13-stable    dir=/usr/lib64/pkgconfig
openssl=3.5.9             dir=/usr/lib64/pkgconfig
```

三个 `.pc` 全部命中系统路径。json 相关头文件也在系统路径下,但它不通过 `.pc` 暴露(`pkg-config json-c` 为空),所以不在上面这张表里。

> 💡 `pkg-config --variable=pcfiledir` 这一条比"编译通过了"更可信。**编译过可能是碰巧搜到了某个残留在 `/usr/local` 的旧版本**;`.pc` 的绝对路径才是"它从哪来"的铁证。

---

## 四、验证:干净重建才算迁移完成

`~/dev/rebuild_verify.sh` 实测结果:

| 检查项 | 结果 |
|---|---|
| 四个 `-devel` 依赖 | 全从 `/usr` 系统路径命中 |
| 编译 `-j20` | 退出码 0,**43 个可执行产物** |
| `ldd` 依赖指向 | 全部 `/lib64`(无用户前缀残留) |
| `game_server_tests` | 退出码 0 |
| 服务监听 | 19002/19003/19006/19007;HTTP 探活返回 404(无路由时的正常应答) |
| 仓库状态 | `HEAD 6efad4e`,工作区 **0 脏** |

工具链:`GCC 16.2.1` / `CMake 4.3.0` / `podman 5.8.9`,机器 `20` 线程 / `31 GiB` / `/home` 余 `460G`。

> 💡 判据:**迁移是否完成,看的是"从零重建能不能过",不是"旧目录复制过去了"**。复制过去的只是文件,能重建的才是环境。

---

## 五、踩坑速记

| 坑 | 现象 | 解法 |
|---|---|---|
| **PowerShell → ssh 吃引号** | `printf` 写入内容变乱码(出现过 `n##` 这种残行);`GIT_SSH_COMMAND` 失效 | **本地写文件 → `scp` 上传 → 远端执行**;不要在 ssh 一个参数里塞多行脚本/引号 |
| **clangd 索引被误提交** | `.cache/clangd/index/*.idx` 43 个文件 7.6MB 进了 git | commit `6efad4e` 脱离跟踪。注意 `.gitignore` 里 `.cache/` 规则**加晚了**;而第 22 行把 `.gitignore` 自身也忽略了(本地私有写法,保持原样) |
| **GitHub 无机钥可认证** | 仓库私有、HTTPS 无凭据、WSL 从未存过 git 凭据 | 把服务器公钥登记进 GitHub → remote 切 `git@github.com:...` → push 成功 |
| **首次 ssh 交互卡住** | `known_hosts` 未收录 → 交互式确认 | `ssh-keyscan -H github.com >> ~/.ssh/known_hosts` |
| **后台等远端长任务** | `pgrep -f 脚本名` 会匹配到自己 | 用 `pgrep -f '[r]ebuild_verify'`(方括号防自匹配),并区分"进程结束"与"服务器掉线" |
| **服务器会间歇掉线** | 睡眠/无线节能导致长传输中断 | 探测上线 + 整轮重试包装 |

---

## 六、WSL 删除的具体动作与不可逆性

```
wsl --shutdown
wsl --unregister Ubuntu-22.04     # 自动删除 ext4.vhdx
# 结果:F:\WSL\Ubuntu-22.04 成为空目录,清理后 wsl -l 零发行版
#       F 盘回收至 100.3 GB 空闲
```

**这是不可回滚操作。** 随之消失的不只是 Linux 用户态,还有:`vllm-env`(8.2G)、Qwen3-8B-AWQ 权重(5.7G)、`java_project`、`docker-data`、WSL 内的 `docs`、以及 `.claude` 的历史。

> 💡 判据:**销毁类操作要先做完"搬什么/不搬什么"的清单并核对一遍**,再执行。清单做在前面是决策,做在后面只是事后追认。

---

## 七、失效条件

- **笔记本换了带独显的新机** → 分工表要重排,推理可能反而该搬到笔记本(便携优先)。
- **Windows 桌面换成 Linux 主机** → 推理侧可以回到 vLLM,并发能力立刻升级。
- **需要 Linux 专属 GPU 工具链**(如某些 profiler) → WSL 或双系统可能重新划算。
- **Ollama / llama.cpp 出了原生 Linux 版多并发优化** → 分工可以再合并成一台。

---

## 六、服务端本体也搬过来了(2026-10-28 补记)

上面四节讲的是**开发环境**的迁移。这天顺手把**服务端本体**(`otf-server`)也搬到了 Fedora,
结果比预想的顺——但撞上一堵硬墙,值得单独记。

### 搬的是什么

| 内容 | 做法 |
|---|---|
| 仓库本体 | `git clone git@github.com:onetofar/game-project.git`(仓库是私有的,但服务器公钥早已登记在 GitHub,SSH 直连) |
| 未提交改动 | `otf-server/src/main.cpp` 有一行未提交的中文注释,**单独 scp 过去**并用 sha256 校验三份文件哈希一致 |
| 构建产物 | **不搬**。Windows 上的 `build/` 是旧环境的产物,重编 |

> ⚠️ **老教训再次生效**:仓库里有 6 项未提交改动,其中服务端那项是**注释**、客户端资源那 3 个是**未跟踪的预览图**。
> 「clone 拿不到工作副本」这条在今天又验证了一次——不搬过去,注释就没了。

### 硬墙:`g++-12` 在 Fedora 上不存在

`otf-server/cmake/toolchain.cmake` 原本**硬钉** `g++-12`,找不到就 `FATAL_ERROR`。
理由写得很清楚:宿主机与容器必须**逐版本对齐**,否则「我这儿能编你那儿不能」,排查极耗时间。

问题在于:Fedora 44 只提供 GCC 16.2.1,`dnf list --available gcc-toolset-*` **查不到 gcc-toolset-12**。
硬钉 = 这台机器上根本编不出来。

### 处置:把「钉版本」从硬编码改成显式声明

没有偷偷改掉那个约束,而是把它**从隐式变显式**:

```cmake
set(OTF_GXX_VERSION "" CACHE STRING "要求的 g++ 主版本号(如 12);留空 = 用系统 g++")
set(OTF_CXX_COMPILER "" CACHE FILEPATH "显式指定 C++ 编译器路径(留空 = 自动选择)")
```

- **要求了版本却找不到 → 仍然 `FATAL_ERROR`**(原语义保留:「你明确要的,没有就是没有」)
- **什么都没要求 → 用系统 g++,并把实际版本打印到 CMake 日志**
- 想对齐仍然随时可以:`-DOTF_GXX_VERSION=12` 或 `-DOTF_CXX_COMPILER=/path/to/g++-12`

> 💡 判据:**「不许静默退回」不等于「必须硬钉」**。
> 前者防的是「编出行为不同但看起来正常的二进制」;后者防的是「换台机器就编不出来」。
> 正确的做法是把选择**显式化**,而不是取消检查。

### dev.sh 里另外两处硬编码

| 原值 | 改成 | 为什么 |
|---|---|---|
| `WSL_SRC=/mnt/f/game_project` | `${OTF_SRC_MOUNT:-$HOST_SRC}` | 当时宿主是 WSL、仓库在 Windows 盘,容器侧必须是 Linux 路径;现在仓库原生在 Linux 上,`HOST_SRC` 本身就是合法挂载源 |
| `docker` | `${CONTAINER_ENGINE:-docker}` | Fedora 默认只有 podman。`CONTAINER_ENGINE=podman` 即可 |

顺带修了一个自己踩的坑:`dev.sh` 开了 `set -u`,新增的 `$OTF_GXX_VERSION` 未定义会直接报错——
**所有外部入参都要在脚本里先给空值兜住**。

### 结果:19/19 全绿

```
== 宿主工具链 ==
  ✓ 系统 g++ = 16.2.1(未钉版本,toolchain.cmake 会用它)
  ✓ cmake /usr/bin/cmake      ✓ ninja /usr/bin/ninja     ✓ ctest /usr/bin/ctest
  ✓ ccache(启用)               ✓ clang-tidy
[66/66] Linking CXX executable otf_server
100% tests passed, 0 tests failed out of 19
Total Test time (real) = 2.01 sec
```

**g++-16 编译 g++-12 时代写的代码,零修改通过**——说明代码本身没有依赖旧编译器的行为差异。

### 附带发现:容器路其实也走不通

本来打算「装 podman-docker 当 docker 用,跑 `dev.sh verify` 做容器内复验」,结果 podman 拉镜像直接超时:

```
pinging container registry registry-1.docker.io: dial tcp [2a03:2880:...]:443: i/o timeout
```

这台机器的网络到 Docker Hub 不通。**结论:原生编译不是退而求其次,是唯一可行路**——
而原生编译恰好也是用户明确要的方向(「以硬件+最好结果适配为准」)。

### 这次迁移的通用判据

1. **先查目标机的工具链事实,再决定改不改约束**。`dnf list --available gcc-toolset-*` 一条命令,比任何推测都快。
2. **约束分两类**:「不许静默退回」(必须保留)与「必须硬钉」(可以放宽)。前者是正确性,后者是便利。
3. **私有仓库用 SSH 走**,公钥提前登记好,`git clone` 不需要任何凭据交互。
4. **未提交改动逐个判断是否要带走**,并用 sha256 确认传输无损。
5. **构建产物永不跨机搬**——它绑定的是「编译器版本 + 依赖版本 + 路径」三者的组合,搬过来就是负担。

## 关联

- [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1 Windows 原生推理选型]] —— 引擎为什么必须换:vLLM 要的 WSL 已经不在
- [[3-1 3080 12G 实战部署：Qwen3-8B-AWQ + vLLM 参数取舍实录|3-1 3080 12G 实战部署实录]] —— mirrored 网络与 flashinfer 两个坑的原始记录,本文第一节的依据
- [[1-1 vLLM 推理引擎精读|vLLM 推理引擎精读]] —— 为什么 vLLM 的强项是并发吞吐,与单用户的取舍
- [[5-1 模型文件名怎么读：Qwen3-14B Q4_K_M 逐段拆解|5-1 模型文件名怎么读]] —— 本次部署用到的两个模型文件名的含义
- 迁移收尾记录:服务器 `~/dev/README-migrated.md`;验证脚本 `~/dev/rebuild_verify.sh`
- 本机推理档案:`F:\LLM\README.md`(实测账本、启停脚本、provider 配置、跨机调用)
