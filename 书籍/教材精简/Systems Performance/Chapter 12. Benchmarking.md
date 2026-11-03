---
title: Chapter 12. Benchmarking（精读）
category: 性能工程
tags:
  - 基准测试
  - 微基准
  - 负载模拟
  - 性能工程
  - 方法论
  - 可扩展性
difficulty: 进阶
source: "《Systems Performance: Enterprise and the Cloud, 2nd Edition》（Brendan Gregg）第12章 Benchmarking"
source_section: "精读 §12.1.1 / §12.1.2 / §12.1.3 / §12.2.1 / §12.2.2 / §12.2.3 / §12.2.4 / §12.3.1 / §12.3.2 / §12.3.3 / §12.3.4 / §12.3.5 / §12.3.6 / §12.3.7 / §12.3.8 / §12.3.9 / §12.3.10 / §12.4；§12.5 Exercises 与 §12.6 References 不立篇（练习题与参考文献是全书共用的自测/索引材料，不承载机制陈述）；§12.1.2 下的 Benchmark Analysis、§12.2.1 下的 Design Example、§12.2.4 下的 TPC / SPEC、§12.3.2 下的 Analysis Case Study、§12.3.3 下的 Example，以及 §12.1.3 下的 16 条无编号失败条目，按原文文字引用，不编 § 号"
skills:
  - obsidian-markdown
---

# Chapter 12. Benchmarking

> **小节地图**：本章讲的不是「怎么测某个组件」，而是**基准测试本身的机制与失败模式**——
> 基准测试在什么条件下可信、不可信有哪几种典型形态、什么时候该停止加压。
> 立篇：§12.1.1（七种动机）、§12.1.2（什么叫「做得好」+ 基准分析四问）、
> §12.1.3（16 条失败清单）、§12.2.1（微基准 + 测试矩阵）、§12.2.2（负载模拟）、
> §12.2.3（轨迹回放）、§12.2.4（行业标准基准）、§12.3.1（被动基准测试）、
> §12.3.2（主动基准测试 + bonnie++ 案例）、§12.3.3（CPU 剖析）、§12.3.4–§12.3.5（USE 方法与负载刻画）、
> §12.3.6（自定义基准）、§12.3.7（渐增负载）、§12.3.8（合理性检查）、
> §12.3.9（统计分析）、§12.3.10（检查清单）、§12.4（拿到别人结果时该问什么）。
> **不立篇**：§12.5 Exercises、§12.6 References（自测题与文献清单，不承载机制陈述）。
> 底层依赖：负载刻画、统计口径、USE 方法、可扩展性曲线的原理都在 [[Chapter 2. Methodologies]]；
> 被测对象侧的 I/O 栈与中断/调度口径见 [[Chapter 3. Operating Systems]]；
> 磁盘侧细节见 [[Chapter 9. Disks]]；整体方法论起点见 [[Chapter 1. Introduction]]。
> ⚠️ 本章不覆盖具体工具的使用步骤（那属于各章的方法论部分），也不覆盖云上虚拟化的性能代价（第 11 章）。

## 0 · 原文锚点（逐条可回查）

下面每条给出「本篇小节 → 原文位置 → 该处原文逐字摘录」。摘录一字不改，
用 `kb_anchor_check.py` 可以把每条引文回查所在行；对不上就是这里错了。

| 本篇小节 | 原文位置 | 该处原文（逐字摘录） |
|---|---|---|
| §12.1.1 云让基准测试变廉价 | 12-Chapter 12. Benchmarking.md L73 | The cloud computing model makes benchmarking easy: a large-scale environment can be created in minutes, used for a benchmark run, and then destroyed, all at very little cost. |
| §12.1.2 流行基准多不可靠 | 12-Chapter 12. Benchmarking.md L81 | In this article we survey 415 file system and storage benchmarks from 106 recent papers. We found that most popular benchmarks are flawed and many research papers do not provide a clear indication of true performance. |
| §12.1.2 评测须说明测了什么 | 12-Chapter 12. Benchmarking.md L83 | The paper also makes recommendations for what should be done; in particular, benchmark evaluations should explain what was tested and why , and they should perform some analysis of the system’s expected behavior. |
| §12.1.2 分析必须在跑的时候做 | 12-Chapter 12. Benchmarking.md L117 | It is best to engage the performance experts during the benchmark so they can analyze the system while it is still running. |
| §12.1.3 测的是 A，量的却是 B | 12-Chapter 12. Benchmarking.md L139 | Casual benchmarking: you benchmark A, but actually measure B, and conclude you’ve measured C. |
| §12.1.3 忽略环境 | 12-Chapter 12. Benchmarking.md L199 | your test server is a default setup with default tunables parameters, default file system, etc. The production database servers are tuned for high disk IOPS, so testing on an untuned system is unrealistic: you missed first understanding the production environment. |
| §12.1.3 忽略错误 | 12-Chapter 12. Benchmarking.md L205 | the web server did nothing at all during the test, as all requests were blocked by a firewall. All requests. |
| §12.1.3 忽略方差 | 12-Chapter 12. Benchmarking.md L211 | Bursts of writes may cause real issues in production, such as by queueing the reads, but are not simulated if the benchmark applies steady average rates. |
| §12.1.3 忽略扰动 | 12-Chapter 12. Benchmarking.md L219 | As a rule, the duration of a benchmark should not be shorter than one second. |
| §12.1.3 基准悖论 | 12-Chapter 12. Benchmarking.md L235 | “If your product’s chances of winning a benchmark are 50/50, you’ll usually lose.” |
| §12.1.3 基准特设 | 12-Chapter 12. Benchmarking.md L287 | All “benchmark special” implementations that improve benchmark results but not real-world performance or pricing, are prohibited. |
| §12.2.1 微基准的优势 | 12-Chapter 12. Benchmarking.md L311 | The advantage is the simplicity: narrowing the number of components and code paths involved results in an easier target to study and allows performance differences to be root-caused quickly. |
| §12.2.2 有状态负载用马尔可夫模型 | 12-Chapter 12. Benchmarking.md L407 | Such a workload can be better simulated using a Markov model , by representing requests as states and measuring the probability of state transitions |
| §12.2.3 回放不会自适应 | 12-Chapter 12. Benchmarking.md L413 | when characteristics and delivered latency change on the server, the captured client workload is unlikely to respond naturally to these differences, which may prove no better than a simulated customer workload. |
| §12.3.2 主动基准测试 | 12-Chapter 12. Benchmarking.md L569 | With active benchmarking, you analyze performance while the benchmark is running—not just after it’s done—using observability tools |
| §12.3.2 案例：bonnie++ 到底测了什么 | 12-Chapter 12. Benchmarking.md L683 | By analyzing the benchmark while it is running (active benchmarking), we’ve learned that the first bonnie++ test is a 1-byte file system write, which buffers in the file system cache. It does not test disk I/O, as is implied by the descriptions of bonnie++. |
| §12.3.3 CPU 剖析发现 ZFS 限流 | 12-Chapter 12. Benchmarking.md L731 | Browsing the stack frames showed that 62.17% of CPU samples included a function called zfs_zone_io_throttle(). I didn’t need to read the code for this function, as its name was enough of a clue: a resource control, ZFS I/O throttling, was active and artificially throttling the benchmark! This was a default setting on the new system (but not the older system) that had been overlooked when the benchmark was performed. |
| §12.3.7 冲过饱和点后延迟不可接受 | 12-Chapter 12. Benchmarking.md L806 | By adding more and more clients, you manage to scrape past 1 million IOPS; however, all operations are now heavily queued, with average latency of over 50 ms, which is not acceptable! Which result do you give marketing? (Answer: 990,000 IOPS.) |
| §12.3.8 合理性检查 | 12-Chapter 12. Benchmarking.md L812 | This is over 3.2 Gbits/s—well in excess of the 1 Gbit/s known limit. Something is wrong! |
| §12.3.10 检查清单：为什么不翻倍 | 12-Chapter 12. Benchmarking.md L888 | Why not double? Why was the operation rate not double the benchmark result? This is really asking what the limiter is. Answering this can solve many benchmarking problems, when you discover that the limiter is not the intended target of the test. |

