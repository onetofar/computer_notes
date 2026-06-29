---
title: "Claude Code 使用笔记"
category: 常用工具
tags:
  - net/practice
  - networking
difficulty: 基础
source: "自整理"
---
# 基础命令
```shell
//----------常用命令----------------------------
/init → 项目初始化（生成CLAUDE.md）
/index → 代码索引（触发向量检索）
/model → 模型管理（查看/切换/状态）
/clear → 清空对话（开新任务）
/edit → 编辑文件（宿主机同步）
/run → 执行命令（编译/运行C++）
/search → 语义搜索（找代码）
/plan → 生成开发计划（架构设计）
/help → 查看命令帮助
//------------------开始项目---------------------------
# 1. 宿主机创建项目目录并进入
mkdir game-server && cd game-server
# 2. 启动Docker容器
docker-compose up -d workspace-dev
# 3. 进入容器
docker-compose exec workspace-dev bash
# 4. 初始化项目（生成CLAUDE.md）
claude
/init
exit
# 5. 宿主机编辑项目规则（中文+C++规范）
code ./CLAUDE.md  # 粘贴之前的C++服务端规范
//----------------------日常开发---------------------------
# 1. 进入容器启动Claude
docker-compose exec workspace-dev bash
claude
# 2. 索引代码（首次+每次代码变更后）
/index
# 3. 切换到Sonnet模型（日常编码主力）
/model sonnet
# 4. 规划新模块（如游戏网关）
/plan 设计基于epoll的高并发游戏网关，支持TCP长连接
# 5. 编写代码（AI生成后编辑）
/edit src/gateway.cpp
# 6. 编译运行
/run g++ -o gateway src/gateway.cpp -lpthread
/run ./gateway
# 7. 性能优化（切换到Opus模型）
/model opus
/perf ./gateway  # 分析性能瓶颈
# 8. 保存会话，清理上下文
/save gateway_dev
/clear
```
# 配置文件对应命令解释:
### `switchModelsOnFlag: true`
**会自动切换档位**。
Claude Code 会实时识别对话中的任务复杂度：
- 当你直接说「帮我做 XX 架构设计」「深度排查内存泄漏」「重构整个网络模块」这类高复杂度需求时，**会自动临时升级到 OPUS 档**（调用你配置的 `deepseek-v4-pro`），保证推理质量；
- 当任务回到常规编码、简单问答时，会自动回落回 SONNET 主力档（`doubao-seed-2.0-code`）；
- 文档查询、简单语法问题等轻量任务，会自动降到 HAIKU 档（`deepseek-v4-flash`），提速省 Token。
- 日常对话不会一直用它。
  根据官方模型配置规则，`ANTHROPIC_MODEL` 仅作为**会话初始化的启动默认值**，只要触发第一次任务复杂度判定，就会自动切换到对应逻辑档位，之后的基准锚点变为 SONNET 档code.claude.com：
  1. **简单问答、文档查询、轻量操作**：自动降到 HAIKU 档（对应 `ANTHROPIC_DEFAULT_HAIKU_MODEL`）
  2. **日常编码、普通对话、中等复杂度任务（占日常 90% 以上场景）**：自动落到 SONNET 档（对应 `ANTHROPIC_DEFAULT_SONNET_MODEL`）
  3. **架构设计、深度排错、大规模重构**：自动升到 OPUS 档（对应 `ANTHROPIC_DEFAULT_OPUS_MODEL`）
  **关键规则：回落锚点是 SONNET，不是初始模型**
  自动升档完成任务后，会回落到 **SONNET 档**，而非回到 `ANTHROPIC_MODEL` 的初始值。也就是说，会话一旦开始交互，基准模型就从 `ANTHROPIC_MODEL` 切换成了 `ANTHROPIC_DEFAULT_SONNET_MODEL`，前者基本不再被调用。
