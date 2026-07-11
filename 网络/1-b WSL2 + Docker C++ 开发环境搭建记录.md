---
title: "WSL2 + Docker C++ 开发环境搭建记录"
category: 常用工具
tags:
  - net/practice
  - net
difficulty: 基础
source: "自整理"
---
# WSL2 + Docker C++ 开发环境搭建踩坑记录
> **环境**：Windows 11 + WSL2 (Ubuntu 22.04) + Docker CE  
> **目标**：搭建纯 Linux C++ 网络编程开发环境，用于游戏服务器引擎项目  
> **最终状态**：全部解决，环境就绪
## 环境信息
| 组件      | 版本/配置                         |
| ------ | --------------------------------- |
| CPU       | AMD Ryzen 7 7800X3D (8C/16T)      |
| 内存      | 32GB DDR5                         |
| 操作系统  | Windows 11                        |
| WSL2 内核 | 6.6.114.1-microsoft-standard-WSL2 |
| Ubuntu    | 22.04 LTS                         |
| Docker    | CE 原生 (非 Desktop)              |
| g++       | 11.4.0                            |
| CMake     | 3.22.1                            |
| tcpdump   | 4.99.1                            |
| libpcap   | 1.10.1                            |
| valgrind  | 3.18.1                            |
| iperf3    | 3.9                               |
**WSL2 资源配置** (`.wslconfig`):
```ini
[wsl2]
memory=16GB
processors=8
swap=4GB
networkingMode=mirrored
```
**数据隔离策略**：所有开发数据存储在 `F:\WSL\Ubuntu-22.04\ext4.vhdx`，C 盘仅保留约 50MB 的 WSL2 内核文件，不写入任何项目数据。
## **时间线**
```tex
  ──  确定技术方案：WSL2 + Docker CE（原生），弃用 VMware 和 Docker Desktop
        │
        ├─ 安装 WSL2 并指定数据目录到 D 盘（后因空间不足迁移至 F 盘）
        ├─ 配置 .wslconfig（16GB 内存 / 8 核 / 镜像网络模式）
        ├─ 配置 WSL 设置 GUI（VM 空闲超时 0 / 硬件性能计数器开 / 嵌套虚拟化开）
        │
  ──  开始环境搭建
        │
        ├─ [坑 1] WSL2 启动报错 mount failed 16
        │   ├─ 尝试：wsl --shutdown + 重启 LxssManager 服务 → 无效
        │   ├─ 尝试：bcdedit 调整 Hyper-V 启动 → 无效
        │   └─ 解决：进入 BIOS，开启 SVM Mode（AMD 虚拟化）
        │
        ├─ [坑 2] WSL2 启动卡在 CheckConnection 网络检测
        │   ├─ 现象：getaddrinfo() failed: -5
        │   ├─ 根因：镜像网络模式与当前网络环境兼容问题
        │   └─ 修复：切回 NAT 模式（后续稳定后再调回 Mirrored）
        │
        ├─ 安装开发工具链
        │   └─ build-essential / g++ / gdb / cmake / libpcap-dev / tcpdump / git / valgrind / iperf3
        │
        ├─ 安装 Docker CE（原生，非 Desktop）
        │   └─ 配置：阿里云镜像加速 / 数据目录迁移至家目录
        │
        ├─ [坑 3] docker run 报错 docker.sock 不存在
        │   └─ 修复：sudo service docker start（WSL2 不会自启 Docker 守护进程）
        │
        ├─ [坑 4] hello-world 镜像拉取缓慢
        │   └─ 修复：替换镜像加速为 DaoCloud（https://docker.m.daocloud.io）
        │
        ├─ [坑 5] Nano 编辑器退不出（!q 无效）
        │   ├─ 根因：Nano 与 Vim 快捷键不同
        │   └─ 解决：Ctrl+X → Y → Enter（后续用 tee 命令直接写入文件）
        │
        ├─ [坑 6] docker compose build 找不到文件
        │   └─ 修复：手动 mkdir ~/game-server-engine 并 cd 进入后再构建
        │
  ──  Dockerfile + docker-compose.yml 构建成功
        └─ docker exec -it cpp_dev bash 进入容器，g++/tcpdump 验证通过
```
---