> **书里没说的**（本篇有几处推论，原文没有，别当成原文观点）：
> - **§12.1.3 的 16 条失败在本篇被压成一张表**，加了「机制 / 后果」两列；原文是 16 个无编号小节的散文，
>   没有这张表，也没有统一的分类维度。
> - **本篇把 §12.3.1 称为「反面教材」**：原文的说法是 passive benchmarking 是「相对于主动基准测试的
>   一种反方法论」（its own anti-methodology），本篇的措辞是延伸。
> - **「厂商结果可当性能上界」**原文有明说，但本篇把它整理成了一条「上界，友好火力情况除外」的判据形式。
> - **§12.2.4 的「TPC 含价格、SPEC 不含价格，所以二者的可比性边界不同」是本篇的推演**：
>   原文只分别陈述了「TPC 结果包含 price/performance」与「SPEC 结果包含调优方式与部件清单、
>   但通常不含价格」，可比性边界的推论是本篇加的。
> - **本篇的「章末选型判据」表是本篇自己的整理**，原文对应的原始清单是 §12.3.10 的六问与 §12.4 的长清单。
> - **「晴天 / 阴天 / 雨天」的译法是本篇的**：原文用词为 sunny day performance testing 与
>   cloudy day or rainy day performance testing。

## §12.1.1 Reasons：七种动机决定了你能在跑的时候改什么

原文列出做基准测试的七种理由（System design / Proofs of concept / Tuning / Development /
Capacity planning / Troubleshooting / Marketing）。它们不是并列的清单——**每种动机决定了
跑基准时允许改变什么、结果给谁看、能不能重跑**：

| 动机 | 原文要点 | 对「怎么跑」的约束（推演） |
|---|---|---|
| System design 系统设计 | 比较不同系统、组件或应用；商用产品场景还提供价格性能比数据供采购决策 | 结论要跨系统可比 ⇒ 必须可重复 |
| Proofs of concept 概念验证 | 采购或投产前，在负载下测软硬件性能 | 结论要支撑采购 ⇒ 测试环境必须贴近生产（见 §12.1.3 #6） |
| Tuning 调优 | 测可调参数与配置项，筛出值得在生产负载上继续深入的 | 同一环境内多次对比 ⇒ 方差与控制变量是核心 |
| Development 开发 | 非回归测试（定期自动跑的测试套件，把性能回退快速定位到产品变更）+ 极限排查（把产品推到极限，找出工程投入该往哪儿使） | 非回归 ⇒ 需要环境冻结；极限排查 ⇒ 需要能加压 |
| Capacity planning 容量规划 | 确定系统与应用极限，为容量建模提供数据，或直接找到容量上限 | 需要**极限**而不是**平均值** |
| Troubleshooting 排障 | 验证组件是否仍能在最大性能下工作（如测两台主机之间的最大网络吞吐，判断有没有网络问题） | 需要与故障相关的**特定**操作 |
| Marketing 市场 | 测出产品最大性能供市场部使用（原文称之为 benchmarketing） | 目标是「最好结果」⇒ 天然鼓励挑配置（这是 §12.1.3 #13/#14 的根源） |

**经济性在云上彻底变了**（本节的锚点）：

> The cloud computing model makes benchmarking easy: a large-scale environment can be created in minutes, used for a benchmark run, and then destroyed, all at very little cost.

对比：本地机柜模式下，测一次硬件要经历发运、上架、布线、装操作系统，周期以周月计，
而且每出一代新硬件就要重来一次；云上则是**分钟级创建 → 跑 → 销毁**，几乎零成本。

**云还改变了「评测」这件事本身**：故障-tolerant 的分布式云环境让实验变容易——新的实例类型一上线，
就可以直接用生产负载测，跳过传统的基准评测流程。此时基准测试的角色变成**解释差异**：
Netflix 的性能工程团队用一组自动化软件分析新实例类型，包含多种微基准，
并自动收集系统统计与 CPU profile，以便发现差异后能定位原因。

## §12.1.2 Effective Benchmarking：把「好基准」拆成可检验的特征

**先看反面证据**（本节锚点）：

> In this article we survey 415 file system and storage benchmarks from 106 recent papers. We found that most popular benchmarks are flawed and many research papers do not provide a clear indication of true performance.

这篇九年研究扫了 106 篇论文里的 415 个文件系统/存储基准，结论是**大多数流行基准有缺陷**，
很多论文也没说清楚真实性能是什么。它给的建议是：

> The paper also makes recommendations for what should be done; in particular, benchmark evaluations should explain what was tested and why , and they should perform some analysis of the system’s expected behavior.

**「好基准」的六条特征**（原文引 Smaalders《Performance Anti-Patterns》）：
Repeatable（可重复，便于比较）、Observable（可观测，性能能被分析与理解）、
Portable（可移植，能在竞品上测、跨不同产品版本测）、Easily presented（易呈现，人人看得懂）、
Realistic（真实，反映客户实际体验）、Runnable（可运行，开发者能快速复测）。
采购场景还要再加一条：**价格性能比**，价格按设备的五年资本成本计。

