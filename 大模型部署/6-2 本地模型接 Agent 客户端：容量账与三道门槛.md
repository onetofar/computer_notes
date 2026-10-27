---
title: "6-2 本地模型接 Agent 客户端:容量账与三道门槛"
category: 大模型部署
tags:
  - Agent
  - 上下文预算
  - MCode
  - ClaudeCode
  - 工具调用
  - 实测
difficulty: 进阶
source: "2026-10-28 本机实测(MCode + Claude Code 2.1.311 接本地 Ollama 0.40.11)"
---

# 本地模型接 Agent 客户端:容量账与三道门槛

> 这篇回答:**本地模型明明能跑,为什么接进 Agent 就废。**
> 引擎选型见 [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1]];
> 术语见 [[6-1 大模型名词解释：读懂模型名与参数手册|6-1]]。

## 核心结论

**Agent 客户端的固定基线,不是模型的问题。**
能不能当 Agent 用,取决于 `上下文 ≥ 客户端基线 + 有效工作区`,
而基线由**客户端的工具 schema + 记忆注入**决定,与模型无关。

---

## 一、门槛一:协议层(能不能连上)

Ollama 同时说 OpenAI 与 Anthropic 两套协议(实测见 [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1]] §1.2),所以**不需要转译代理**。

| 客户端 | 端点 | 注意 |
|---|---|---|
| MCode / 通用 OpenAI 客户端 | `http://127.0.0.1:11434/v1` | **必须带 `/v1`**;只写 `:11434` 会打到 Ollama 原生 API,路径不匹配 |
| Claude Code | `ANTHROPIC_BASE_URL=http://192.168.1.5:11434` | **不要带 `/v1`**,客户端自己拼 `/v1/messages` |

**鉴权是假的**:Ollama 不校验 token,但客户端通常要求"必须有 key",所以填任意占位值(`ollama`)即可。

---

## 二、门槛二:容量层(装不装得下基线)

### 2.1 实测基线(MCode 自己记录的,不是估算)

关掉 aseprite 等 MCP 之后的一次真实请求:

| 构成 | tokens | 说明 |
|---|---|---|
| **TOOLS** | **17,028 – 17,680** | 关 MCP 前约 3.2 万,关完仍 1.7 万(内置工具 + 未关的插件) |
| **MEMORY** | **9,590 – 9,958** | 用户画像 + 累积记忆条目,**与任务无关但每次都要带** |
| SYSTEM_PROMPT | 2,866 – 2,976 | |
| SKILLS | 793 – 823 | |
| **固定基线合计** | **≈ 30,300** | |

### 2.2 压缩为什么救不了它

第一次接进 MCode,第一条消息就失败(会话标题是「你好」——**用户还没说话**):

```
400 request (32689 tokens) exceeds the available context size (20480 tokens)
compaction_failed: "Generated checkpoint does not fit the next Provider request."   ×6
```

**压缩只能压「对话」,压不掉基线。** 压缩请求本身也要发 `基线 + 对话`;
摘要生成后再发 `基线 + 摘要`,仍然超限 → 判定装不进 → 重试 → 放弃。
所以现象是"上下文压缩都不行",**病灶在基线,不在压缩**。

### 2.3 算术:一道很窄的余量

```
40960(上下文) − 30300(基线) = 10660
10660 − 4096(output 预留)    = 6564   ← checkpoint + 保留消息的全部空间
```

⚠️ **`output` 预留直接决定压缩能否成功**,而且极易被忽略:
最初把 `output` 写成 **8192** 时,`40960 − 8192 = 32768 < 基线 30300`,
**任何 checkpoint 都不可能装下**——这是"压缩永远失败"最直接的一个原因。改成 4096 后才腾出 6.5K。

### 2.4 两个反直觉点

> ① **关 MCP 只砍掉了一半 TOOLS**,剩下的是内置工具与插件——MCP 不是唯一大头。
> ② **MEMORY 占 9.6K**,它是**纯固定成本**,不随任务变化——精简记忆条目比关 MCP 更直接。

---

## 三、门槛三:能力层(照指令做对事)

**容量过了不等于能用。** 实测:14B @ 20480 直接 `Prompt is too long`(基线就超);
8B @ 40960 简单问答会 `Autocompact is thrashing: the context refilled to the limit within 3 turns … 3 times in a row`。

即使给了 8B,任务一变也露馅:指令是「只回答一个字:好」,它没答"好",
反而生成一整套**幻觉任务**——捏一个不存在的 `WSL_deps.ps1`、拿它调 `workflow_validate`、再 `read` 一个不存在的路径,5 次工具调用全废。
根因是输入里带着**跨会话投递的元数据**,8B **把元数据当成了任务**。