###  `switchModelsOnFlag: false`
**完全不会自动切换**。
无论你描述的任务多么复杂，都会始终使用当前生效的模型（默认是 `ANTHROPIC_MODEL` 指定的 `ark-code-latest`，或你手动 `/model` 切换后的档位），只有手动执行 `/model` 命令才会变更模型。
**补充关键细节**
1. **自动切换是临时的**：仅针对当前复杂任务临时升档，任务完成后自动回落，不会永久停留在高档位。
2. **手动切换优先级更高**：如果你手动执行过 `/model sonnet` 固定了档位，自动升档只会临时调用 OPUS，结束后仍回到你手动指定的 SONNET。
3. **严格使用你配置的模型**：所有自动升降档都严格对应你配置的三个默认模型，不会调用其他未配置的模型。
4. **验证方式**：输入 `/model status` 可查看自动切换开关状态；对话过程中状态栏会显示当前实际使用的模型档位。
1. **真正会触发自动升档的命令只有 3 个**：`/plan`、`/perf`、`/model opusplan`，且必须 `switchModelsOnFlag=true` 才生效；其余编码、搜索、问答类命令默认都走 SONNET 主力档。
2. **`/index` 和 `/search` 的召回阶段完全不碰对话模型**，调用独立的 Embedding 接口，额度和计费也和对话模型分开。
3. **手动 `/model` 切换优先级最高**：只要你手动切过模型，自动切换规则都会在手动指定的基础上运作；如果想彻底杜绝自动切换，把 `switchModelsOnFlag` 设为 `false` 即可，全程 100% 手动可控。
# 详解命令
## 一、项目初始化与配置（类似/init的核心基础）
这组是**新项目必用**，定义项目规则、索引代码、管理目录，对应你之前的 `/init` 需求。
| 命令                | 作用                                                         | 何时用                          | 实操示例                                       |
| ------------------- | ------------------------------------------------------------ | ------------------------------- | ---------------------------------------------- |
| `/init`             | 生成项目级 `CLAUDE.md`，定义AI行为规则                       | 新项目首次打开                  | `/init` → 宿主机编辑 `./CLAUDE.md`             |
| `/index`            | 索引当前目录代码，触发向量检索（用你配置的 `doubao-embedding-vision`） | 项目代码变更后、需全局搜索时    | `/index` → 等待索引完成 → 可用 `/search`       |
| `/add-dir [path]`   | 添加额外工作目录到上下文                                     | 多模块项目（如网关+逻辑服）     | `/add-dir ./gateway ./logic`                   |
| `/ignore [pattern]` | 忽略指定文件/目录（不索引/不分析）                           | 过滤build、log、node_modules    | `/ignore build/** *.log node_modules`          |
| `/config`           | 打开全局配置（`settings.json`）                              | 改模型、API、语言、自动切换规则 | `/config` → 宿主机编辑 `.claude/settings.json` |
**关键规则重申**：
- `/init` 生成的**项目级 `CLAUDE.md` 优先级 > 全局 `.claude/CLAUDE.md`**，当前目录有则覆盖全局 
- `/index` 会触发你配置的 `EMBEDDING_MODEL` 和 `EMBEDDING_BASE_URL`，用于代码语义搜索 
---

## 二、模型管理（彻底掌控，消除黑盒）
针对你「怕自动切换、想手动掌控」的核心诉求，这组命令让你**100%控制模型选择**。

| 命令              | 作用                                  | 何时用                          | 实操示例                                                     |
| ----------------- | ------------------------------------- | ------------------------------- | ------------------------------------------------------------ |
| `/model`          | 查看可用模型 & 当前状态               | 确认模型配置、切换前检查        | `/model` → 显示 Haiku/Sonnet/Opus 映射                       |
| `/model [model]`  | 手动切换模型                          | 快速任务切Haiku、架构设计切Opus | `/model sonnet`（日常编码）<br>`/model opus`（底层优化）<br>`/model haiku`（快速调试） |
| `/model status`   | 查看自动切换状态                      | 验证 `switchModelsOnFlag` 配置  | `/model status` → 显示 "Auto switching: Disabled"（方案A）   |
| `/model opusplan` | 启用规划专用模式（规划→编码自动切换） | 大型模块设计（如Reactor框架）   | `/model opusplan` → 规划用Opus，编码自动切Sonnet             |

**模型切换逻辑**：
- 方案A（`switchModelsOnFlag: false`）：**全程固定默认模型**，仅手动 `/model` 切换生效
- 方案B（`switchModelsOnFlag: true`）：仅 `/plan` 工作流自动切模型，其他场景不自动切换

---

## 三、对话与上下文控制（高效管理会话）
控制对话历史、回退错误、并行提问，避免思路被带偏。