### Benchmark Analysis：跑基准时必须回答的四个问题

原文要求用基准时必须想清楚：测的是什么；限制因素是哪些；哪些扰动会影响结果；
从结果能推出什么结论。这四问要求「深度理解基准软件在干什么、系统如何响应、
结果与目标环境如何关联」——因此分析必须发生在**运行期间**（本节锚点）：

> It is best to engage the performance experts during the benchmark so they can analyze the system while it is still running.

原文点名的典型错误是：让初级员工执行基准，跑完再请性能专家来解释结果；
专家应当在跑的时候就介入，顺带做下钻分析来解释并量化限制因素。

**一个正面样本（Bill Joy，1981）**：在两台机器间传 4 MB 数据，按 1024 字节记录、封装成 1068 字节以太帧，
从 11/750 到 11/780 用了 28 秒（含建立与拆除连接），用户到用户吞吐 1.2 Mb/s。
这段时间里 **11/750 CPU 饱和、11/780 约 30% 空闲**；而花在系统里的时间被拆成
Ethernet 处理 20%、IP 包处理 10%、TCP 处理 30%、校验和 25%、用户系统调用处理 15%，
没有哪一项占主导。原文点出这段引文的价值：**先定位限制因素，再拆解内核各组件的时间占比**；
并指出「高层级地汇总内核 CPU 花在哪」直到火焰图出现才变得容易——这段引文比火焰图早了三十年。

**替代方案**：与其用现成工具，不如自己写基准软件，或至少自己写负载生成器——
保持简短、只测需要的东西，便于快速分析与调试。
**当你既没有工具也没有系统时**（比如在读别人发布的结果），就只能靠材料提问：
系统环境是什么？怎么配置的？（完整问题清单见 §12.4。）

## §12.1.3 Benchmarking Failures：16 条失败清单

原文在这一节列了 16 种失败。本篇把它压成一张表（**失败 / 机制 / 后果**），随后展开其中六条：

| # | 失败 | 机制 | 后果 |
|---|---|---|---|
| 1 | Casual Benchmarking | 工具给出数字，但数字未必对应你以为它测的东西 | 结论凭空而来 |
| 2 | Blind Faith | 流行 ≠ 正确（argumentum ad populum）；真正的问题常在**对结果的解释**而非软件本身 | 把有缺陷的基准当权威 |
| 3 | Numbers Without Analysis | 只有数字、没有分析细节 | 往往只是调查的开始 |
| 4 | Complex Benchmark Tools | 工具自身的复杂度阻碍分析；「benchmarking the benchmark」——结果被基准软件自己限制住，常见原因是单线程基准软件 | 测出来的是工具的上限 |
| 5 | Testing the Wrong Thing | 工具很多但与目标负载无关（典型：目标负载全在 page cache 里，却去测磁盘） | 工程努力优化了错误的行为 |
| 6 | Ignoring the Environment | 测试环境 ≠ 生产环境（默认参数、默认文件系统、未调优） | 结论在生产不成立 |
| 7 | Ignoring Errors | 部分甚至全部请求报错，也照样计入结果 | 报出来的「延迟」其实是超时 |
| 8 | Ignoring Variance | 用稳态平均负载掩盖了突发与正交的操作类型 | 漏掉生产上的排队问题 |
| 9 | Ignoring Perturbations | 备份、监控采集、云上同宿主机的其他租户 | 结果不可重复 |
| 10 | Changing Multiple Factors | 两组结果之间还有别的差异（网络跳数、拥塞、实例落在快慢不同的机器上） | 对比无效 |
| 11 | Benchmark Paradox | 单个基准 50% 胜率，而客户要连赢多个 | 测得越多越可能输 |
| 12 | Benchmarking the Competition | 客户会用几个月并自行调优，你没有几周去调优竞品 | 未调优对比，双方信誉受损 |
| 13 | Friendly Fire | 自己的产品配置没调好、或没推到极限就发布 | 自家产品被低估 |
| 14 | Misleading Benchmarks | 技术正确但被选择性呈现（典型是省略价格） | 客户预期错位 |
| 15 | Benchmark Specials | 为跑分加「特设」，牺牲真实性能或真实价格 | 行业规范明令禁止 |
| 16 | Cheating | 直接编造数字 | 罕见但存在 |

### #1 测的是 A，量的却是 B

> Casual benchmarking: you benchmark A, but actually measure B, and conclude you’ve measured C.

原文的例子：很多工具声称或暗示测的是**磁盘**性能，实际测的是**文件系统**性能。
差距可能是数量级——文件系统用缓存和缓冲把磁盘 I/O 换成了内存 I/O。
工具本身可能完全正常、确实在测文件系统，但**你关于磁盘的结论会错得离谱**。

原文还解释了为什么这件事对新手特别难：你没有判断数字是否可疑的直觉。
温度计把你所在房间显示成 1000 度，你会立刻知道不对；基准不会——它产出的是你不熟悉的数字。

### #6 忽略环境：默认配置跑出来的不是生产

> your test server is a default setup with default tunables parameters, default file system, etc. The production database servers are tuned for high disk IOPS, so testing on an untuned system is unrealistic: you missed first understanding the production environment.

原文场景：评估一套新数据库，配了台测试机跑数据库基准，后来才发现漏了一步——
测试机是**默认设置**（默认可调参数、默认文件系统等），而生产库是为高磁盘 IOPS 调过的。

### #7 忽略错误：全部请求被挡在门外

> the web server did nothing at all during the test, as all requests were blocked by a firewall. All requests.

原文的 web 服务器基准：跑测的人报告平均延迟超过 1 秒，不符合需求。
快速分析发现问题所在——**web 服务器在测试期间什么都没做，全部请求都被防火墙挡掉了**；
报出来的延迟是基准客户端超时并报错所花的时间。

### #8 忽略方差：稳态平均掩盖了突发

> Bursts of writes may cause real issues in production, such as by queueing the reads, but are not simulated if the benchmark applies steady average rates.

机制：基准工具常按真实世界特性的**平均值**施加稳定负载（例：磁盘负载平均 500 读/秒、50 写/秒，
工具就按这个速率或 10:1 的读写比来模拟）。但操作**速率会变**，**操作类型也会变**，甚至正交出现——
写可能每 10 秒突发一次（异步 write-back 刷脏），同步读却是平稳的。
突发写会在生产上造成真实问题（把读排队），而稳态平均的基准模拟不出来。
原文给的解法是让基准用**马尔可夫模型**来模拟方差（见 §12.2.2）。

