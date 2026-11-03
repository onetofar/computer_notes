# kb-lab · 教材讲义的可执行对照实验

> 这个目录不是笔记，是**能编译、能运行、能对比**的实验场。
> 每个主题目录对应一本教材的一个具体问题，给出 2~4 个各自独立的实现，输出可直接横向对比。

## 它和知识库的关系

| 层 | 位置 | 作用 |
| --- | --- | --- |
| 讲义 | `书籍/教材讲义/<书名>/` | 教材原文的讲解版，是**唯一的原料来源** |
| 实验 | `markdown/lab/books/<slug>/` | 把讲义里的机制变成可运行的对照代码 |
| 部署 | 服务器 `~/dev/kb-lab/` | 编译运行的环境（g++ 16.2.1 / valgrind / perf） |

**代码不复制教材结论，只复现教材机制。** 教材原话放在每个主题目录的 README 里，逐字引用。

## 规模

- 10 本书 · 66 个对照主题 · 222 个可执行实现 · 约 12,400 行
- 语言：C++ 157 / C 51 / Shell 8 / Python 6
- 其中 21 个是「故意演示未定义行为」的文件（`@ub: yes`），由 valgrind 检出

## 目录结构

```
lab/
├── books/
│   ├── <book-slug>/            # 一本书
│   │   ├── README.md           # 该书的索引
│   │   └── <topic-slug>/       # 一个对照讨论
│   │       ├── README.md       # 对照表 + 教材原话 + 跑法
│   │       └── NN-*.cpp        # 2~4 个可独立运行的实现
├── tools/
│   ├── lab_check.py            # 守卫: 结构 / 元信息 / 颗粒度
│   ├── check_one.sh            # 编译并运行单个文件
│   ├── run_all.sh              # 编译并运行全部
│   ├── build_all.sh            # 只做语法检查
│   ├── gen_index.py            # 生成书目索引
│   ├── sync_check.py           # 写后校验: 文件是否真在盘上
│   └── deploy.sh               # 同步到服务器
└── .gitignore
```

## 书目索引

| slug | 教材 | 主题数 |
| --- | --- | --- |
| `effective-cpp` | Effective C++（第三版中文） | 7 |
| `effective-modern-cpp` | Effective Modern C++ | 7 |
| `cpp-concurrency-in-action` | C++ Concurrency in Action | 8 |
| `linux-multithread-server` | Linux 多线程服务端编程 | 7 |
| `csapp` | 深入理解计算机系统 | 7 |
| `os-concepts` | 操作系统导论 | 7 |
| `systems-performance` | Systems Performance | 6 |
| `game-arch` | 网络多人游戏架构与编程 | 7 |
| `game-dev` | 网络多人在线游戏开发实战教程 | 7 |
| `linux-kernel` | Linux Kernel Programming | 3 |

每本书的主题目录清单见 `books/README.md`。

## 硬规矩

1. **一个主题目录 = 一个对照讨论**，至少 2 个实现文件，各自独立可运行
2. 单文件 ≤ 120 行，只验证一个机制
3. 文件头必须有 `@book @topic @source @expect` 四行元信息
4. 实现文件必须住在主题目录里，不能直接丢在书目目录下
5. 故意演示未定义行为的文件标 `@ub: yes`，默认跳过；`--ub` 才跑，期望检测器报错
6. 教材引文只放 README，代码里不写引文
7. 零编译警告（`-Wall -Wextra`）

## 跑法

```bash
# 本地 / 服务器通用
python3 tools/lab_check.py     # 守卫: 结构 / 元信息 / 颗粒度
bash tools/run_all.sh           # 编译并运行全部（跳过 @ub）
bash tools/run_all.sh --ub      # 连 @ub 一起跑，期望检测器报错
bash tools/build_all.sh         # 只做语法检查

# 部署到服务器
bash tools/deploy.sh fedora     # -> /home/xier/dev/kb-lab/
```

## 运行环境

- 服务器：`fedora` (192.168.1.13)，`~/dev/kb-lab/`，与游戏服务器 `~/dev/game-project/otf-server/` **完全分开**
- 编译器：g++ 16.2.1 / gcc 16.2.1，C++20，C11
- UB 检测：valgrind（免特权）；装了 `libasan` 可设 `KB_LAB_ASAN=1` 切 ASan
- 编译产物落 `/tmp/kb-lab-build/`，**不写进 vault**（避免污染 Obsidian 索引）

## 为什么在 vault 里

这个目录在 Obsidian vault 的 `markdown/` 下，因此：

- 讲义和实验同在 `markdown/` 一棵树里，引用关系一眼可见
- `markdown/` 是独立 git worktree（有远端 `computer_notes.git`），实验代码跟着备份
- 服务器通过 CIFS 挂载同一份 vault，改完即见

实验代码本身用 `.cpp/.c/.sh/.py` 后缀，Obsidian 不会索引为笔记；每个主题目录的 `README.md` 是唯一会被索引的文件——它是给人读的索引，不是知识库正文。
