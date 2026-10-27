---
title: "4-1 Windows 原生推理选型:为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM"
category: 大模型部署
tags:
  - Ollama
  - llama.cpp
  - Qwen3
  - 本地推理
  - Windows原生
  - 显存预算
  - 实机部署
difficulty: 进阶
source: "2026-10-28 本机实机部署(Windows 原生 / RTX 3080 12G 整卡 / Ollama 0.40.11 / 驱动 591.86)"
---

# Windows 原生推理选型:为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM

> **这篇打的是 [[3-1 3080 12G 实战部署：Qwen3-8B-AWQ + vLLM 参数取舍实录|3-1]] 留下的那个死结**:
> 3-1 里 14B 被判死刑——"能塞进 12G,但 KV Cache 只剩 ~1.5G,16k 上下文都开不满"。
> 这篇记录**同一个 12G 卡上,14B 后来怎么变成 100% GPU、20K 上下文、82 tok/s** 的,以及为什么引擎从 vLLM 换成了 Ollama。
> vLLM 原理不重复,一律链接 1-1 / 2-1 / 3-1。

## 结论先行

三件事同时变了,缺一件 14B 都上不了卡:

| 变化 | 3-1 的状态 | 现在 | 释放的显存 |
|---|---|---|---|
| 桌面输出 | 在 3080 上,占 ~1.1G | **搬到核显**,3080 基线只剩 **151 MiB** | ~0.9G |
| KV Cache 精度 | auto(= FP16) | **q8_0**(实测生效,日志见下) | 同上下文下约省一半 |
| 并发预留 | 按 256 seq 预留池子 | **单 seq**(`NUM_PARALLEL=1`) | 池子不用按最坏情况留 |

叠加结果:**14B Q4_K_M 权重 8.38 GiB + KV(q8) 2.6G + 计算缓冲 ≈ 10.4G,41/41 层全上卡**。

> 💡 判据:**12G 卡上"能不能跑 14B"不是模型问题,是桌面显存 + KV 精度 + 并发假设三个旋钮的问题**。3-1 的结论在当时是对的,它的失效条件是"桌面不再占这张卡"——现在这个条件满足了。

---

## 一、引擎选型:为什么是 Ollama

### 关键前提:vLLM 已经不在选项里

vLLM **没有原生 Windows 支持**。要用它就必须留在 WSL2 里,而 WSL 已被销毁(见 [[4-2 WSL 退役与 Linux 迁移：开发环境搬去 Fedora，推理留在 Windows|4-2]])。所以这不是"Ollama 比 vLLM 好",而是**引擎必须能原生跑 Windows**。

### 候选对比

