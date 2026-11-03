# 对照讨论：线格式怎么定（手写 vs 反射 · 定长 vs 变长 · 字节序）

> **问题**：同一只猫发上网，**字节该怎么排**？写成固定宽度还是按需变长？用主机序还是网络序？
> 代码里写死每一对 `Write/Read`，还是让一张成员表驱动一个通用 `Serialize`？

## 对照表

| 文件 | 差异点 | 预期输出 |
| --- | --- | --- |
| `01-hand-written-fields.cpp` | 手写 `Write`/`Read` 镜像对，字段定长 | `字节数=30`；读回逐字段一致；name 定长槽位 16 字节只装了 5 字节 |
| `02-reflective-table.cpp` | 通用 `Serialize`：成员表（名字+类型+偏移）+ 按类型分发 | `字节数=30`，线格式与 01 **逐字节相同**；加字段只改表一处 |
| `03-variable-length-and-byte-order.cpp` | 定长 vs 变长（varint + 长度前缀）；主机序 vs 网络序 | `定长字节数=30 变长字节数=12`；`0x12345678` 两种序字节不同 |
| `04-unaligned-read-ub.cpp` | 把字节流偏移 1 处按 `uint32` 解释（不对齐、不顾别名） | `runtime error: ... 未满足 4 字节对齐`（`@ub: yes`，检测到即算通过） |

横向看：01 与 02 的**线格式完全相同**——换的是"怎么写"，不是"发什么"。
03 把同一只猫从 30 字节压到 12 字节。04 是 01/02 都没覆盖的地雷。

## 教材原话（逐字引文）

> **别发对象，发字段**——每个字段自己决定怎么进流。

> **写头/读头对偶**：输出流推进 `mHead` 追加数据；输入流从 0 开始按同样顺序读。**写端什么顺序写，读端就必须什么顺序读**——流里没有字段名，只有位置，错一位后面全错；

> 小端平台（x86）把 `0x12345678` 在内存里存成 `78 56 34 12`，大端平台存成 `12 34 56 78`。跨平台传输多字节数字时必须统一。工程做法：**流自己定一个字节序，平台与流不一致时，写入/读出前做字节交换**

> `mName[128]` 装着 "Fuzzy" → 先 `strlen` 发 1 字节长度，再发 6 字节内容，省下 122 字节。

> 每个新数据结构都要写一对必须**手动保持镜像**的 Read/Write——改了写法忘了读法，错误不在当场，在远端某台机器错位读。

> - **数据驱动的序列化**：干脆不写逐字段的代码——给每个类挂一张"成员变量表"（名字 + 类型 + 内存偏移，偏移用自定义 `OffsetOf` 宏算），一个通用 Serialize 循环查表偏移、按类型分发。C++ 没有反射，这张手写/工具生成的表就是穷人版反射。以少量性能换代码量骤减，副产物是同一套元数据还能服务存盘、GC、GUI 编辑器。

—— 第4章《对象序列化（讲义）》§4.2 / §4.4 / §4.5

## 跑法

```bash
cd lab
bash tools/check_one.sh books/game-arch/wire-format-encoding/01-hand-written-fields.cpp
bash tools/check_one.sh books/game-arch/wire-format-encoding/02-reflective-table.cpp
bash tools/check_one.sh books/game-arch/wire-format-encoding/03-variable-length-and-byte-order.cpp
bash tools/check_one.sh books/game-arch/wire-format-encoding/04-unaligned-read-ub.cpp --ub
```

或直接编译：

```bash
g++ -std=c++20 -O2 -Wall -Wextra -pthread \
    books/game-arch/wire-format-encoding/03-variable-length-and-byte-order.cpp -o /tmp/kb-lab-build/wfe03
```

> 注：`04` 是故意演示未定义行为的文件，`run_all.sh` 默认跳过它，`run_all.sh --ub` 才跑。
