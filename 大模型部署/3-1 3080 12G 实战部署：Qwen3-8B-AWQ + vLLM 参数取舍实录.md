---
title: "3-1 3080 12G 实战部署:Qwen3-8B-AWQ + vLLM 参数取舍实录"
category: 大模型部署
tags:
  - vLLM
  - 推理引擎
  - LLM
  - AI基础设施
  - 实机部署
  - AWQ
difficulty: 进阶
source: "2026-10-27 本机实机部署(WSL2 + RTX 3080 12G,CUDA 13.1,vLLM 0.31.0)"
---

# 3080 12G 实战部署:Qwen3-8B-AWQ + vLLM 参数取舍实录

> **这篇和 [[2-1 vLLM 参数速查表|参数速查表]] 的分工**:速查表回答「每个参数是什么、默认值多少、通用调优方向」;
> 这篇回答「**一次真实部署里,每个位置我实际选了什么、放弃了哪个相似选项、为什么**」。
> 通用原理不重复,一律链接过去。

## 最终启动脚本

```bash
#!/bin/bash
source /root/vllm-env/bin/activate
export CUDA_HOME=/root/vllm-env/lib/python3.10/site-packages/nvidia/cu13
export PATH=$CUDA_HOME/bin:$PATH
export VLLM_USE_FLASHINFER_SAMPLER=0
export VLLM_ATTENTION_BACKEND=FLASH_ATTN
exec vllm serve /root/models/Qwen3-8B-AWQ \
  --served-model-name qwen3-8b \
  --host 0.0.0.0 \
  --port 8000 \
  --gpu-memory-utilization 0.88 \
  --max-model-len 16384 \
  --reasoning-parser qwen3
```

环境:RTX 3080 12G(sm_86)/ WSL2 mirrored 网络 / 驱动 591.86 / vLLM 0.31.0 + PyTorch 2.13(CUDA 13)/ 模型 Qwen3-8B-AWQ(ModelScope 下载,约 5.7G)。实测显存占用 11.8G,单请求约 40-60 tok/s。

---

## 一、模型选型:8B AWQ,放弃了什么

| 候选 | 显存 | 为什么没选 |
|---|---|---|
| 7B 原生 FP16 | ~15G 权重都放不下 / 7B 也近 15G | 12G 卡放 16G 权重直接 OOM;**量化在这里不是优化,是能不能跑的门槛**。呼应速查表那句话:「所有优化手段里提升最明显的方法,没有之一」 |
| 14B AWQ 4bit | ~9.5G 权重 | 能塞进 12G,但 KV Cache 只剩 ~1.5G,16k 上下文都开不满,并发能力归零。**能力上限换来的显存没有变回可用容量** |
| **8B AWQ 4bit(选)** | ~6G 权重 | 留出 ~5G 给 KV Cache,上下文和并发都有余量 |

> 💡 判据:**AWQ 量化的意义在 12G 卡上不是"省显存换速度",而是"把权重压到权重的 1/4,把省下的空间还给 KV Cache"**。KV Cache 才决定上下文长度和并发数。

## 二、用了的参数:每一个为什么是它

### `--gpu-memory-utilization 0.88`(官方默认 0.92)

- **为什么改小**:这台机器有桌面输出,Windows/WSL 图形栈常驻占了 ~1.1G。启动时 vLLM 检查「空闲显存(10.84G)≥ 配额(0.92×12=11.04G)」,0.92 **直接启动失败**(严格按启动时瞬时空闲值判,不是按总显存判)。
- **放弃了什么**:保持 0.92。相似参数 `--kv-cache-memory-bytes`(手动指定 KV 字节数、并**忽略** utilization)不用——它解决的问题是我**精确知道** KV 预算该多大,而我的问题只是"给桌面留 1G 余量",降比例更简单直接。
- **失效条件**:换无桌面输出的服务器卡(如纯计算 4090)→ 恢复 0.92。