### #9 忽略扰动：时长不够就测不稳

> As a rule, the duration of a benchmark should not be shorter than one second.

原文的应对策略是**把基准跑长**（用分钟而不是秒）。短测试容易被异常扰动：
设备中断（把线程钉住去执行中断服务例程）、内核的 CPU 调度决策（为保持 CPU 亲和性而推迟迁移排队线程）、
CPU 缓存的冷热效应。原文要求**跑多次并检查标准差，越小越好**；
同时要采集数据以便事后研究扰动——比如采集操作延迟的**分布**（而不只是总时长），让离群点可见并可追溯。

### #11 基准悖论：测得越多越可能输

> “If your product’s chances of winning a benchmark are 50/50, you’ll usually lose.”

机制是简单概率：客户按性能买产品时，往往要**很有把握**，于是不跑一个基准，而是跑好几个、
并且希望**全赢**。单个基准 50% 胜率时，连赢三个的概率是 0.5 × 0.5 × 0.5 = 0.125 = 12.5%。
基准越多、要求全胜，胜算越差。

### #15 基准特设：行业用规范封死的那条路

术语出自 1993 年的 TPC-A 争议：Standish Group 指控 Oracle 在数据库软件里加了一个
「离散事务」特殊选项，唯一目的就是抬高 TPC-A 成绩；Oracle 抗辩说自己遵守了规范条文，
而规范里根本没写 benchmark specials。TPC 于是加了反特设条款（本节锚点）：

> All “benchmark special” implementations that improve benchmark results but not real-world performance or pricing, are prohibited.

还有一路是**特殊定价**——用客户根本拿不到的深度折扣来抬高数字。TPC 的定价要求规定：
总价必须落在客户实际会付的价格的 2% 以内。原文也提醒：这些例子是解释概念用的，
TPC 早在多年前就在规范里处理了这些问题，今天不一定还能遇到。

### #12 / #14 / #16 剩下的三条商业向失败

- **#12 Benchmarking the Competition**：客户买回去会用上几个月，并在这期间分析、调优、
  把最糟的问题在前几周就抖出来；**你没有几周时间去调优竞品**，只能拿到「未调优、因而不真实」的结果。
  竞品的客户（正是营销想打动的人）看到未调优的结果，反而让你在目标人群面前失去信誉。
  真要测竞品，就得给竞品花同样的调优功夫，再做正面对比。
- **#14 Misleading Benchmarks**：常见形态是**省略信息**（不披露价格、只说「我们快 2 倍」），
  或者第三方受托方**按结果挑人**（挑的是能又快又便宜交付的，非专业性反而是优势）。
  但这类结果仍有价值——原文明确说它们可以作为**性能上界**来用（友好火力的情况除外）。
- **#16 Cheating**：编造数字。原文说这很少见甚至不存在，他没见过纯粹凭空编的数字被公开。

## §12.2.1 Micro-Benchmarking：微基准

**机制**：微基准用人工负载测某**一种**特定操作——单类文件系统 I/O、数据库查询、CPU 指令、系统调用。
它的优势来自简单（本节锚点）：

> The advantage is the simplicity: narrowing the number of components and code paths involved results in an easier target to study and allows performance differences to be root-caused quickly.

即：涉及的组件与代码路径被收窄 ⇒ 更容易研究、性能差异能快速**定位到根因**；
其他组件带来的波动被尽量排除 ⇒ 结果通常可重复；测起来快；而且因为是刻意人工的，
**不容易与真实负载模拟混淆**。

**但微基准的结果必须被映射回目标负载**——一个微基准可能测了多个维度，
只有一两个与目标相关；对目标系统做性能分析或建模，才能判断哪些微基准结果适用、适用到什么程度。

原文按资源类型列的工具：CPU 用 SysBench；内存 I/O 用 lmbench；文件系统用 fio；
磁盘用 hdparm、dd 或 fio 加 direct I/O；网络用 iperf。原文同时提醒 Traeger 的结论
「大多数流行基准都有缺陷」。也可以自己写：要尽量简单，识别出负载中**可以单独测试的属性**，
并用外部工具验证它确实做了声称的操作。

### Design Example：文件系统微基准的测试矩阵

原文用「顺序/随机 × I/O 大小 × 方向」三个维度，设计了五个测试（Table 12.1）：

| # | 测试 | 目的 |
|---|---|---|
| 1 | 顺序 512 字节读 | 测最大（现实的）IOPS |
| 2 | 顺序 1 MB 读 | 测最大读吞吐 |
| 3 | 顺序 1 MB 写 | 测最大写吞吐 |
| 4 | 随机 512 字节读 | 测随机 I/O 的影响 |
| 5 | 随机 512 字节写 | 测重写的影响 |

两个脚注里的机制点值得记：**IOPS 最大化**要用更小更多的 I/O（1 字节看着更极致，
但磁盘至少会向上取整到扇区大小——512 字节或 4 KB）；**吞吐最大化**要用更大更少的 I/O
（省下 I/O 初始化时间），但存在「甜点」——原文例子是 Solaris 内核在 128 KB I/O 时最好，
因为那正是最大的 slab cache 大小，更大的 I/O 会落到 oversize arena，性能反而更差。

这五个测试再乘两个因子：**工作集大小**（远小于主存 ⇒ 数据全缓存在文件系统缓存里，
考察文件系统软件本身；远大于主存 ⇒ 尽量排除缓存影响，把基准推向磁盘 I/O）
与**线程数**（单线程 ⇒ 按当前 CPU 时钟速度测文件系统；多线程打满所有 CPU ⇒ 测系统 + 文件系统 + CPU 的最大性能）。
矩阵会迅速爆炸，所以要用统计分析技术把必需的测试集缩减。

**晴天与雨天**：原文把只关注最高速度的基准称为 sunny day performance testing；
为了让问题不被漏掉，还要做 cloudy day / rainy day performance testing——
测非理想情形：**争用、扰动、负载方差**（正对应 §12.1.3 的 #8 与 #9）。

## §12.2.2 Simulation：负载模拟（宏基准）

**机制**：模拟客户应用负载的基准（原文也叫 macro-benchmarks），通常基于生产环境的
**负载刻画**（见 [[Chapter 2. Methodologies]]）来确定要模拟什么。
原文的例子：某生产 NFS 负载的构成是读 40%、写 7%、getattr 19%、readdir 1% 等等，
其他特征也可以测量并模拟。

