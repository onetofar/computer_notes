---
title: "Docker配置踩坑指南"
category: 常用工具
tags:
  - net/practice
  - net
difficulty: 基础
source: "自整理"
---
# CLion + Docker 容器化C++开发完整踩坑指南
## 一、概述
本文档完整记录了从**WSL本地开发**迁移到**Docker容器化开发**过程中遇到的所有问题、错误根源及最终解决方案。目标是实现标准化、可复现的开发环境，解决"在我电脑上能跑"的问题，同时隔离开发环境与宿主机系统。
### 为什么要容器化开发？
1.  **环境一致性**：所有开发者使用完全相同的编译、调试、依赖环境，避免"本地能跑，线上崩"
2.  **系统隔离**：开发环境的修改不会污染宿主机系统，随时可以重置
3.  **权限隔离**：网络抓包、内核操作等高权限操作在容器内完成，不影响宿主机安全
4.  **快速迁移**：开发环境可以一键打包、部署到任何支持Docker的机器
## 二、前置准备：正确的项目结构与配置文件
所有问题的根源几乎都来自**错误的项目结构**和**错误的配置文件**。这是容器化开发的基础，必须严格遵守。
### 2.1 标准项目结构
**核心原则：所有文件必须放在同一个项目根目录下**
```
/root/game-server-engine/          # 项目根目录（名字可自定义）
├── Dockerfile                     # 镜像构建文件
├── docker-compose.yml             # 容器编排文件
├── CMakeLists.txt                 # 项目构建配置
├── src/                           # 源代码目录
│   ├── arp_sender.cpp
│   └── arp_sniffer.cpp
├── .clang-format                  # 代码格式化配置
└── .gitignore                     # Git忽略配置
```
### 2.2 Dockerfile（镜像构建文件）
```dockerfile
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
# 替换阿里云源，加速国内下载
RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list && \
    apt update && \
    apt install -y --no-install-recommends \
        build-essential g++ gdb cmake libpcap-dev tcpdump git \
        valgrind iperf3 net-tools iproute2 iputils-ping vim && \
    rm -rf /var/lib/apt/lists/*  # 清理缓存，减小镜像体积
# 容器内默认工作目录
WORKDIR /workspace
# 容器启动后默认执行bash，保持交互
CMD ["/bin/bash"]
```
### 2.3 docker-compose.yml（容器编排文件）
```yaml
services:
  workspace-dev:
    build: .                          # 基于当前目录的Dockerfile构建镜像
    image: workspace-dev:latest       # 强制指定镜像名，避免生成随机前缀
    container_name: workspace-dev     # 固定容器名，方便管理
    network_mode: "host"              # 共享宿主机网络栈（抓包必须）
    cap_add:                          # 追加内核权限（网络操作必须）
      - NET_RAW                       # 允许原始套接字（tcpdump抓包）
      - NET_ADMIN                     # 允许修改网络配置
    volumes:
      - ./:/workspace                 # 【核心挂载规则】宿主机当前目录 ↔ 容器内/workspace
    tty: true                         # 分配伪终端，保持容器运行
    stdin_open: true                  # 保持标准输入打开，支持交互
    restart: "no"                     # 容器退出后不自动重启
```
## 三、核心问题与解决方案
### 3.1 最致命的坑：挂载错误（容器内/workspace为空）
#### 问题现象
- 容器启动成功，但进入容器后`ls /workspace`为空
- CLion构建报错：找不到源文件、CMakeLists.txt不存在
- 宿主机修改代码，容器内看不到变化
#### 错误根源
`docker-compose.yml`中的`./:/workspace`挂载规则，**`./`是执行`docker compose up`命令时的当前目录，而不是docker-compose.yml文件所在的目录**。
之前的错误操作：
1.  在WSL根目录`/workspace`（空目录）执行了`docker compose up`
2.  导致`./`指向了空的`/workspace`，挂载到容器内的`/workspace`
3.  容器内自然看不到任何代码
#### 解决方案
1.  **强制清理所有旧容器**（避免残留容器干扰）
    ```bash
    cd /root/game-server-engine/
    docker compose down
    docker rm -f $(docker ps -a -q --filter ancestor=workspace-dev:latest)
    ```
2.  **必须在项目根目录执行启动命令**
    ```bash
    # 进入项目根目录（docker-compose.yml所在目录）
    cd /root/game-server-engine/
    # 强制构建并启动容器
    docker compose up -d --build
    ```
3.  **验证挂载是否正确**
    ```bash
    # 查看挂载信息
    docker inspect workspace-dev | grep -A 10 "Mounts"
    ```
    ✅ 正确输出：
    ```json
    "Mounts": [
        {
            "Type": "bind",
            "Source": "/root/game-server-engine",  // 宿主机项目目录
            "Destination": "/workspace",           // 容器内目录
            "Mode": "rw",
            "RW": true
        }
    ]
    ```
4.  **验证代码是否同步**
    ```bash
    docker exec -it workspace-dev bash
    ls /workspace  # 应该能看到src/、CMakeLists.txt等文件
    ```
