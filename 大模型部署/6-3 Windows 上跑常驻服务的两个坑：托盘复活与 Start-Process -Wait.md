---
title: "6-3 Windows 上跑常驻服务的两个坑:托盘复活与 Start-Process -Wait"
category: 大模型部署
tags:
  - PowerShell
  - Windows
  - 进程管理
  - 幂等脚本
  - 实机部署
difficulty: 进阶
source: "2026-10-28 本机实测(Ollama 0.40.11 / Windows 11 / PowerShell 5.1)"
---

# Windows 上跑常驻服务的两个坑:托盘复活与 Start-Process -Wait

> 这两件事都不报错、都不崩溃,只是**静默地不工作**,所以专门记一笔。
> 场景背景见 [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1]]。

---

## 一、托盘进程会复活被停的服务

### 现象

「停止」脚本杀了服务进程,紧接着验证端口,**端口又开着**——但 PID 变了。

```
先杀:  PID 36792
再看:  PID 33316   ← 不是我杀的那个
```

### 根因

Windows 版 Ollama 除了服务端 `ollama.exe`,还有一个常驻托盘 **`ollama app`**。
它**监督**服务端:服务端一死,它立刻再起一个新的。

所以「杀进程 → 验证」这一步必然失败:
脚本杀完 → 托盘在几百毫秒内补了一个新的 → 脚本回读端口,看到还开着 → 报"进程没杀干净"。

### 解法:顺序不能反

```
① 先杀托盘（ollama app）
② 再杀服务端（pidfile 优先，端口反查兜底）
③ 回读验证
```

验证:只杀服务端 → 复活;先杀托盘再杀服务端 → 端口保持释放。

> 💡 判据:**进程树里凡是带 GUI 的"管理器/托盘/守护"角色,都可能具备重启自己孩子的能力。**
> 停服务前先想一遍"谁会把它拉回来"。
> 识别方法:`Get-Process | Where ProcessName -like 'xxx*'` 列出同族进程,
> 名字带空格(`ollama app`)的那个通常就是托盘。

### 启动脚本也要一起改

只改停止脚本不够:托盘还活着时,启动脚本拉起的服务可能被托盘接管或与托盘自带的实例抢端口。
本机的做法是**控制面全归脚本**——`start-ollama.ps1` 启动前也先清托盘。

---

## 二、`Start-Process -Wait` 会等子孙进程

### 现象

包装脚本跑 `powershell -File start-ollama.ps1`,结果**永久挂住**,
而那个脚本明明已经打印了 "OK: ollama serve @ :11434" 并且服务确实起来了。

### 根因

`Start-Process -Wait` 的语义不是"等这个进程",而是**「等这个进程及其所有子孙进程都结束」**。

```
包装器  ──Start-Process -Wait──>  子 powershell  ──启动──>  ollama serve
                                        (退出)                   (永不退出)
   └──────────────── 等到 ollama 退出才返回 ⇒ 永久挂住 ────────────────┘
```

### 解法:自己轮询,不用 -Wait

```powershell
$proc = Start-Process -FilePath $psExe -ArgumentList $args -PassThru -NoNewWindow `
        -RedirectStandardOutput $tmpOut -RedirectStandardError "$tmpOut.err"
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not $proc.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
if (-not $proc.HasExited) { $proc.Kill() }   # 超时兜底
```

> 💡 判据:**`-Wait` 只适合"跑完就退"的一次性命令**。
> 凡是会拉起常驻子进程的脚本,`-Wait` 就会变成一个永远等不到的等待。

---

## 三、附带坑:被继承的 stdout 句柄让文件读不出来

包装器把子进程的输出重定向到临时文件,子进程退出后再读——报:

```
The process cannot access the file '...\rh-20261028-042930.txt'
because it is being used by another process.
```

原因:常驻服务(ollama serve)继承了子进程的 stdout 句柄,**子进程退出后文件仍被占用**。

解法:用允许共享的方式打开。

```powershell
function Read-Shared([string]$path) {
    try {
        $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open,
                                     [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        $sr = New-Object System.IO.StreamReader($fs, [Text.Encoding]::UTF8)
        $t = $sr.ReadToEnd(); $sr.Dispose(); $fs.Dispose(); return $t
    } catch { return '' }
}
```

---

## 四、桌面快捷方式为什么"闪退"

三种典型成因,本机全中:

| 成因 | 表现 | 修法 |
|---|---|---|
| 最小化控制台跑完就关 | 窗口一闪就没,**成功失败都看不见** | 隐藏窗口 + 结果用消息框弹出来(`run-hidden.ps1`) |
| `-Wait` 挂死 | 点了完全没反应 | 自己轮询 `HasExited`(见第二节) |
| 托盘复活 | 明明杀了还在 | 先杀托盘(见第一节) |

**最终形态**:`run-hidden.ps1` = 隐藏窗口跑 + 捕获输出 + 消息框显示 + 日志落盘。
成功显示一行摘要,失败显示完整报错并写进 `logs\run-hidden-*.log`。

> 💡 判据:**给人用的快捷方式,失败路径必须可见**。
> 一个静默失败的脚本比没有脚本更糟——用户只会记住"点了没反应",而不是"报了什么错"。

---

## 五、幂等启停的两层检查(仍然适用)

| 层 | 防什么 |
|---|---|
| 端口探测 | pidfile 丢了/异常退出没清理,但服务还活着——此时再拉一个会撞端口 |
| pidfile + 进程名校验 | 模型**加载中**(进程活着、端口还没开)——只查端口会误判"没在运行" |

停止时**回读验证**端口与显存,不要假设 kill 发出去了就算完。

## 关联

- [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1 原生推理选型]] —— 本机的服务启停与显存账
- [[6-2 本地模型接 Agent 客户端：容量账与三道门槛|6-2 本地模型接 Agent 客户端]] —— 另一类"能连上但用不了"的坑
- 本机档案:`F:\LLM\README.md`(脚本清单与启停说明)