## 踩坑记录

### 坑 1：WSL2 启动报错 `mount failed 16`

**现象**：执行 `wsl` 后，调试控制台输出 `UtilMount:1793: mount(/dev/sdc, /distro, ext4, ...) failed 16`，系统无法启动。

**根因**：AMD 平台的 CPU 虚拟化功能在 BIOS 中被关闭。WSL2 依赖 Hyper-V，而 Hyper-V 需要硬件虚拟化支持。

**修复**：
1. 重启进入 BIOS（通常按 `Del` 或 `F2`）
2. 找到 **SVM Mode**（Secure Virtual Machine）
   - 微星主板：`OC → CPU Features → SVM Mode`
   - 华硕主板：`Advanced → CPU Configuration → SVM`
   - 技嘉主板：`Tweaker → Advanced CPU Settings → SVM Mode`
3. 设为 **Enabled**，`F10` 保存退出
4. 验证：Windows 任务管理器 → 性能 → CPU → "虚拟化：已启用"

---

### 坑 2：WSL2 启动卡在 `CheckConnection` 网络检测

**现象**：WSL2 启动时输出 `CheckConnection: getaddrinfo() failed: -5` 和 `connect() failed: 101`，系统卡在网络检测阶段无法进入 Shell。

**根因**：WSL2 的镜像网络模式 (`mirrored`) 在部分网络环境下与 Windows 防火墙或 IPv6 解析存在兼容性问题。

**修复**：
1. 将 `.wslconfig` 中的 `networkingMode` 改为 `NAT`（或通过 WSL 设置 GUI 切换）
2. 执行 `wsl --shutdown` 彻底关闭 WSL
3. 如仍有问题，管理员 PowerShell 执行：
   ```powershell
   netsh winsock reset
   netsh int ip reset all
   ```
   然后重启电脑

---

### 坑 3：WSL 设置中交换文件路径配置导致系统崩溃

**现象**：在 WSL 设置 GUI 中，将交换文件位置指定为 Ubuntu 内部路径（如 `/home/user/swap`），导致 WSL2 启动失败。

**根因**：WSL 设置中的"交换文件位置"要求填写 **Windows 路径**（如 `D:\WSL\swap.vhdx`），而非 Linux 路径。此交换文件是 WSL2 虚拟机级别的虚拟内存，与 Ubuntu 内部的 `/swapfile` 无关。

**修复**：
- 将交换文件位置**留空**，WSL2 会自动在 `%USERPROFILE%\AppData\Local\Temp\` 下管理
- 如必须自定义，在 `.wslconfig` 中用 Windows 绝对路径指定：`swapFile=D:\WSL\swap.vhdx`

---

### 坑 4：Docker CE 守护进程未启动

**现象**：在 WSL2 中安装 Docker CE 后，执行 `docker run hello-world` 报错：
```
failed to connect to the docker API at unix:///var/run/docker.sock
dial unix /var/run/docker.sock: connect: no such file or directory
```

**根因**：Docker CE 在 WSL2 中不会自动启动守护进程。与 Docker Desktop 不同，原生 Docker CE 需要手动启动 `dockerd`。

**修复**：
```bash
sudo service docker start
```

如需每次启动 WSL 自动执行，在 `~/.bashrc` 中添加：
```bash
sudo service docker start > /dev/null 2>&1
```

**注意**：Docker Desktop 与 Docker CE 不可并存。若已安装 Desktop，须二选一。游戏服务器原生开发推荐使用 Docker CE。

---

### 坑 5：Docker 镜像拉取缓慢或超时

**现象**：`docker run hello-world` 拉取镜像时卡住或超时。

**根因**：Docker Hub 的默认镜像仓库在国内访问不稳定。

**修复**：配置国内镜像加速。编辑 `/etc/docker/daemon.json`：
```json
{
  "registry-mirrors": ["https://docker.m.daocloud.io"]
}
```
然后重启 Docker：
```bash
sudo service docker restart
```

备选镜像加速地址：
- 阿里云（个人专属）：`https://<你的ID>.mirror.aliyuncs.com`
- 网易：`https://hub-mirror.c.163.com`