### `--max-model-len 16384`(不设 = 模型上限 auto)

- **为什么设**:Qwen3 原生 32k+。上下文越长,KV Cache 预算被"潜在单请求"占得越多——池子要按最长请求预留块。16k 对中文对话/助手场景绰绰有余,还能**直接拒掉超长请求**防止单请求吃满池子。
- **放弃了什么**:保持 auto。auto 的语义是"模型上限",不是"业务上限"——速查表出入说明 #4 说的就是这件事。

### `--reasoning-parser qwen3`

- **为什么用**:Qwen3 是混合思考模型,输出里带 `<think>...</think>`。配了 parser,思考过程进 API 响应的 `reasoning` 字段、正文进 `content`,客户端不用自己解析标签;配 `chat_template_kwargs.enable_thinking` 还能按请求关思考。
- **放弃了什么**:不配 parser 裸跑——那样 `<think>` 原文混在 content 里,调用方得自己处理,等于把解析成本转嫁给每个客户端。

### `--served-model-name qwen3-8b` / `--host 0.0.0.0` / `--port 8000`

- 模型名:`--served-model-name` 让 API 里的名字与本地目录路径解耦,客户端不用感知 `/root/models/...` 这种路径。
- `--host 0.0.0.0` 而非默认 `127.0.0.1`:本机 WSL2 是 **mirrored 网络模式**,实测 `127.0.0.1:8000` 在 Linux 侧连接被拒(内核明明在 LISTEN),只有 WSL eth0 IP(`192.168.1.5`)可达。绑 0.0.0.0 保证任何接口都能连。
- 端口 8000:与 Windows 侧保留端口范围(50000-50059)无冲突,已核对。

## 三、没用 / 没传的参数:为什么不用这些相似的

| 参数 | 默认行为 | 为什么这次不用 |
|---|---|---|
| `--quantization` | `auto`,自动识别 | **AWQ 模型 config 里自带量化信息,自动识别就是正确答案**。手动传 `--quantization awq` 只在原版模型现场压缩时才有意义。传了也是同一个行为——属于"无害但无信息量"的冗余 |
| `--kv-cache-dtype fp8` | `auto`(跟模型精度) | **单用户场景 KV Cache 根本没吃紧**(16k 上下文下池子余量充足),fp8 的收益"token 容量翻倍"无从兑现;且 Ampere(sm_86)上 fp8 支持不完整,白冒精度风险 |
| `--max-num-seqs` | 小显存档 256 | 默认 256 已远超单机单人用法。速查表说"显存够但 util 上不去才动它"——我没这个症状 |
| `--max-num-batched-tokens` | 2048 | 8192+ 是**吞吐**场景的建议值。单请求对话不构成吞吐瓶颈,2048 反而让输出更流畅(设大→首字快,设小→更流畅) |
| `--enable-chunked-prefill` | 模型支持则默认开 | 默认已开,显式传是冗余 |
| `--enable-prefix-caching` | 视版本默认 | 单轮对话为主,提示词复用率低,命中率上不去;且要求"固定内容在前"会约束提示词写法,约束换不来的收益 |
| `--block-size` | 版本默认 | 16 的默认值在 PagedAttention 下空洞率可忽略,没有证据支持要动它 |
| `--tensor-parallel-size` / `--pipeline-parallel-size` | 1 | **单卡,TP/PP 无从谈起**。放着是为了记录:将来加卡时,PCIe 家用平台 TP2 效率 ~87%,优先 TP 不做 PP(同主机做 TP,跨主机才 PP) |
| `--cpu-offload-gb` | 0 | "自己体验还行,正经服务扛不住"。12G 卡如果哪天真要跑 14B,正确解是换更小量化档,不是借内存 |

> 💡 **没传 ≠ 没生效**。vLLM 几乎所有参数都有合理默认,一次好的部署不是"把参数堆满",而是**只改默认值与事实冲突的那几项**(本例:0.92 撞上桌面显存、auto 撞上业务上限、flashinfer 撞上 CUDA 版本错配)。