**优势**：结果能接近真实客户端行为；可以一次性涵盖微基准很难同时覆盖的多个因素，
也能包含复杂系统交互带来的效应——这些在微基准里往往完全看不到。
历史例子：Whetstone（1972 年，模拟当时的科学计算负载）与 Dhrystone（1984 年，模拟当时的整数负载）。
HTTP 负载生成器（wrk、siege、hey）可用来评估软硬件改动，也可模拟峰值负载
（例：在线购物平台的「闪购」）以暴露瓶颈，再去分析和解决。

**无状态 vs 有状态**：无状态模拟里每个服务器请求与前一个无关（例如按测得概率随机挑操作类型）；
有状态模拟里每个请求依赖客户端状态，至少依赖上一个请求——
NFS 的读写往往成组到达，「写后跟写」的概率远高于「读后跟写」。这类负载更适合用马尔可夫模型
（本节锚点）：

> Such a workload can be better simulated using a Markov model , by representing requests as states and measuring the probability of state transitions

**缺点**：模拟会忽略方差（回到 §12.1.3 #8）；客户使用模式会随时间变化，模拟必须跟着更新调整；
而如果已经基于旧版基准发布过结果，就会有阻力——旧结果没法和新版本做比较了。

## §12.2.3 Replay：轨迹回放

**机制**：把捕获的 trace log 回放到目标上，用真实的客户端操作测它的性能。
听起来理想——像在生产环境里测一样。但它是有问题的（本节锚点）：

> when characteristics and delivered latency change on the server, the captured client workload is unlikely to respond naturally to these differences, which may prove no better than a simulated customer workload.

即：**服务端特性与交付延迟一变，捕获来的客户端负载不会自适应**——
它不会因为变慢而少发一点、不会因为变快而多发一点，因此回放可能并不比模拟负载更好；
过度信任它，情况反而更糟。

**原文的假想案例**：客户考虑升级存储，把当前生产负载 trace 后回放到新硬件，
结果性能更差，丢单。原因是回放发生在**磁盘 I/O 层**：老系统是 10K rpm 磁盘，
新系统是更慢的 7200 rpm；但新系统有 **16 倍**的文件系统缓存和更快的处理器。
真实的生产负载本会变好（大部分从缓存返回），而回放磁盘事件没有模拟出这一点。
原文也提醒：即使回放层级选对了，**细微的时序效应**照样会毁掉结果——所以任何基准都必须分析。

## §12.2.4 Industry Standards：行业标准基准

**机制**：由独立组织提供，目标是**公平且相关**。通常是若干定义良好、文档齐全的微基准与负载模拟，
必须在特定指南下执行才能得到预期结果；厂商可以参与（通常付费），从而拿到执行软件；
厂商的结果通常要求**完整披露被测环境**，并且**可以被审计**。

**对客户的价值**：省时间——各家厂商的结果本来就公开了；客户的任务是找出
**最接近自己未来或当前生产负载**的那个基准（当前负载用负载刻画来确定）。

**历史**：1985 年 Jim Gray 等人的《A Measure of Transaction Processing Power》说明了
测量价格性能比的必要性，给出 Sort、Scan、DebitCredit 三个可执行的基准，
并建议以 DebitCredit 为基础建立行业标准的每秒事务数（TPS）度量——像汽车的 mpg 那样使用。
Jim Gray 的工作后来促成了 TPC 的成立。
扮演类似角色的度量还有 **MIPS**（百万指令每秒；问题在于所执行的指令类型不同，
不同处理器架构之间难以比较）与 **FLOPS**（每秒浮点运算次数）。
关键机制：**行业基准通常度量的是一个自定义指标，只服务于与它自己的比较。**

**TPC**（数据库性能导向）：TPC-C 模拟完整计算环境，一批用户对数据库执行事务；
TPC-DS 模拟决策支持系统（含查询与数据维护）；TPC-E 是 OLTP 负载，
建模经纪公司数据库（客户产生交易、账户查询、市场研究相关事务）；
TPC-H 模拟决策支持基准（ad hoc 查询 + 并发数据修改）；
TPC-VMS（TPC Virtual Measurement Single System）为虚拟化数据库采集其他基准；
TPCx-HS 是 Hadoop 大数据基准；TPCx-V 测虚拟机中的数据库负载。TPC 结果在线公开并**包含价格性能比**。

**SPEC**：SPEC Cloud IaaS 2018 测配置、计算、存储与网络资源，使用多个多实例负载；
SPEC CPU 2017 测计算密集型负载（整数与浮点），并可选能耗指标；
SPECjEnterprise 2018 Web Profile 测 Java EE Web Profile 7 及以上应用服务器、数据库与支撑设施的全系统性能；
SPECsfs2014 模拟客户端文件访问负载（NFS、CIFS 等）；
SPECvirt_sc2013 测虚拟化环境的端到端性能（虚拟硬件、平台、客户操作系统与应用软件）。
SPEC 结果在线公开，**包含调优方式与部件清单，但通常不含价格**。

## §12.3.1 Passive Benchmarking：被动基准测试（反面教材）

**机制**：fire-and-forget——跑起来就放着，直到跑完；主要目标是**收集基准数据**。
原文明确把它当作「相对于主动基准测试的一种反方法论」来写。

典型步骤：挑一个基准工具 → 用各种选项跑一遍 → 做一叠幻灯片 → 交给管理层。

结果可能：因基准软件 bug 而**无效**；被基准软件自己**限制**（例如单线程）；
被与基准目标无关的组件**限制**（例如网络拥塞）；被**配置**限制（性能特性没开、不是最大配置）；
受**扰动**影响（不可重复）；或者**完全测错了东西**。

原文的代价评估：由厂商做，会制造假警报、浪费工程资源或导致丢单；
由客户做，会导致糟糕的产品选择，长期纠缠公司。

## §12.3.2 Active Benchmarking：主动基准测试

**机制**（本节锚点）：

> With active benchmarking, you analyze performance while the benchmark is running—not just after it’s done—using observability tools

即：**在基准运行期间**分析，而不是事后——用可观测性工具。
它能确认基准确实测了它声称测的东西、而且你确实理解那是什么；
它还能找出被测系统（**或基准自身**）真正的限制因素；分享结果时带上「遇到的限制」细节也很有帮助。
附带好处：这是练手性能可观测工具的好时机——负载是已知的，能看到这些工具怎么看它。
理想状态是：基准配置好后保持稳态运行**几小时甚至几天**，以便持续做分析。

### Analysis Case Study：bonnie++ 到底测了什么

bonnie++ 自称是测硬盘性能的套件。跑第一个测试的输出（截选）：