### 3.2 CLion构建目录路径错误
#### 问题现象
```
com.github.dockerjava.api.exception.DockerException: Status 400: 
the working directory '\\wsl.localhost\Ubuntu-22.04\workspace\cmake-build-debug' is invalid, 
it needs to be an absolute path
```
#### 错误根源
- Docker容器是Linux系统，**只认识`/`开头的Linux绝对路径**
- CLion会自动将Windows/WSL格式的路径（`\\wsl.localhost\...`）转换为容器不认识的格式
- 即使手动输入`/workspace/xxx`，CLion也可能自动转成`\workspace\xxx`
#### 解决方案
**不要填绝对路径！只填相对路径！**
1.  打开CLion → 设置 → 构建、执行、部署 → CMake
2.  构建目录：**只填`cmake-build-debug`**（不要加任何前缀）
3.  工具链：选择配置好的Docker工具链
4.  点击应用 → Reload CMake Project
✅ 原理：当工具链为Docker时，CLion会自动将相对路径解析为容器内的`/workspace/cmake-build-debug`，并自动同步到宿主机的`/root/game-server-engine/cmake-build-debug`。
### 3.3 单文件运行配置不支持远程工具链
#### 问题现象
```
C/C++ 文件运行配置不支持远程工具链。C/C++ 文件运行配置仅支持本地工具链
```
#### 错误根源
点击单个`.cpp`文件前的绿色箭头，会生成**单文件快速运行配置**，这种配置只能使用本地工具链，不支持Docker/WSL等远程工具链。
#### 解决方案
使用**CMake目标配置**：
1.  右上角配置下拉框 → 选择CMake自动生成的`arp_sender`目标（对应`add_executable`）
2.  不要选择单个`.cpp`文件的配置
3.  编辑配置 → 确认工具链为Docker，工作目录为`/workspace`
### 3.4 容器启动参数缺失导致抓包失败
#### 问题现象
- `pcap_open_live`报错：无法打开网卡
- 程序运行后看不到任何网络包
- 无法发送ARP报文
#### 错误根源
容器默认没有网络操作权限，且使用隔离的网络命名空间，看不到宿主机的网卡。
#### 解决方案
在`docker-compose.yml`中添加以下配置：
```yaml
network_mode: "host"              # 共享宿主机网络栈
cap_add:
  - NET_RAW                       # 原始套接字权限（抓包必须）
  - NET_ADMIN                     # 网络管理权限（发送ARP必须）
```
### 3.5 容器残留导致docker compose down无效
#### 问题现象
- 执行`docker compose down`提示`No resource found to remove`
- `docker ps -a`显示大量状态为`Created`的残留容器
- 新启动的容器还是使用旧的挂载配置
#### 解决方案
强制删除所有相关容器：
```bash
docker rm -f $(docker ps -a -q --filter ancestor=workspace-dev:latest)
```
---

## 四、完整正确流程（从0到1）
### 4.1 环境准备
1.  确保WSL2已安装并启用
2.  确保Docker Desktop已安装，且配置为使用WSL2后端
3.  按照2.1节创建标准项目结构
4.  按照2.2和2.3节编写Dockerfile和docker-compose.yml

### 4.2 启动容器
```bash
# 进入项目根目录（必须！）
cd /root/game-server-engine/

# 清理旧容器
docker compose down
docker rm -f $(docker ps -a -q --filter ancestor=workspace-dev:latest)

# 构建并启动容器
docker compose up -d --build

# 验证容器状态
docker ps  # 应该看到workspace-dev容器状态为Up

# 验证挂载
docker exec -it workspace-dev bash
ls /workspace  # 应该能看到所有项目文件
```

### 4.3 CLion配置
1.  打开CLion → 打开项目`/root/game-server-engine`
2.  配置Docker工具链：
    - 设置 → 构建、执行、部署 → 工具链 → 添加 → Docker
    - 连接方式：WSL: Ubuntu-22.04
    - 镜像文件：workspace-dev:latest
    - 容器设置 → 运行选项：`--net=host --cap-add=NET_RAW --cap-add=NET_ADMIN`
    - 点击刷新，确认CMake、g++、gdb都检测成功
3.  配置CMake：
    - 设置 → 构建、执行、部署 → CMake
    - 工具链：选择刚才配置的Docker工具链
    - 构建目录：`cmake-build-debug`（只填相对路径）
    - CMake选项：`-DCMAKE_BUILD_TYPE=Debug`
    - 点击应用 → Reload CMake Project
4.  配置运行/调试：
    - 右上角配置下拉框 → 选择`arp_sender`目标
    - 编辑配置 → 程序参数：`eth0 192.168.1.1`（你的网卡和目标IP）
    - 工作目录：`/workspace`

### 4.4 构建、运行、调试
1.  **构建**：点击右上角锤子图标，日志显示`[100%] Built target arp_sender`
2.  **运行**：点击运行按钮，程序在容器内启动，共享宿主机网络
3.  **调试**：在代码行号左侧打红色断点，点击调试按钮，断点正常命中

---

## 五、避坑指南（核心原则）
1.  **所有文件必须在同一个项目根目录下**，包括Dockerfile、docker-compose.yml、代码、配置
2.  **所有docker compose命令必须在项目根目录执行**，否则`./`会指向错误的路径
3.  **CLion构建目录只填相对路径**，不要填任何绝对路径
4.  **永远使用CMake目标配置运行/调试**，不要点击单个.cpp文件的箭头
5.  **网络开发必须使用host网络模式**，并添加NET_RAW和NET_ADMIN权限
6.  **遇到奇怪的问题先清理旧容器**：`docker rm -f $(docker ps -a -q --filter ancestor=workspace-dev:latest)`
7.  **WSL根目录的/workspace可以删除**，它和容器内的/workspace没有任何关系

---

## 六、总结
容器化开发的核心是**环境一致性**和**隔离性**，但前提是配置正确。本次踩坑的所有问题，本质上都是对Docker挂载规则和CLion远程工具链机制的误解。

通过本文档的流程，你可以快速搭建一个标准化的C++网络开发环境，所有编译、运行、调试都在容器内完成，同时代码实时同步到宿主机，兼顾了开发效率和环境一致性。