| 命令                              | 作用                                 | 何时用                  | 实操示例                     |
| --------------------------------- | ------------------------------------ | ----------------------- | ---------------------------- |
| `/clear`（别名：`/reset`/`/new`） | 清空当前对话历史（保留文件/规则）    | 开新任务、上下文混乱时  | `/clear` → 重新开始对话      |
| `/rewind [n]`                     | 回退n步对话（支持代码/对话独立回退） | AI改坏代码、回答偏离时  | `/rewind 2` → 回到2步前状态  |
| `/btw [question]`                 | 并行提问，不打断当前任务             | 重构时临时问API用法     | `/btw 如何用epoll实现ET模式` |
| `/compact`                        | 压缩上下文（保留关键信息，省Token）  | 会话过长、响应变慢时    | `/compact` → 优化上下文体积  |
| `/save [name]`                    | 保存当前会话到项目历史               | 重要调试/设计会话需存档 | `/save epoll_server_debug`   |

---

## 四、代码与文件操作（核心开发场景）
直接在会话中操作文件、查看差异、执行代码，适配C++服务端开发。

| 命令           | 作用                               | 何时用                      | 实操示例                                                    |
| -------------- | ---------------------------------- | --------------------------- | ----------------------------------------------------------- |
| `/edit [file]` | 编辑文件（容器内打开，宿主机同步） | 修改代码、配置文件          | `/edit src/epoll_server.cpp` → 宿主机编辑对应文件           |
| `/view [file]` | 查看文件内容（带语法高亮）         | 快速浏览头文件、配置        | `/view include/net/epoll.h`                                 |
| `/diff [file]` | 查看文件修改差异                   | 验证AI代码改动、对比版本    | `/diff src/epoll_server.cpp`                                |
| `/run [cmd]`   | 执行系统命令（容器内）             | 编译C++、运行测试、查看日志 | `/run g++ -o server src/*.cpp -lpthread`<br>`/run ./server` |
| `/test [file]` | 生成并运行单元测试                 | 验证C++模块正确性           | `/test src/utils.cpp` → 生成gtest测试用例                   |

---

## 五、搜索与查询（利用向量检索提效）
基于你配置的Embedding模型，快速定位代码、查询项目信息。

| 命令              | 作用                                   | 何时用                        | 实操示例                                                   |
| ----------------- | -------------------------------------- | ----------------------------- | ---------------------------------------------------------- |
| `/search [query]` | 语义搜索项目代码（用Embedding）        | 找epoll相关代码、内存池实现   | `/search epoll反应堆模式实现`                              |
| `/find [pattern]` | 文件/符号搜索（类似grep）              | 找特定函数、宏定义            | `/find "void handle_connection"`                           |
| `/docs [topic]`   | 查询技术文档（内置C++标准库/系统调用） | 查epoll_ctl参数、智能指针用法 | `/docs epoll_ctl EPOLL_CTL_ADD`<br>`/docs std::shared_ptr` |
| `/todo`           | 列出项目中所有TODO注释                 | 跟踪开发任务                  | `/todo` → 显示所有待完成项                                 |

---

## 六、任务与工作流（提升开发效率）
规划架构、分解任务、管理后台进程，适配大型项目开发。

| 命令                         | 作用                               | 何时用                     | 实操示例                                 |
| ---------------------------- | ---------------------------------- | -------------------------- | ---------------------------------------- |
| `/plan [task]`               | 生成详细开发计划（架构→步骤→代码） | 开发新模块（如游戏登录服） | `/plan 设计基于epoll的游戏登录服务器`    |
| `/tasks`（别名：`/bashes`）  | 查看后台任务列表                   | 并行编译、长时间索引时     | `/tasks` → 显示运行中任务                |
| `/background`（别名：`/bg`） | 将会话放到后台运行                 | 执行耗时任务（如全量索引） | `/background` → 会话后台运行，可继续操作 |
| `/resume`                    | 恢复后台会话                       | 查看后台任务结果           | `/resume` → 回到之前后台会话             |

---

## 七、设置与界面（个性化体验）
调整界面语言、显示模式，适配你的中文使用习惯。

| 命令               | 作用                      | 何时用                 | 实操示例                                |
| ------------------ | ------------------------- | ---------------------- | --------------------------------------- |
| `/language [lang]` | 切换界面语言（临时生效）  | 验证中文界面           | `/language zh-CN` → 界面立即变中文      |
| `/theme [theme]`   | 切换终端主题（暗色/亮色） | 护眼、适配环境         | `/theme dark`                           |
| `/vim`             | 切换Vim编辑模式           | 习惯Vim操作的开发者    | `/vim` → 编辑文件时用Vim快捷键          |
| `/status-bar`      | 切换状态栏显示            | 精简界面、显示关键信息 | `/status-bar` → 切换显示模型/Token/时间 |

