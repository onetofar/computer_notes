---
title: "WSL2+Docker+VSCode 全链路 ARP 发包抓包"
category: 计算机网络
tags:
  - net/link
  - net/practice
  - networking
difficulty: 进阶
source: "自整理"
link: ["[[1-2 arp笔记|ARP协议]]"]
---
# WSL2+Docker+VSCode 全链路 ARP 发包/抓包
## 踩坑全记录 + 终极落地方案
**本文完全基于今日真实实操流程：环境隔离选型 → IDE 弃坑 → 网络模式切换 → 代码修复 → 抓包验证 全闭环**
## 一、文档背景
本次作业目标：基于 `libpcap` 实现 **ARP 请求发送 + 响应捕获 + 协议解析**，全程追求**环境隔离、线上线下一致、可复现**。
从 WSL2 本地运行 → 引入 Docker 做环境隔离 → 踩坑 IDE/网络/代码/抓包 全环节 → 最终以「VSCode 直连 Docker + WSL2 Mirrored + tcpdump 抓包 + 代码自动获取 IP/MAC」完美落地。
## 二、核心目标
1. 使用 **Docker** 实现环境 100% 隔离，避免本地依赖污染
2. 选用稳定 IDE，实现**开发环境 = 线上运行环境**
3. 解决 WSL2 网络限制，实现 ARP 广播正常收发（请求 + 响应）
4. 代码去掉硬编码，**自动获取本机 IP/MAC**，适配任意网卡
5. 实现 Docker 内抓包 → 导出文件 → Windows Wireshark 可视化分析
---

## 三、最终稳定架构（最终版）
```
Windows 11
└─ WSL2（Ubuntu 22.04）
   ├─ 网络模式：Mirrored（镜像模式，解决 ARP 隔离）
   ├─ Docker 容器（环境完全隔离，编译/运行均在容器内）
   │   ├─ VSCode 直接连入容器（开发 = 运行，无差异）
   │   ├─ libpcap：ARP 发包/抓包
   │   ├─ tcpdump：容器内抓包保存为 pcap
   │   └─ 自动获取本机 IP/MAC（无硬编码）
   └─ 导出 pcap → Windows Wireshark 打开分析
```

---

## 四、全流程踩坑实录（按今日真实顺序）
### 阶段 1：为环境隔离，选择 Docker
- **初衷**：统一依赖、避免本地环境混乱、保证线上线下一致
- **初始方案**：WSL2 内运行 Docker，把编译/运行全部放进容器

---

### 阶段 2：CLion 彻底弃坑（致命问题）
- **使用场景**：想用 CLion 本地编译，调用 Docker 内的工具链（g++/libpcap）
- **现象**
  1. 本地编译时，CLion 强制创建临时 `tmp` 目录
  2. 临时目录无法映射到 Docker 真实环境
  3. **运行结果不可复现**，本地能跑、容器跑不通
- **根因**
  CLion 是**本地编译 + 远程工具链**模式，不是“完全进入容器开发”，环境无法对齐
- **最终决策**：直接舍弃 CLion，换 **VSCode 直连 Docker 容器**

---

### 阶段 3：VSCode 配置大半天（编译/调试全踩坑）
- **目标**：VSCode 进入容器，实现容器内一键编译、调试
- **坑点清单**
  1. 路径混乱：`${fileDirname}` vs `${workspaceFolder}`
  2. 编译产物乱放：源码和可执行文件混在一起
  3. 调试无 root 权限：libpcap 打不开网卡
  4. 任务与调试不匹配：F5 无法自动编译
- **最终解决**
  - 统一编译到 **项目根目录 /build**
  - `tasks.json` 自动创建 build 文件夹
  - 容器内默认 root，调试直接运行，无需 sudo 配置
  - 调试配置直接指向容器内的可执行文件

---

### 阶段 4：WSL2 NAT 模式 → ARP 只有请求、没有响应
- **现象**
  1. ARP 请求能发出去
  2. 抓包能看到请求
  3. **完全收不到 ARP 响应**
- **根因**
  WSL2 默认 NAT 模式会**隔离 ARP 广播**，响应包被 NAT 拦截，无法回到容器/WSL
- **结论**：NAT 模式不能做 ARP 实验

---