```
# bonnie++
[...]
Version  1.97       ------Sequential Output------ --Sequential Input- --Random-
Concurrency   1     -Per Chr- --Block-- -Rewrite- -Per Chr- --Block-- --Seeks--
Machine        Size K/sec %CP K/sec %CP K/sec %CP K/sec %CP K/sec %CP  /sec %CP
ip-10-1-239-21   4G   739  99 549247  46 308024  37  1845  99 1156838  38 +++++ +++
Latency             18699us     983ms     280ms   11065us    4505us    7762us
[...]
```

第一个测试是「Sequential Output / Per Chr」，bonnie++ 给出 739 KB/s。
**合理性检查**：如果真是「每字符一次 I/O」，那就意味着 739,000 IOPS——这台机器的磁盘不可能做到。

用 bpftrace 数块层事件：

```
# bpftrace -e 'tracepoint:block:* { @[probe] = count(); }'
Attaching 18 probes...
^C
@[tracepoint:block:block_dirty_buffer]: 808225
@[tracepoint:block:block_touch_buffer]: 1025678
```

**没有** `block:block_rq_issue`（发起）与 `block:block_rq_complete`（完成）——即没有块 I/O 发出，
只有缓冲区被弄脏。再用 cachestat(8) 看文件系统缓存状态：

```
# cachestat 1
    HITS   MISSES  DIRTIES HITRATIO   BUFFERS_MB  CACHED_MB
       0        0        0    0.00%           49        361
     293        0    54299  100.00%           49        361
     658        0   298748  100.00%           49        361
     250        0   602499  100.00%           49        362
[...]
```

往 I/O 栈上层查，在 VFS 层：

```
# bpftrace -e 'kprobe:vfs_* /comm == "bonnie++"/ { @[probe] = count(); }'
Attaching 65 probes...
^C
@[kprobe:vfs_fsync_range]: 2
@[kprobe:vfs_statx_fd]: 6
@[kprobe:vfs_open]: 7
@[kprobe:vfs_read]: 13
@[kprobe:vfs_write]: 1176936
```

确实是大量的 `vfs_write()`。再下钻写的大小（vfs_write 的字节数参数）：

```
# bpftrace -e 'k:vfs_write /comm == "bonnie++"/ { @bytes = hist(arg2); }'
Attaching 1 probe...
^C
@bytes:
[1]               668839 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[2, 4)                 0 |                                                    |
[4, 8)                 0 |                                                    |
[8, 16)                0 |                                                    |
[16, 32)               1 |                                                    |
```

绝大部分是 **1 字节**（16~31 字节区间那一次大概是 bonnie++ 打印「开始基准」的消息）。
**结论**（本节锚点）：

> By analyzing the benchmark while it is running (active benchmarking), we’ve learned that the first bonnie++ test is a 1-byte file system write, which buffers in the file system cache. It does not test disk I/O, as is implied by the descriptions of bonnie++.

**顺带**：man page 说有 `-b` 选项（no write buffering，每次写后调 fsync），但用 strace 看：

```
$ strace bonnie++ -b
[...]
write(3, "6", 1)                        = 1
write(3, "7", 1)                        = 1
write(3, "8", 1)                        = 1
write(3, "9", 1)                        = 1
write(3, ":", 1)                        = 1
[...]
```

**并没有每次写后都 fsync**；而它的 `-D`（direct I/O）选项在作者机器上失败——
所以在那台机器上根本没法做真正的「每字符磁盘写」测试。
⚠️ 原文还提醒：strace 本身开销很高，**用它期间的基准结果应丢弃**。

**再补一刀**：旧版 bonnie++ 允许 libc 先缓冲写入，VFS 写大小是 4 KB 或更大
（取决于 libc 与操作系统版本），这让**跨不同 libc 缓冲大小的操作系统**比较 bonnie++ 结果变得有误导性；
新版修了这个问题，于是**新结果不能与旧结果比较**。
原文也强调：bonnie++ 本身并不是特别差的工具，选它只是因为它知名、作者研究过、
而且这类发现并不罕见——**它只是一个例子**。

## §12.3.3 CPU Profiling：剖析基准软件本身

**机制**：把**被测目标**和**基准软件自己**的 CPU 剖析单独拎出来，是一种值得单列的方法论，
因为它常常能带来快速发现；它通常是主动基准调查的一部分。
目的是快速看清所有软件在干什么，同时把研究范围收窄到基准里**真正起作用的组件**。
用户态与内核态栈都能剖析（用户态见第 5 章，内核态与火焰图见第 6 章 §6.6）。

**案例**：一次磁盘微基准在新系统上吞吐更差，作者被要求查原因，
预期是磁盘或磁盘控制器不行、应该升级。他先用 USE 方法（见 §12.3.4）发现**磁盘并不忙**，
但有一些 system-time（内核）CPU 占用。磁盘基准里 CPU 通常不是有意思的分析目标，
但既然有内核 CPU 用量，就顺手快速剖一次，生成火焰图（原文 Figure 12.2）。
结果（本节锚点）：

> Browsing the stack frames showed that 62.17% of CPU samples included a function called zfs_zone_io_throttle(). I didn’t need to read the code for this function, as its name was enough of a clue: a resource control, ZFS I/O throttling, was active and artificially throttling the benchmark! This was a default setting on the new system (but not the older system) that had been overlooked when the benchmark was performed.

机制归纳（推演）：**新系统的默认设置里开了资源控制（限流），旧系统没开**——
于是「硬件更差」这个表象其实是限流造成的。这是基准环境差异伪装成硬件差异的典型形态。

## §12.3.4–§12.3.5 USE Method 与负载刻画

- **USE 方法**（第 2 章引入）：基准期间应用它，可以**确保找到一个限制**——
  要么某个组件（硬件或软件）到了 100% 利用率，要么你**根本没把系统推到极限**。
  这两条正好对应「结果虚高」和「结果偏低」两种最常见的报告失败。
- **负载刻画**（第 2 章引入）：通过刻画**生产负载**来判断某个基准与当前生产环境的关系，
  是「这个基准对不对得上我的生产负载」这个问题的唯一可靠解法（对应 §12.1.3 #5）。

## §12.3.6 Custom Benchmarks：自己写基准

**机制**：简单的基准可以自己写；程序尽量短，避免复杂度阻碍分析。

**语言选择**：C 通常是微基准的好选择，因为它贴近实际执行的东西——
但必须想清楚**编译器优化**对代码的影响：编译器可能认为输出没被使用、因而没必要计算，
就把简单的基准例程**优化掉**。原文的要求是：基准运行时用别的工具确认它的行为，
必要时**反汇编**编译产物，看真正会执行什么。
涉及虚拟机、异步 GC、动态运行时编译的语言要难调试、难精确控制得多；
但如果必须模拟用这些语言写的客户端（宏基准），那就得用它们。