---

## 八、系统与更新（维护工具健康）
管理Claude Code版本、缓存、日志，确保稳定运行。

| 命令       | 作用                      | 何时用                   | 实操示例                            |
| ---------- | ------------------------- | ------------------------ | ----------------------------------- |
| `/update`  | 升级到最新版本            | 修复bug、获取新功能      | `/update` → 自动下载安装更新        |
| `/clean`   | 清理缓存（MCP/索引/会话） | 磁盘空间不足、缓存异常时 | `/clean` → 清理 `.cache/mcp` 等目录 |
| `/version` | 查看当前版本              | 确认是否需要更新         | `/version` → 显示版本号             |
| `/log`     | 查看会话日志              | 排查错误、调试问题       | `/log` → 显示当前会话日志           |

---

## 九、进阶命令（C++游戏服务端专属）
针对你「Java转C++、做游戏服务端」的核心场景，这些命令能大幅提效。

| 命令              | 作用                        | 何时用                | 实操示例                               |
| ----------------- | --------------------------- | --------------------- | -------------------------------------- |
| `/perf [cmd]`     | 性能分析（集成perf/strace） | 优化C++服务端性能瓶颈 | `/perf ./server` → 分析CPU/内存占用    |
| `/gdb [program]`  | 启动GDB调试                 | 调试C++崩溃、死锁     | `/gdb ./server` → 进入GDB调试界面      |
| `/valgrind [cmd]` | 内存泄漏检测                | 排查C++内存问题       | `/valgrind --leak-check=full ./server` |
| `/docker [cmd]`   | 容器内执行Docker命令        | 管理服务端容器        | `/docker ps` → 查看运行中容器          |

---

## 十、帮助与快捷命令（快速上手）
| 命令               | 作用             | 何时用               |
| ------------------ | ---------------- | -------------------- |
| `/help`            | 查看所有可用命令 | 忘记命令语法时       |
| `/help [command]`  | 查看特定命令详情 | 了解命令参数、用法   |
| `/`（输入后按Tab） | 命令自动补全     | 快速输入长命令       |
| `Esc`              | 中断当前生成     | AI回答偏离、需停止时 |
| `Ctrl+C`           | 强制取消操作     | 命令执行卡住时       |



---

## 十二、关键避坑总结
1. **模型切换**：`switchModelsOnFlag: false` 时，**只有 `/model` 命令能切换模型**，彻底消除黑盒
2. **规则优先级**：项目级 `CLAUDE.md` > 全局 `.claude/CLAUDE.md`，**不要重复执行 `/init`** 覆盖规则
3. **Embedding触发**：`/index` 和 `/search` 会自动调用你配置的 `EMBEDDING_MODEL`，无需手动触发
4. **宿主机编辑**：所有配置文件（`settings.json`/`CLAUDE.md`）**只在宿主机修改**，Docker会实时同步
5. **命令冲突**：避免同时使用多个模型切换命令，推荐全程用 `/model` 手动控制





# MCP AGENT

1. 

### 进阶阶段（建议配置 Agent）：

当你开始做**复杂多模块开发**时（如写完整 Reactor 模型、线程池）：

1. 配置 `cpp-network-expert` Agent：负责核心网络代码实现
2. 配置 `code-reviewer` Agent：检查内存泄漏、线程安全
3. 配置 `cmake-specialist` Agent：处理复杂编译问题

------

### 快速实操：把 /role-dev 升级为 Agent（可选）

如果你想体验 Agent，只需：

1. 复制文件到 agents 目录：

   ```
   cp /home/onetofar/docker-data/claude-config/commands/role-dev.md \
      /home/onetofar/docker-data/claude-config/agents/cpp-network-agent.md
   ```

2. 添加 YAML 头（关键步骤）：

   ```
   ---
   name: cpp-network-agent
   description: C++游戏服务端开发专家，精通Linux socket/epoll/线程池
   model: deepseek-v4-pro
   memory: user
   permissions:
     read: ["**/*.cpp", "**/*.h", "CMakeLists.txt"]
     write: ["**/*.cpp", "**/*.h"]
   ---
   ```

3. 验证：

   ```
   claude agent list-available  # 能看到cpp-network-agent
   ```