### 阶段 5：切换 WSL2 Mirrored 模式 → 网络彻底打通
- **操作**
  1. 新建 `.wslconfig` 开启 `networkingMode=mirrored`
  2. `wsl --shutdown` 重启
- **效果**
  1. WSL/Docker 直接使用 Windows 物理网卡
  2. ARP 广播正常收发
  3. **请求 ↔ 响应全通**
- **副作用**
  网卡名从 `eth0` → `eth1`，代码/启动参数必须同步修改

---

### 阶段 6：Windows Wireshark 抓不到 Docker/Mirrored 流量
- **现象**
  包发在 Docker 内、走 Mirrored 虚拟栈，Windows Wireshark 看不到物理网卡之外的流量
- **最终方案**
  容器内使用 **tcpdump** 抓包并保存为 `.pcap` 文件
  复制到 Windows → Wireshark 直接打开分析
  （业内标准 Docker 抓包方案）

---

### 阶段 7：代码硬编码致命坑（发包无效、无响应）
- **现象**
  代码写死 MAC/IP，换环境/换网卡就失效，目标设备不回包
- **根因**
  Mirrored + Docker 环境下，本机 MAC/IP 会变化，硬编码完全不匹配
- **修复**
  代码增加 **自动获取本机 IP + MAC** 函数
  不写死任何地址，适配任意网卡

---

## 五、最终落地：所有问题的终极解
今日最终稳定方案（缺一不可）：
1. **Docker 做环境隔离**（编译/运行全在容器）
2. **VSCode 直连 Docker 容器**（开发 = 线上，完全一致）
3. **WSL2 切 Mirrored**（ARP 广播无隔离）
4. **代码自动获取 IP/MAC**（无硬编码，通用）
5. **容器内 tcpdump 抓包 → 导出 Windows 分析**

---

## 六、最终稳定配置（可直接复用）
### 1. VSCode tasks.json（容器内编译到 build）
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "C/C++: g++ 编译当前文件",
            "type": "shell",
            "command": "mkdir -p ${workspaceFolder}/build && g++ -g ${file} -o ${workspaceFolder}/build/${fileBasenameNoExtension} -lpcap",
            "options": {
                "cwd": "${workspaceFolder}"
            },
            "problemMatcher": ["$gcc"],
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

### 2. VSCode launch.json（容器内调试）
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "C++ 调试",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/${fileBasenameNoExtension}",
            "args": ["eth1", "192.168.1.1"],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "/usr/bin/gdb",
            "preLaunchTask": "C/C++: g++ 编译当前文件",
            "setupCommands": [
                {
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ]
        }
    ]
}
```

### 3. Git 清理（删除远程 .vscode，本地保留）
```bash
git rm -r --cached .vscode
echo ".vscode/" >> .gitignore
echo "build/" >> .gitignore
git add .gitignore
git commit -m "clean: 移除远程 .vscode & build"
git push
```

---

## 七、完整运行流程（今日最终版）
1. VSCode 直接连接 Docker 容器
2. 打开 ARP 代码
3. Ctrl+Shift+B 编译到 build
4. 运行抓包：`./build/arp_sniffer`
5. 运行发包：`./build/arp_sender eth1 192.168.1.1`
6. 容器内抓包：`tcpdump -i eth1 arp -w arp.pcap`
7. 复制到 Windows：`cp arp.pcap /mnt/c/Users/xxx/Desktop/`
8. Wireshark 打开查看

---

## 八、今日核心总结（最值钱的经验）
1. **Docker 网络隔离** 是优势，但对 ARP 必须用 `Mirrored` 才能通
2. **CLion 不适合 Docker 全容器开发**，tmp 目录无法复现环境
3. **VSCode 直连容器** 才是真正的“开发 = 线上”
4. **WSL2 NAT 不能做 ARP**，必须 Mirrored
5. **网络编程永远不要硬编码 IP/MAC**
6. **Docker 内抓包最佳方案：tcpdump → 导出 pcap**
7. 环境配置一天不是浪费，是**把所有底层坑一次性踩完**

---

## 九、最终结论
今日从 0 → 1 完成：
**环境隔离（Docker）+ IDE 稳定（VSCode 直连）+ 网络通（Mirrored）+ 代码通用（自动获取）+ 抓包可验证（tcpdump）**
是一套**可复现、可迁移、线上线下一致**的标准 C++ 网络开发环境。