**副产品**：自己写基准常能顺带暴露目标系统的细节。原文的例子——开发数据库基准时，
发现 API 支持各种能提升性能的选项，而**生产环境是在这些选项出现之前就建好的**，并没有用上。

**职责切分**：你的软件可以只负责**产生负载**（负载生成器），测量交给别的工具；
其中一种做法就是渐增负载（§12.3.7）。

## §12.3.7 Ramping Load：渐增负载

**机制**：小步加负载，测交付吞吐，直到触到极限；结果可以画成**可扩展性曲线**
（scalability profile），既可目视研究，也可用可扩展性模型分析（第 2 章）。

原文的例子（Figure 12.3）：文件系统与服务器随线程数扩展，每线程对**已缓存文件**做 8 KB 随机读，
线程逐个加入；系统峰值接近**每秒 50 万次读**。结果用 VFS 层统计核对过，
确认 I/O 大小确实是 8 KB，且峰值时传输量超过 **3.5 GB/s**。
负载生成器用 Perl 写的，短到可以整段放进书里（照抄原文）：

```perl
#!/usr/bin/perl -w
#
# randread.pl - randomly read over specified file.

use strict;

my $IOSIZE = 8192;                      # size of I/O, bytes
my $QUANTA = $IOSIZE;                   # seek granularity, bytes

die "USAGE: randread.pl filename\n" if @ARGV != 1 or not -e $ARGV[0];

my $file = $ARGV[0];
my $span = -s $file;                    # span to randomly read, bytes
my $junk;

open FILE, "$file" or die "ERROR: reading $file: $!\n";

while (1) {
        seek(FILE, int(rand($span / $QUANTA)) * $QUANTA, 0);
        sysread(FILE, $junk, $IOSIZE);
}

close FILE;
```

两个细节：用 `sysread()` 是为了**绕开缓冲**，直接调 `read(2)` 系统调用。

**用法**：这个程序用来微基准一台 NFS 服务器，从一批客户端并行执行、
各自在 NFS 挂载的文件上做随机读；结果（读/秒）是在 **NFS 服务器端**用 nfsstat(8) 等工具测的。
被访问的文件数量与总大小是受控的——**这就是工作集大小**，用来让一部分测试全部命中服务器缓存、
另一部分走磁盘（呼应 §12.2.1 的测试矩阵）。客户端农场上的实例数逐个增加，把负载推到极限，
同时画可扩展性曲线，配合 USE 方法的资源利用率，确认确实有资源被耗尽——这次是**服务器 CPU**，
于是引出后续的进一步优化调查。

**工具选择的反直觉结论**：作者用这套方法找到了 Sun ZFS Storage Appliance 的极限，
并作为官方结果发布（据其所知是当时的世界纪录）。他本来还写了一套 C 版本（通常他用 C），
但这次**没必要**：客户端 CPU 有富余，换成 C 虽然降低了客户端 CPU 占用，
却**不改变结果**，因为目标上碰到的是同一个瓶颈；其他更复杂的基准、别的语言，
也都没能超过这套 Perl 结果。

**纪律（本节锚点）**：随负载接近极限，排队延迟会显著、延迟随之上升；
**推过头，延迟会高到结果不再有意义**。要问自己：交付给客户的延迟能接受吗？

> By adding more and more clients, you manage to scrape past 1 million IOPS; however, all operations are now heavily queued, with average latency of over 50 ms, which is not acceptable! Which result do you give marketing? (Answer: 990,000 IOPS.)

## §12.3.8 Sanity Check：合理性检查

**机制**：检查结果里有没有**说不通**的特征——具体说，这个结果是否要求某个组件
**超过它已知的上限**：网络带宽、控制器带宽、互连带宽、磁盘 IOPS。
只要超了，就值得深入调查；多数情况下，这个练习最终会发现**基准结果是假的**。

原文的例子（本节锚点）：

> This is over 3.2 Gbits/s—well in excess of the 1 Gbit/s known limit. Something is wrong!

上下文：NFS 服务器用 8 KB 读、被报称交付 50,000 IOPS，只连了一个 1 Gbit/s 以太口。
驱动这 50,000 IOPS 需要 50,000 × 8 KB = 400,000 KB/s，加上协议头——**超过 3.2 Gbit/s，
远超 1 Gbit/s 的已知上限**。这类结果通常意味着：基准测的是**客户端缓存**，
没有把整个负载真正压到 NFS 服务器。

原文还列了它在单个 1 Gbit/s 接口上见过的假吞吐：120 MB/s（0.96 Gbit/s）、200 MB/s（1.6）、
350 MB/s（2.8）、800 MB/s（6.4）、1.15 GB/s（9.2）——**都是单向**结果。
120 MB/s 大致正常（1 Gbit/s 实际约 119 MB/s）；200 MB/s 只有在双向流量被合计时才可能；
350 MB/s 及以上明显是假的。

**手法**：拿到一个基准结果，先找**能对给定数字做的简单求和/乘法**，用已知上限去卡它。
有系统访问权时可以继续做实验，按科学方法推进：当前要检验的问题就是
「这个基准结果是否有效」，由此提出假设与预测，再去验证。

## §12.3.9 Statistical Analysis：统计分析

**三个阶段**：① 选定基准工具、配置，以及要采集的系统性能指标；
② 执行基准，收集大量结果与指标数据；③ 用统计分析解释数据，产出报告。

**三种方法的分工**（这是本章的方法论骨架）：主动基准测试关注**跑的时候分析系统**；
统计分析关注**分析结果**（且它是「跑的时候不分析、事后集中分析」）；
被动基准测试则是**完全不分析**。

**适用场景**：接触大系统的机会既受限又昂贵——例如只有一台「最大配置」机器，
却有很多团队要同时用它：销售（PoC 里跑模拟客户负载，展示最大配置能交付什么）、
市场（拿最好的数字做营销）、支持（调查只在大配置机、严重负载下才出现的病理）、
工程（测新特性与代码改动）、质量（非回归测试与认证）。
每个团队能占用机器的时间有限，但事后分析的时间多得多。

**关键纪律**：指标采集昂贵 ⇒ **额外花力气保证指标可靠可信**，避免出问题后不得不重采。
除了技术上的生成方式，还要多采统计属性（变化度、完整分布、误差范围等）以便更早发现问题。
做代码改动或非回归测试时，**理解变化与误差范围尤其关键**，否则读不懂一对结果。