学习阶段 不建议一上来装很多 MCP。你现在这个项目是 C++17 网络编程学习项目，Claude Code 已经自带了读文件、改文件、运行命令、搜索代码等能力，所以很多 MCP 对你来说是重复的。

  如果只选一个，我建议优先装：

    1. 首选：文档类 MCP，例如 Context7

  适合你现在的阶段。

  它的价值是：当你问 C++、CMake、Linux 网络编程、第三方库用法时，可以让 Claude 查更接近官方/新版的文档，而不是只靠模型记忆。

  适合问：

  - epoll 的正确使用方式；
  - CMake target/link/include 的写法；
  - std::thread、mutex、RAII 的用法；
  - 如果以后学 Boost.Asio、fmt、spdlog、gtest，会更有用。

  示例安装思路：

  claude mcp add context7 -- npx -y @upstash/context7-mcp

  如果你想在当前 Claude Code 会话里执行，可以输入：

  ! claude mcp add context7 -- npx -y @upstash/context7-mcp

  然后重启 Claude Code 或重新检查：

  ! claude mcp list

---

    2. 第二优先级：GitHub MCP

  如果你的学习项目已经推到 GitHub，并且你开始用：

  - issue 记录学习问题；
  - PR 做阶段性重构；
  - code review；
  - 查看远程仓库、提交、分支；

  那就装 GitHub MCP。

  但如果你只是本地写代码、编译、运行，其实暂时不急。Claude Code 已经能通过 git 命令看本地状态、diff、commit 历史。

  适合阶段：

  ▎ 你开始把每个学习任务做成 issue/branch/PR 的时候再装。

---

    3. 暂时不必装：filesystem / git MCP

  这两个对 Claude Code 用户通常 重复度很高。

  Claude Code 本身已经有：

  - 读文件；
  - 写文件；
  - 搜索代码；
  - 运行 shell；
  - 查看 git diff；
  - 构建项目。

  所以你现在没必要额外装 filesystem MCP 或 git MCP。反而多一个文件系统入口，权限边界更复杂。

---

    4. 暂时不必装：数据库类 MCP

  例如：

  - PostgreSQL MCP；
  - MySQL MCP；
  - Redis MCP；
  - SQLite MCP。

  你的当前项目重点是：

  - socket；
  - ARP/IP/TCP/UDP；
  - epoll；
  - echo server；
  - 游戏服务器网络模型。

  还没到数据库阶段。等你做：

  - 账号系统；
  - 玩家数据持久化；
  - 排行榜；
  - 战绩记录；
  - Redis 匹配队列；

  再考虑数据库 MCP。

---

    5. 暂时不必装：Kubernetes / Docker / 云服务 MCP

  你现在虽然有 Docker 开发环境，但学习重点不是部署平台。

  这些以后再说：

  - Docker MCP；
  - Kubernetes MCP；
  - AWS/GCP/Azure MCP；
  - Terraform MCP。

  等你开始学：

  - 服务部署；
  - 多进程管理；
  - 日志采集；
  - 压测；
  - 容器编排；

  再装也不晚。

---

    6. 可选：Playwright / 浏览器 MCP

  目前你的项目是网络服务端，不是 Web 前端，所以不是刚需。

  什么时候有用？

  - 你写了 Web 管理后台；
  - 游戏服务器有 HTTP/WebSocket 测试页面；
  - 需要 Claude 自动打开网页、点按钮、截图验证。

  现在可以先不装。

---

  我的建议清单

  按你当前学习阶段，我建议这样：

  ┌────────┬───────────────────────┬──────────────┬──────────────────────────────────┐
  │ 优先级 │          MCP          │   是否建议   │               原因               │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  │ 1      │ Context7 / 文档类 MCP │ 建议装       │ 查 C++、CMake、库文档最有用      │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  │ 2      │ GitHub MCP            │ 可稍后装     │ 等你开始用 issue/PR 管理学习任务 │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  │ 3      │ Playwright MCP        │ 暂不装       │ 当前不是 Web 项目                │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  │ 4      │ 数据库 MCP            │ 暂不装       │ 还没进入持久化阶段               │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  │ 5      │ Docker/K8s/云服务 MCP │ 暂不装       │ 部署阶段再说                     │
  ├────────┼───────────────────────┼──────────────┼──────────────────────────────────┤
  现在可以先不装。

  一句话结论

  你现在最应该装的是：

  ▎ Context7 这类文档 MCP。

  其他 MCP 先别急。你的阶段最重要的是：
  把 C++、Linux socket、epoll、线程池、协议设计、调试能力练扎实。
  MCP 只装一个能帮你查文档的就够了。