## 四、环境变量:三个非 vLLM 参数,却决定成败

这是本次部署最大的坑,值得单独立节。

**故障链**:vLLM 0.31 把 flashinfer 当**硬依赖**(sampler 初始化路径直接 `import flashinfer`,ModuleNotFoundError 不被捕获)→ flashinfer 首次运行要 JIT 编译 CUDA kernel → 编译失败:`#error "CUDA compiler and CUDA toolkit headers are incompatible"`。

**根因**:pip 装的 CUDA 组件里,nvcc 是 **13.4.92**,但 runtime 头文件是 **13.0.x**——同一目录里编译器和头文件版本错配。

**考虑过的三个方案**:

| 方案 | 结果 | 为什么不用 |
|---|---|---|
| ① apt 装 CUDA toolkit | ❌ | Ubuntu 22.04 官方源只有 CUDA 11.5,太老;NVIDIA 源要下好几个 G,为了绕过一个 JIT 直接装全套 toolkit 本末倒置 |
| ② 把 runtime 头文件升级到 13.4 对齐 nvcc | ⚠️ 未验证 | pip 上 nvidia-\* 包版本号不保证逐一对齐,要试错;万一成功也是修好 JIT,flashinfer 的收益(sampler)对单用户场景仍然可忽略 |
| **③ 禁用 flashinfer(选)** | ✅ | 环境变量 `VLLM_USE_FLASHINFER_SAMPLER=0`(查源码确认它会在触发 flashinfer 导入**之前**短路返回)+ `VLLM_ATTENTION_BACKEND=FLASH_ATTN`(强制用 vLLM 自带的 FlashAttention 后端)。**单用户场景 flashinfer 的收益本来就可忽略,用自带实现损失最小、确定性最高** |

**两个反直觉的细节**:

1. **flashinfer-python 包不能卸载**。先卸了它再启动,直接 `ModuleNotFoundError: No module named 'flashinfer'`——vLLM 0.31 硬依赖包存在。正确姿势是**包保留、运行时禁用**。还得精确装回 `0.7.0.post1`(镜像源会装新版 0.7.2,vLLM 的 `pip check` 直接报不兼容)。
2. `CUDA_HOME` 指向 pip 组件目录(`/root/vllm-env/lib/python3.10/site-packages/nvidia/cu13`)是当初为了救 flashinfer JIT 加的——禁用 flashinfer 后它已无实际作用,保留只是防御性配置(万一有其他组件找 nvcc)。

**失效条件**:将来 vLLM/flashinfer 修复 CUDA 13 对齐,或换装版本一致的原生 toolkit 后,`VLLM_ATTENTION_BACKEND=FLASH_ATTN` 这个强制项可以去掉(让 vLLM 自动选);`VLLM_USE_FLASHINFER_SAMPLER=0` 若 flashinfer sampler 有实测收益也可恢复。

## 五、踩坑速记

| 坑 | 现象 | 解法 |
|---|---|---|
| 桌面显存吃掉配额 | `Free memory 10.84/12 GiB less than desired GPU memory utilization (0.92, 11.04 GiB)` 启动即失败 | 0.92 → 0.88 |
| flashinfer JIT 失败 | `#error CUDA compiler and CUDA toolkit headers are incompatible` | 见第四节方案③ |
| WSL2 mirrored 回环不通 | 内核 LISTEN 明确存在,`127.0.0.1` 仍 Connection refused;**WSL eth0 IP 可达**,Windows 侧 `localhost` 也不通 | 统一用 `http://192.168.1.5:8000` 访问;要 Windows localhost 需放开 Hyper-V 防火墙入站 |
| HuggingFace 不通 | `curl https://huggingface.co` 超时 | 改用 ModelScope:`modelscope download --model Qwen/Qwen3-8B-AWQ` |