---

### 坑 6：Nano 编辑器无法退出

**现象**：使用 `nano` 编辑文件后，输入 `!q` 或 `:q!` 无法退出编辑器。

**根因**：`nano` 与 `vim` 是不同的编辑器，快捷键不通用。`nano` 的退出快捷键是 `Ctrl+X`。

**修复**：
- 保存并退出：`Ctrl+X` → `Y` → `Enter`
- 放弃修改退出：`Ctrl+X` → `N`

**替代方案**：使用 `tee` 命令直接写入文件，避免进入交互编辑器：
```bash
sudo tee /path/to/file <<'EOF'
file content here
EOF
```

---

### 坑 7：Docker 构建时找不到 Dockerfile

**现象**：执行 `docker compose build` 报错找不到 `Dockerfile`。

**根因**：`docker compose` 要求当前工作目录下存在 `Dockerfile` 或 `docker-compose.yml`，且目录必须已手动创建。Docker 不会自动创建项目目录或生成配置文件。

**修复**：
```bash
mkdir -p ~/game-server-engine    # 手动创建项目目录
cd ~/game-server-engine         # 进入目录
# 创建 Dockerfile 和 docker-compose.yml 后再执行构建
docker compose build
```

---

## 环境验证清单

| 检查项                         | 预期结果                       |
| ------------------------------ | ------------------------------ |
| `g++ --version`                | 11.4.0                         |
| `cmake --version`              | 3.22.1                         |
| `tcpdump --version`            | 4.99.1                         |
| `valgrind --version`           | 3.18.1                         |
| `iperf3 --version`             | 3.9                            |
| `git --version`                | 2.34.1                         |
| `docker run --rm hello-world`  | "Hello from Docker!"           |
| `docker compose version`       | v2.x.x                         |
| `docker exec -it cpp_dev bash` | 进入容器，`g++ --version` 正常 |

---

## 项目结构

```
~/game-server-engine/
├── Dockerfile
├── docker-compose.yml
├── CMakeLists.txt       (后续添加)
├── src/                 (后续添加)
│   ├── main.cpp
│   └── arp_sniffer.cpp
└── docs/
    └── pitfalls.md      (本文件)
```

常用命令

```sh
wsl --shutdown               # 关闭所有WSL实例
wsl -l -v                    # 列出已安装的WSL发行版
wsl --export Ubuntu-22.04 F:\WSL\backup.tar # 备份WSL

sudo service docker start    # 启动Docker守护进程
sudo service docker restart  # 重启Docker
docker compose build         # 构建镜像
docker compose up -d         # 后台启动容器
docker compose down          # 停止并删除容器
docker exec -it cpp_dev bash # 进入容器Shell
docker ps                    # 查看运行中的容器
docker images                # 查看本地镜像

g++ -g src/main.cpp -o main  # 编译（带调试信息）
gdb ./main                   # 调试程序
valgrind --leak-check=full ./main # 内存泄漏检测
tcpdump -i eth0 port 8080    # 抓包（容器内需特权模式）
```



---

## 参考命令速查

```bash
# WSL 管理
wsl --shutdown                                           # 关闭所有 WSL 实例
wsl -l -v                                                # 列出所有发行版

# Docker 管理
sudo service docker start                                # 启动 Docker 守护进程
sudo service docker restart                              # 重启 Docker
docker compose build                                     # 构建镜像
docker compose up -d                                     # 后台启动容器
docker exec -it cpp_dev bash                             # 进入容器 Shell
docker ps                                                # 查看运行中的容器
```