> 💡 判据:**容量是门槛,能力是天花板,两者要分别过。**
> 验法很便宜:给一条"只答一个字"的极简指令,答了别的就是不过——不需要复杂任务就能识别。

### 3.1 一个真实翻车点:别让模型做单位换算

工具返回 `{bytes: 9001752960, total_gb: 14.65}`,模型把 **total 原样引用对了**,
但**自己心算单个文件的 GB 两次都算错**(说 8.58 / 6.32 / 6.41 GB,实际 9.0 GB 十进制 / 8.38 GiB)。

> 💡 工具设计原则:**服务端算好、带上单位再返回**,别指望模型做 bytes→GB 除法。
> 改成 `{name, bytes, gb}` 三字段,模型只负责转述,准确率立刻上去。

---

## 四、闭环验证(光看 tool_calls 字段存在是不够的)

那只能证明它会说话,不能证明它能干活。完整闭环长这样:

```
TURN 1  finish_reason: tool_calls        8.2s
        list_dir({"path": "F:\\LLM\\models"})
本地真实执行 → {"total_bytes":15727652000, "total_gb":14.65, "files":[...]}
TURN 2  finish_reason: stop              8.8s
        「…总大小为 14.65 GB」            ← 数字来自工具返回,不是编的
PASS    总耗时 16.9s
```

脚本:`F:\LLM\scripts\agent-smoke.ps1`(**失败非 0 退出**,可当回归门禁)。

---

## 五、Claude Code 接本地模型的实测(4-2 同期)

| 配置 | 结果 |
|---|---|
| 14B @ 20480 | ❌ **完全不可用**:`Prompt is too long` —— 客户端基线就超过 20480 |
| 8B @ 40960 · 简单问答 | ⚠️ `Autocompact is thrashing`(压了三次没压出空间) |
| 8B @ 40960 · **工具调用** | ✅ **真跑通**:`claude --allowedTools Bash -p "用 Bash 执行 ls /tmp \| head -3"` 返回了 xier 上真实存在的文件 |
| 协议层 | ✅ 6 秒返回,rc=0,Anthropic SSE 事件完整 |

### 5.1 Claude Code 侧的四个硬约束(都是实测撞出来的)

| 项 | 事实 |
|---|---|
| `CLAUDE_CODE_MAX_CONTEXT_TOKENS` | **必须显式给**。模型 ID 不以 `claude-` 开头时它直接生效;不给就按 200K 规划,发出超过实际窗口的请求 |
| `CLAUDE_CODE_AUTO_COMPACT_WINDOW` | **不要设**。它有 **100K 下限**——给 16384 会报 `reads as 16,384 tokens … treats it as the 100k minimum` 并直接 `Prompt is too long` |
| `ANTHROPIC_BASE_URL` | 不带 `/v1`;token 任意非空值 |
| `claude -p` 在 ssh 里 | 必须 `< /dev/null`,否则一直等 stdin |

> 💡 20K 级模型在 Agent 场景里**没有可配的压缩窗口**——这是 100K 下限带来的死结,
> 不是配置没调对。

---

## 六、失效条件

| 条件 | 后果 |
|---|---|
| **客户端换了 / 装了更多插件** | 基线变大,工作区进一步被挤 |
| **记忆条目继续累积** | MEMORY 固定成本上升,可用工作区同步缩小 |
| **换成原生 ≥128K 上下文的模型** | 这三道门槛里的容量门槛自然消失(12G 卡上 Qwen3 到顶 40960,不是调参能解决的) |

## 关联

- [[6-1 大模型名词解释：读懂模型名与参数手册|大模型名词解释]] —— 上下文窗口 / KV Cache / 思考模型 / 工具调用 等术语
- [[4-1 Windows 原生推理选型：为什么 Ollama + Qwen3-14B 取代了 WSL 里的 vLLM|4-1 原生推理选型]] —— 引擎与模型选型、显存账
- [[5-1 模型文件名怎么读：Qwen3-14B Q4_K_M 逐段拆解|5-1 模型文件名怎么读]] —— 量化档位与 bpw
- [[4-2 WSL 退役与 Linux 迁移：开发环境搬去 Fedora，推理留在 Windows|4-2 WSL 退役与 Linux 迁移]] —— 跨机调用的配置位置
- 冒烟脚本:`F:\LLM\scripts\agent-smoke.ps1`