## 六、启停脚本:写法与原理

部署完总要有人管进程。目标:**Windows 桌面双击即启/即停**。落地为两层:WSL 侧两个 `.sh` 承载全部逻辑(`/root/vllm-start.sh`、`/root/vllm-stop.sh`),Windows 桌面两个 `.bat` 只做转发(`wsl.exe -u root -e bash /root/vllm-start.sh`)——Windows 不能双击运行 `.sh`,`.bat` 是"可点击"的载体;`-u root` 显式指定,不赌 WSL 默认用户是谁。

### 为什么不能只 `kill <pid>`:vLLM 是多进程树

`vllm serve` 起来后至少两个进程:API Server(父)+ EngineCore(子,真正占 12G 显存的那个)。只杀父进程,EngineCore 可能变孤儿继续占显存——**"停止了但显存没释放"就是这么来的**。

解法是让整棵进程树可以一锅端:

```bash
setsid bash -c 'echo $$ > /root/.vllm.pid; exec vllm serve ...' &
kill -TERM -- -"$(cat /root/.vllm.pid)"   # 注意负号:杀整个进程组
```

- `setsid` 使主进程成为**新会话的组长**,于是 pidfile 里记录的 PID **同时就是 PGID**
- `kill -- -PGID`(负号)发给组内所有成员,EngineCore 无处可逃

### TERM → KILL 的 15 秒升级

先 `TERM` 让 vLLM 优雅退出(清理 NCCL 通信组、显存、socket),15 秒内没退干净再 `KILL`。反过来直接 KILL 省事,但内核直接回收、进程没有机会做清理,极端情况会留下半初始化的共享内存/显存状态。

### 幂等检查要两层,一层都会被骗

| 层 | 防什么 |
|---|---|
| `curl /v1/models` 探测端口 | pidfile 丢失(手动删过/异常退出没清理)但服务还活着——此时拉第二个实例会撞端口崩溃 |
| pidfile + `kill -0` | 模型**加载中**(进程活着、端口还没开)——只查端口会误判"没在运行"然后拉重复实例 |

这是实测踩出来的:测试时旧的前台实例还占着 8000 端口,新实例启动即崩溃,而就绪探测被旧实例"骗"过了——两层检查缺一不可。停止脚本同样有兜底:pidfile 不存在时退化为 `pkill -TERM -f "vllm serve"`。

### 其他写法对比

| 方案 | 为什么没用 |
|---|---|
| 只 `pkill -f vllm` | 依赖命令行特征匹配,PID 复用/误杀风险;且杀的是单进程,不处理进程组 |
| `nohup ... &` 不加 setsid | 后台进程仍在原进程组,组杀时会把脚本自己所在的组牵连进去;且拿不到"PID=PGID"的干净不变量 |
| WSL 内 systemd 用户服务 | 更优雅,但依赖 `/etc/wsl.conf` 开 systemd 且发行版支持;两个显式脚本对"双击"场景已够,留作升级路线 |
| Windows 侧用任务计划/服务 | 杀鸡用牛刀,`.bat` 两行就够 |

### 健壮性细节

- 就绪探测的 IP **动态取**(`hostname -I`):mirrored 模式下 WSL IP 随 Windows 主机 DHCP 变化,写死 `192.168.1.5` 迟早失效
- 启动超时 4 分钟(模型加载 1-2 分钟余量),超时提示看日志而非干等
- 停止脚本最后用 `nvidia-smi` 回读显存——**验证停止是否真的成功**,而不是假设 kill 发出去了就算完

## 关联

- [[2-1 vLLM 参数速查表|vLLM 参数速查表]] —— 本文所有参数的通用语义、默认值与调优决策树在那边
- [[1-1 vLLM 推理引擎精读|vLLM 推理引擎精读]] —— 为什么有 KV Cache、为什么要分页:本文"量化省下的空间还给 KV Cache"的原理基础