**取证数据**：尽可能多地收集运行中系统的性能数据（但不要因为采集开销损害结果），
以便事后做取证分析：sar(1)、监控产品、自定义工具导出所有可用统计。
Linux 上可以写个 shell 脚本，在跑之前与跑之后拷贝 `/proc` 的计数文件
（脚注警告：**别用 tar(1)**，它会被 /proc 里的零字节文件搞混，而且不会读它们的内容）；
也可以在基准期间按间隔执行，只要性能开销可接受。

**分析方法**：还包括可扩展性分析，以及用**排队论**把系统建模成排队网络（第 2 章，另有专书）。

## §12.3.10 Benchmarking Checklist：六问清单

原文受 Performance Mantras 清单（第 2 章 §2.5.20）启发，做了一份基准评测问题清单（Gregg 18d）：

| 六问 | 问的是什么 | 对应失败 |
|---|---|---|
| Why not double? | 为什么操作率不是基准结果的两倍？**这是在问限制因素是谁** | §12.1.3 #5、§12.3.4 |
| Did it break limits? | 有没有突破已知上限 | §12.3.8 合理性检查 |
| Did it error? | 有没有报错 | §12.1.3 #7 |
| Does it reproduce? | 结果有多一致 | §12.1.3 #9 |
| Does it matter? | 这个基准测的负载跟我的生产需求有关吗 | §12.1.3 #5 |
| Did it even happen? | 事情到底发生了没有 | §12.1.3 #7 |

第一条是整张清单的核心（本节锚点）：

> Why not double? Why was the operation rate not double the benchmark result? This is really asking what the limiter is. Answering this can solve many benchmarking problems, when you discover that the limiter is not the intended target of the test.

即：**「为什么不翻倍」=「限制因素是谁」**。回答它往往能解决大部分基准问题——
因为你常会发现限制因素并不是这次测试本来想测的那个目标。
「Did it even happen」呼应 §12.1.3 的 Ignoring Errors：防火墙挡掉全部请求、把超时当成延迟报出来。
「Does it matter」提醒：有些微基准测的是单个系统调用或库调用，**你的应用可能根本不用它们**。

§12.4 给出的是更长的同类清单，适合**拿不到目标系统、只能读别人的结果**时使用。

## §12.4 Benchmark Questions：拿到一份结果时该问什么

目标是搞清楚**到底测了什么**、结果有多真实/可重复。以下按原文分类整理：

**通用**
- 这个基准与我的生产负载相关吗？
- 被测系统是什么配置？测的是单机还是一组集群？
- 被测系统成本多少？
- 基准客户端是什么配置？
- 测试时长多久？采集了多少条结果？
- 结果是平均值还是峰值？平均值是多少？
- 还有哪些分布细节（标准差、百分位，或完整分布）？
- 基准的**限制因素**是什么？
- 操作的成功/失败比例是多少？
- 操作的属性是什么？这些属性是为了模拟负载而选的吗？怎么选的？
- 基准模拟了方差，还是只跑了平均负载？
- 结果是否用其他分析工具确认过？（要截图）
- 结果能否表达为一个**误差范围**？
- 结果可复现吗？

**CPU / 内存相关**
- 用的什么处理器？是否超频？是否用了定制散热（如水冷）？
- 用了多少内存条？怎么插在插槽上的？有没有禁用 CPU 核心？
- 全系统 CPU 利用率是多少？（轻载系统因为 turbo 更高反而可能更快）
- 测的是物理核还是超线程？
- 装了多少主存？什么类型？有没有自定义 BIOS 设置？

**存储相关**
- 存储设备配置（用了多少块、什么类型、什么存储协议、RAID 怎么配、缓存多大、write-back 还是 write-through）？
- 文件系统配置（类型、数量、是否开 journaling，以及各项调优）？
- 工作集多大？工作集缓存到什么程度？缓存在哪一层？
- 访问了多少个文件？

**网络相关**
- 网络配置（几个接口、什么类型、怎么配）？网络拓扑？
- 用了哪些协议？socket 选项？
- 网络栈设置调过吗？TCP/UDP 的可调参数呢？

研究行业基准时，很多问题可以直接从披露细节里找到答案。

## 章末选型判据

**先看场景，再选方法**：

| 场景 | 用什么 | 别用什么 |
|---|---|---|
| 证明某个组件更快 | 微基准（§12.2.1）+ USE 方法确认瓶颈就是它（§12.3.4） | 拿微基准数字直接对客户承诺性能提升 |
| 评估整机 / 整个栈 | 负载模拟（§12.2.2），参数来自生产负载刻画（§12.3.5） | 用稳态平均负载模拟有方差的真实负载（§12.1.3 #8） |
| 复现线上真实行为 | 记录 trace 供离线分析 | 用回放结果做采购决策（§12.2.3） |
| 只有一台大机器、多个团队排队 | 走统计分析流程（§12.3.9）：一次采集、多方分析 | 多人同时抢机器跑基准（§12.3.1 被动式） |
| 拿到一份外部结果 | 先做合理性检查（§12.3.8），再按 §12.4 问清单 | 把厂商数字当事实（它只是上界） |
| 结果要对外发布 | 先确认自己的配置已调好且已推到极限（§12.1.3 #13），再披露价格与配置（§12.1.3 #14） | 只发布「快 2 倍」这种省略上下文的摘要 |
| 排查性能回退 | 固定环境重复跑，比标准差与分布（§12.1.3 #9、§12.3.9） | 单次结果对比 |

**五条硬规则（可直接落到流程里）**：

1. **单次基准时长 ≥ 1 秒**；每次至少重复若干次并检查标准差，越小越好（§12.1.3 #9）。
2. **任何要写进报告的数字，都必须能回答「限制因素是谁」**——即「为什么不翻倍」（§12.3.10）。
3. **任何吞吐数字，先用链路带宽 / 控制器带宽 / 磁盘 IOPS 的已知上限卡一遍**（§12.3.8）。
4. **报错率、客户端缓存命中、负载方差**这三项没查之前，不接受任何基准数字（§12.1.3 #7 / #8 / #5）。
5. **加压到饱和时同时看延迟分布**；延迟已不可接受就停在饱和前的那个数——要 990,000 IOPS 而不是 1,000,000（§12.3.7）。

**什么时候干脆别做基准测试**：新实例类型上线时，直接用生产负载灰度测比造一套基准更快更真
（§12.1.1 的云与 Netflix 段落）；故障排查优先用观测与 trace，而不是「跑个基准看看」——
基准能告诉你上限，排查需要的是归因（§12.1.2 的四问）。