| 候选 | 为什么没选 |
|---|---|
| **vLLM** | 无原生 Windows 支持;且它优化的是**多请求并发吞吐**(PagedAttention / 连续批处理都是为批设计的)。单用户单流场景,它的本事用不上,反而要为批处理牺牲单流延迟 |
| **llama.cpp 直跑** | 实测可行:8B Q6_K、ctx 32768、100% GPU、**104.5 tok/s**。但缺模型管理面——多模型切换、别名、采样参数固化都得自己写脚本。最终留作备用(`F:\LLM\bin\runtime\`) |
| **Ollama(选)** | 底层 runner **就是 llama.cpp**(`ollama ps` 的 RUNNER 列实测显示 `llamacpp`),所以**不是拿性能换便利**,是在同一个后端上加了模型管理面(Modelfile 固化参数、`ollama create` 导入本地 GGUF、别名、模型列表) |
| **LM Studio** | GUI 为主,CLI 弱;日常是命令行工作流,不需要一个带界面的壳 |
| **TabbyAPI / exllamav2** | 吞吐向方案,Windows 支持差;单用户不需要 |

> 💡 判据:**选 Ollama 不等于放弃 llama.cpp 的性能**——`ollama ps` 的 RUNNER 列直接证明它跑的是 llamacpp。多出来的只是"参数写在哪里"和"模型怎么注册"。

### 实测速度对照(同卡,非同条件,看方向不看倍数)

| 方案 | 模型档 | 上下文 | 速度 |
|---|---|---|---|
| vLLM(3-1 记录) | 8B **AWQ 4bit** | 16384 | 40-60 tok/s |
| llama.cpp 直跑 | 8B **Q6_K** | 32768 | 104.5 tok/s |
| Ollama | 8B **Q6_K** | 32768 | 112.4 tok/s |
| Ollama | 14B **Q4_K_M** | 20480 | 82.1 tok/s |

⚠️ **这不是同条件对照**:量化档不同(AWQ 4bit vs Q6_K)、引擎不同、上下文不同。
唯一能下的结论是:**单流延迟上 llama.cpp 系明显更高**;反过来在**多并发吞吐**上 vLLM 仍会赢——两者的设计目标本就不同。别拿这张表去推"vLLM 慢"。

---

## 二、模型选型:14B 是 12G 整卡的天花板

| 候选 | 实测/推算 | 判定 |
|---|---|---|
| **Qwen3-14B Q4_K_M(选)** | 权重 8.38 GiB,总占 **10442 MiB**,**100% GPU**,82.1 tok/s | 质量最高档,还能全上卡 |
| Qwen3-8B Q6_K(保留) | 权重 6.26 GiB,总占 8986 MiB,112.4 tok/s | 快 37%,质量低一档;留作大批量改写/补全 |
| 14B Q5_K_M 及以上 | 权重约 9.8 GiB 起 | KV 只剩不到 1G,20K 上下文开不满,回到 3-1 那个死结 |
| 20B / 30B / 32B | 权重远超 12G | 必须 CPU 卸载 → 卸载层走 PCIe,速度掉一个数量级。**不是慢一点,是换个量级** |

> 💡 判据:**12G 卡上"更大的模型"是假升级**。真正决定体验的是"权重 + KV + 缓冲"能不能同时留在卡里;留不住的层不是"稍慢",而是被 PCIe 带宽接管。

### 上下文上限是实测出来的,不是猜的

Ollama 按保守显存预算决定上卡层数,**越过边界不会报错,只会悄悄把几层踢回 CPU**:

| num_ctx | 上卡层数 | 判定 |
|---|---|---|
| 16384 | 41/41 | 全上卡 |
| 18432 | 41/41 | 全上卡 |
| **20480** | **41/41** | **← 定稿** |
| 21504 | 40/41 | 开始掉层 |
| 22528 | 40/41 | 7% CPU |
| 24576 | 39/41 | 9% CPU |
| 32768 | 36/41 | 16% CPU |

**判据永远看 `ollama ps` 的 PROCESSOR 列**:`100% GPU` 才算过;出现 `x%/y% CPU/GPU` 就已经掉层了。
**别看速度去感觉**——32K 时模型照样回话,只是部分层在走 PCIe,你会误以为"32K 也行"。

### KV q8_0 确实生效了(不是白设)

`OLLAMA_KV_CACHE_TYPE=q8_0` + `OLLAMA_FLASH_ATTENTION=true` 之后,runner 日志实测:

```
llama_context: flash_attn = enabled
llama_kv_cache: size = 2720.00 MiB ( 32768 cells, 40 layers, 1/1 seqs),
                K (q8_0): 1360.00 MiB, V (q8_0): 1360.00 MiB
```

`K (q8_0)` / `V (q8_0)` 是硬证据——这两个字母不出现就说明没生效,该去查环境变量有没有传给服务端进程。

---

## 三、踩坑速记

| 坑 | 现象 | 解法 |
|---|---|---|
| **上下文超界不报错** | 32K 能跑,但 `ollama ps` 显示 `16%/84% CPU/GPU`,部分层已回 CPU | 按层数二分找边界,本例 20480 |
| **`keep_alive` 传不进 API** | API 里 `keep_alive: "-1"` → **HTTP 400** | 只能服务端设 `OLLAMA_KEEP_ALIVE=-1` |
| **默认 5 分钟卸载模型** | 空闲几分钟后每个请求白等 ~20s 重加载 | 同上,服务端 `KEEP_ALIVE=-1` |
| **`ollama create` 会复制权重** | 源 GGUF 14.6 GB + blob 14.6 GB,磁盘翻倍 | 导入是**复制**不是引用;源文件删掉会失去可重放性,自己权衡 |
| **PowerShell 5.1 的 BOM 双向坑** | 中文 .ps1 不带 BOM → 按 GBK 解码解析失败;发 API 的 JSON **带** BOM → 服务端报 `invalid character '﻿'` | 脚本要 BOM;JSON body 必须无 BOM(`[IO.File]::WriteAllText` + `UTF8Encoding($false)`) |
| **`/v1` 的思考字段名** | Qwen3 的思考内容在 `message.reasoning`,**不是** `reasoning_content` | 客户端只认后者时 content 为空——其实是思考吃光了 `max_tokens`;给足 max_tokens 或提示词加 `/no_think` |
| **`think=false` 在 `/v1` 无效** | 只有 `/api/chat` 认这个参数 | `/v1` 改用提示词 `/no_think`(实测 6 tokens 答完) |

---

## 四、当 Agent 用:闭环实测

只验"`tool_calls` 字段存在"是不够的——那只能证明它会说话,不能证明它能干活。完整闭环:

```
TURN 1  finish_reason: tool_calls        8.2s
        list_dir({"path": "F:\\LLM\\models"})
本地真实执行 → {"total_bytes":15727652000, "total_gb":14.65, "files":[...]}
TURN 2  finish_reason: stop              8.8s
        「…总大小为 14.65 GB」            ← 数字来自工具返回,不是编的
PASS    总耗时 16.9s
```

脚本:`F:\LLM\scripts\agent-smoke.ps1`(失败非 0 退出,可当回归门禁)。

### 一个真实翻车点:别让模型做单位换算

工具返回 `{bytes: 9001752960, total_gb: 14.65}`,模型把 **total 原样引用对了**,但**自己心算单个文件的 GB 两次都算错**(说 8.58 / 6.32 / 6.41 GB,实际 9.0 GB 十进制 / 8.38 GiB)。

> 💡 工具设计原则:**服务端算好、带上单位再返回**,别指望模型做 bytes→GB 除法。改成 `{name, bytes, gb}` 三字段,模型只负责转述。

### MCode 接入的硬性细节

| 项 | 值 | 为什么 |
|---|---|---|
| `baseURL` | `http://127.0.0.1:11434/v1` | **必须带 `/v1`**;只写 `:11434` 会打到 Ollama 原生 API,路径不匹配 |
| `apiKey` | 任意占位(`ollama`) | Ollama 不校验,但客户端通常要校验"有没有 key";实测 `Bearer ollama` → 200 |
| `attachment` | `false` | Qwen3 是纯文本模型,没有视觉塔;开了传图直接报错 |
| `limit.context` | **20480** | 必须与实测边界一致;写大不报错,只会悄悄变慢 |

---

### 意外发现:Ollama 原生支持 Anthropic Messages 协议

除了 OpenAI 兼容的 `/v1/chat/completions`,Ollama 0.40.11 **还实现了 `/v1/messages`**(Anthropic 格式)。实测:

```
POST /v1/messages  (anthropic-version: 2023-06-01)
→ 200 {"type":"message","role":"assistant","content":[{"type":"thinking",...}],"stop_reason":"max_tokens",...}
```

带 tools 再测:

```
stop_reason: "tool_use"
block type=tool_use  name=list_dir  input={"path":"F:\\LLM\\models"}
```

⇒ **Claude Code 这类只讲 Anthropic 协议的客户端可以直连 Ollama,不需要 claude-code-router / LiteLLM 之类的转译层。**
配置就是把 `ANTHROPIC_BASE_URL` 指向 `http://127.0.0.1:11434`(客户端自己拼 `/v1/messages`),token 填任意占位值。

> 💡 这条改变了引擎选型的成本计算:之前"用 vLLM 就得留在 WSL"的隐含理由是协议与平台绑在一起;现在**一个原生 Windows 的引擎同时说两种协议**,换客户端的边际成本从"加一层代理"降到"改一个环境变量"。
> 注意 `/v1/chat/completions` 的思考字段叫 `reasoning`,而 `/v1/messages` 里是标准 Anthropic 的 `thinking` 块——两套协议各说各的,别混。



### 真正的门槛:MCode 的固定基线,和压缩救不了它

前面验证的闭环是**手写脚本**发的请求(几十个 token 的提示)。真接到 MCode 里跑,第一条消息就失败了:

```
BYOK provider custom_provider:ollama-local upstream error:
400 request (32689 tokens) exceeds the available context size (20480 tokens)
```

会话标题是「你好」——**用户还没说话,基线就已经 32689 tokens**。随后连报 6 次:

```
compaction_failed: "Generated checkpoint does not fit the next Provider request."
```

**为什么压缩救不了**:压缩只能把「对话」压小,**压不掉基线**。压缩请求本身也要把 `基线 + 对话` 发给模型;摘要生成后,`基线 + 摘要` 仍超限 → 判定装不进 → 重试 → 放弃。所以现象是"上下文压缩都不行",但病灶在基线,不在压缩。

基线的构成——**这是 MCode 自己记录的实测值,不是估算**(关掉 aseprite 等 MCP 之后的一次真实请求):

| 构成 | tokens | 说明 |
|---|---|---|
| **TOOLS** | **17,028 – 17,680** | 关 MCP 前约 3.2 万,关完仍 1.7 万(内置工具 + 未关的插件) |
| **MEMORY** | **9,590 – 9,958** | 用户画像 + 累积记忆条目,**和任务无关但每次都要带** |
| SYSTEM_PROMPT | 2,866 – 2,976 | |
| SKILLS | 793 – 823 | |
| MESSAGES | 3,198 → 5,954 | 随对话增长 |
| **固定基线合计** | **≈ 30,300** | |

于是压缩能不能成功,取决于一道很窄的算术:

```
40960(上下文) − 30300(基线) = 10660
10660 − 4096(output 预留)   = 6564   ← checkpoint + 保留消息的全部空间
```

`output` 预留在模型配置里,最初写成 **8192** 时这道式子变成 `40960−8192=32768 < 基线`,**任何 checkpoint 都不可能装下**——这是"压缩永远失败"最直接的一个原因。改成 4096 后才腾出 6.5K。

> 💡 两个反直觉点:
> ① **关 MCP 只砍掉了一半 TOOLS**,剩下的是内置工具与插件,MCP 不是唯一大头;
> ② **MEMORY 占 9.6K**,它是纯固定成本,不随任务变化——精简记忆条目是比关 MCP 更直接的杠杆。


### 40960 才是 Qwen3 的天花板,再往上没意义

实测请求 `num_ctx=65536` 与 `49152`,服务端日志:

```
WARN "requested context size too large for model" num_ctx=65536 n_ctx_train=40960
llama_context: n_ctx = 40960
```

**两个上下文请求都返回了完全相同的 9636 MiB / 37/37 层** —— 被钳到模型原生 `n_ctx_train = 40960`。所以:

| 配置 | 上下文 | 上卡 | 速度 | 显存 |
|---|---|---|---|---|
| 8B Q6_K @ 40960 | **40960(上限)** | **37/37 全上卡** | **112.8 tok/s** | 9636 MiB |
| 14B Q4_K_M @ 40960 | 40960(上限) | 34/41 掉 7 层 | 27.8 tok/s | 10382 MiB |

⇒ **MCode 场景必须用 8B**:14B 换不来任何上下文(天花板相同),只把速度砍到 1/4。

掉的那 7 层去哪了,日志给了确切数字:14B @ 40960 时 `CUDA_Host model buffer = 1773.91 MiB`——
**这 1.73 GiB 权重搬到了系统内存,且每个 token 都要过一次 PCIe**。对照 8B 全上卡时仅 `486.86 MiB` 在内存。
三级存储(磁盘/内存/显存)的分工见 [[5-1 模型文件名怎么读：Qwen3-14B Q4_K_M 逐段拆解|5-1 模型文件名怎么读]]。

### 这笔账要算清楚

```
40960 − 基线 30300 = 10660
10660 − output 预留 4096 = 6564   ← checkpoint + 保留消息的全部空间
```

6.5K 装 checkpoint 装不下,于是**每轮都触发压缩、每次压缩都失败、会话卡死**。这就是"压缩都不行"的完整因果。

三条出路,按性价比排:

1. **改 `output` 预留**(已做,8192 → 4096):光这一项就把可用空间从 `32768−30300=2468` 拉回到 6564。**这是最便宜的一刀,而且是最容易被忽略的**——没人会想到"输出预留"会决定"压缩能不能成功"。
2. **裁 MEMORY(9.6K)**:精简记忆条目,比关 MCP 更直接(纯固定成本,不随任务变化)。
3. **继续裁 TOOLS(17K)**:MCP 关完仍有 1.7 万,剩下的在**内置工具与插件**里,不是 MCP 一家的事。
4. **换原生长上下文模型**(128K 级):12G 卡上 Qwen3 只能到 40960,要更大必须换模型族,不是调参能解决的。

### 额度够之后,还有一道质量关(实测)

把模型换成 8B @ 40960、把 `output` 调成 4096 之后,那一轮**确实跑完了**(`status: completed`,不再 400、不再压缩失败)。但输出是这样的:

- 指令是「只回答一个字:好」,**它没答"好"**;
- 反而生成了一整套「迁移 WSL 依赖」的幻觉任务:编出一个不存在的 `WSL_deps.ps1`、拿它去调 `workflow_validate`、再 `read` 一个不存在的路径、最后 `ask_user` 追问用户;
- 5 次工具调用全部无效。

根因是它的输入里带着跨会话投递的元数据(`fromTitle: "迁移 WSL 依赖管线到 Linux 服务器"`),8B **把元数据当成了任务指令**。这暴露的是 8B 这一档的指令分辨力,**和上下文容量无关**——就算把上下文加到 128K,这类"分不清哪句是指令"的失败一样会发生。

> 💡 判据:**容量是门槛,能力是天花板,两者要分别过。**
> `40960 ≥ 基线 + 工作区` 只说明"能开口说话";能不能**照指令做对事**,是另一件需要单独验的事。
> 验法:给一条"只答一个字"的极简指令,模型答了别的就是不过——不需要复杂任务就能识别。



## 五、失效条件

- **桌面搬回 3080** → 可用显存立刻少 ~0.9G,20480 会掉层,得回退 16384。
- **能关掉 aseprite(116 工具)这类巨型 MCP** → 基线从 32689 降下来,本地模型才有可用工作区;关不掉就只适合"脚本直接调 API",不适合当 MCode 的会话模型。换 128K 原生上下文的模型族是另一条路。
- **Qwen3 之外有无 YaRN/rope 扩展支持且实测质量可接受的模型** → 40960 这个天花板可以突破。
- **换 16G 卡(4080 / 4070Ti Super 等)** → 14B Q5_K_M 甚至 32B Q4 可以全上卡,这张选型表要重算。
- **需要多用户并发** → 回 vLLM(在 Linux 上),Ollama 不是干这个的。
- **Ollama 支持了 `/v1` 的 `think=false`** → 提示词 `/no_think` 那套 workaround 可以去掉。
- **Ollama 修了 `ollama create` 的复制行为(改硬链/引用)** → 磁盘翻倍那条消失。

## 关联

- [[3-1 3080 12G 实战部署：Qwen3-8B-AWQ + vLLM 参数取舍实录|3-1 3080 12G 实战部署实录]] —— 这篇的起点:14B 被判死刑的那次部署,以及 flashinfer / mirrored 网络两个坑
- [[4-2 WSL 退役与 Linux 迁移：开发环境搬去 Fedora，推理留在 Windows|4-2 WSL 退役与 Linux 迁移]] —— 为什么引擎必须换:vLLM 要的 WSL 已经不在了
- [[2-1 vLLM 参数速查表|vLLM 参数速查表]] —— `--gpu-memory-utilization` / `--max-model-len` / KV dtype 的通用语义,本文只记取舍结果
- 本机部署档案:`F:\LLM\README.md`(实测账本、启停脚本、provider 配置)
- 冒烟脚本:`F:\LLM\scripts\agent-smoke.ps1`(agent 闭环,失败非 0 退出)
