---
title: "服务器常用命令"
category: 常用工具
tags:
  - net/practice
  - net
difficulty: 基础
source: "自整理"
---
监控
```shell
#(监控图)
btop 
---

------------------------------
# 监控指定 PID，每秒刷新一次
pidstat -p 你的PID 1
-------------------------------------
# 监控所有线程（C++ 多线程服务必用）
pidstat -p 你的PID -t 1
-------------------------------------
# 进入调试模式(会阻塞)
gdb -p 你的PID
# 进入后，常用命令（背会这 5 个就够）
bt          # 打印当前调用栈
frame 0     # 跳到最顶层函数（当前正在执行的）
p 变量名    # 打印变量的值！！（核心功能）
p *this     # 打印 C++ 对象的所有成员
quit        # 退出
-------------------------------------
#实时gpd不会阻塞(看卡在了哪)
gdb -p 你的PID -ex "set pagination off" -ex "thread apply all bt" -ex "c" -ex "detach" -ex "quit"
-------------------------------------
#直接打印进程正在执行的系统调用
strace -p 你的PID

#最常用！ 跟踪 30 秒，自动保存日志，不影响服务
strace -tt -T -p 你的PID -o trace.log & sleep 30 && kill %1
#-tt：显示时间 -T：显示每个调用耗时 -o trace.log：输出到日志 sleep 30：只抓 30 秒

#查看日志
cat trace.log

#只看接收 / 发送 / 连接，忽略无用信息
strace -p 你的PID -e trace=network
#看到这些，你就知道程序在干嘛：
#epoll_wait(...) = 1 → 正常等待客户端消息
#recvfrom(...) = 100 → 收到了 100 字节数据
#sendto(...) = 100 → 发送了 100 字节数据
#-1 EAGAIN → 非阻塞，没数据，正常
#如果一直不动，只显示 epoll_wait
#→ 说明程序空闲等待，没毛病！
#如果卡在 recvfrom/sendto 不返回
#→ 说明网络阻塞，找到问题了！
```

打包

```
cmake ..
make -j4
```

arp发送

```
arping -I eth0 -c 1 172.17.0.1
```

docerk

```
//进入docker
docker exec -it workspace-dev bash
```

仅删除远程仓库 .vscode、本地文件完全不动

```
//删除仓库
git rm -r --cached .vscode
//写入忽略文件
echo ".vscode/" >> .gitignore
echo "build/" >> .gitignore
//加入忽略文件 再次提交
git add .gitignore
git commit -m "docs: 移除远程 .vscode 配置文件夹"
git push origin master
```

查看网卡

```
ip addr
//查看网卡mac ： eth0
ip link show eth0
//查看 mac+ip eth0
ip addr show eth1
```



抓包流程（tcpdump）

```
# 如果你退出了，重新进容器
docker exec -it 你的容器名 /bin/bash
// 容器内安装抓包工具 tcpdump
apt update && apt install tcpdump -y
//确认up的网卡
ip addr
//只抓arp
tcpdump -i eth1 arp -w arp_dump.pcap
//抓所有包
tcpdump -i eth1 -w all_dump.pcap
//抓指定 IP 的包（精准抓网关 / 目标机）
tcpdump -i eth1 host 192.168.1.1 -w gateway_dump.pcap
```

ping

```
1. 本机回环地址（验证本机协议栈）
这是最基础的网络连通性测试，只要你的系统网络模块正常，就绝对能 ping 通。
- 127.0.0.1
- 127.0.0.2 （整个 127.0.0.0/8 网段都是回环地址，都可以 ping 通）

2. 局域网网关地址（验证同网段 ARP 和链路层）
如果你通过 SSH 连接到这台服务器，那么服务器的默认网关绝对是通的。你可以通过在终端输入 ip route 或 route -n 查看，第一行通常就是默认网关（Gateway）。常见的形式为：
- 192.168.1.1
- 10.0.0.1
注：ping 网关地址会在 eth1 上触发 ARP 请求和响应，非常适合用来测试你代码中对以太网帧头（14字节）和 IP 头的解析是否正确。

3. 公共 DNS 服务器（验证外网连通性）
- 223.5.5.5 （阿里云 DNS，国内延迟极低）
- 114.114.114.114 （国内老牌公共 DNS）
- 8.8.8.8 （Google DNS，国内可能偶尔有丢包，但基本通）
- 1.1.1.1 （Cloudflare DNS）

4. 自身 eth1 的 IP 地址（验证自身网卡）
让服务器 ping 自己 eth1 的 IP 地址也是绝对能通的，且流量会经过 eth1。
你可以通过在终端输入 ip addr show eth1 查看其 IP 地址（例如 192.168.1.100），然后直接 ping 它。

编程测试建议
结合你之前的 ip_parser.cpp 代码，建议你按以下顺序测试：
1. 先 ping 网关（如 ping 192.168.1.1）：观察 eth1 上的数据包，这会产生标准的 ICMP Echo Request/Reply，你的程序应该能正确解析出协议字段为 1（ICMP）。
2. 再 ping 公网（如 ping 223.5.5.5）：这会产生经过 NAT 转发出去的包，你可以验证 TTL 递减等 IP 头部字段的变化。
3. 最后测试 TCP/UDP：虽然 ping 只产生 ICMP 包，但如果你想抓取 TCP 包来验证协议字段为 6，可以直接使用 curl baidu.com 或 wget，这样 eth1 上就会捕获到完整的 TCP 三次握手 IP 数据包。

```

wsl+docker

```
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


#重新构建docker
cd ~/java_project
docker compose down #关闭
docker rmi -f workspace-dev:latest #删除
docker compose up -d --build #构建
docker compose exec workspace-dev bash #重启
```

