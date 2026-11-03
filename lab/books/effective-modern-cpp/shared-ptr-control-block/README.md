# shared-ptr 控制块：三个文件，一组对照

> 来源讲义：`Effective Modern C++/4. Smart Pointers（讲义）.md` §2 Item 19、§4 Item 21

## 对照关系

| 文件 | 控制块数 | 结局 |
| --- | --- | --- |
| `01-raw-ptr-no-count.cpp` | 0（裸指针） | 没人负责 delete → 泄漏 |
| `02-two-shared-ptr-from-one-raw.cpp` | 2 | double free，检测器报错 |
| `03-make-shared-single-block.cpp` | 1 | 正常析构一次 |

## 结论

教材原话：

> A consequence of these rules is that constructing more than one std::shared_ptr from a single raw pointer gives you a complimentary ride on the particle accelerator of undefined behavior.

一个裸指针 → 一个 `shared_ptr`，**不是**"计数变 1"，而是"开了第一块控制块"。
再从同一个裸指针开第二个 `shared_ptr`，就是第二块控制块，两块互不知情，各自认为引用计数是 1。

## 跑法

```bash
bash tools/run_all.sh          # 跳过 ub 文件
bash tools/run_all.sh --ub     # 连 ub 一起跑, 期望检测器报错
```

01 和 02 标了 `@ub: yes`，默认构建时会被跳过，避免污染正常构建。
UB 检测默认用 **valgrind**（免特权）；装了 `libasan` 后可设 `KB_LAB_ASAN=1` 切到 ASan。
