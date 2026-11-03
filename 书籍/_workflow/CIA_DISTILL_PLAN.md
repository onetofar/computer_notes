# CiA 精简层派工表（15 篇）

原文稿：`/mnt/kb/markdown/书籍/教材原文/C++ Concurrency in Action/`
输出目录：`/mnt/kb/markdown/书籍/教材精简/C++ Concurrency in Action/`
输出文件名 = 原文稿 basename **去数字前缀**（`01-` / `02-` … 去掉），保留其余全部字符。

| 组 | 篇数 | 输出文件 | 原文稿 |
|---|---|---|---|
| A | 3 | `Chapter 1. Hello, world of concurrency in C++!.md`<br>`Chapter 2. Managing threads.md`<br>`Chapter 3. Sharing data between threads.md` | `01-` / `02-` / `03-` |
| B | 2 | `Chapter 4. Synchronizing concurrent operations.md`<br>`Chapter 5. The C++ memory model and operations on atomic types.md` | `04-` / `05-` |
| C | 2 | `Chapter 6. Designing lock-based concurrent data structures.md`<br>`Chapter 7. Designing lock-free concurrent data structures.md` | `06-` / `07-` |
| D | 2 | `Chapter 8. Designing concurrent code.md`<br>`Chapter 9. Advanced thread management.md` | `08-` / `09-` |
| E | 2 | `Chapter 10. Parallel algorithms.md`<br>`Chapter 11. Testing and debugging multithreaded applications.md` | `10-` / `11-` |
| F | 3 | `附录A-Appendix A. Brief reference for some C++11 language features.md`<br>`附录B-Appendix B. Brief comparison of concurrency libraries.md`<br>`附录C-Appendix C. A message-passing framework and complete ATM example.md` | `附录A-` / `附录B-` / `附录C-` |
| G | 1 | `附录D-Appendix D. C++ Thread Library reference.md` | `附录D-` |

## 每篇统一要求
- frontmatter `category: 并发编程`
- `source`: `《C++ Concurrency in Action 第二版》（Anthony Williams）第N章 <标题>`
- `source_section`: `精读 §X.Y / §Z.W；<不立篇的节> 不立篇（<理由>）`
- 锚点 12–20 条，第 2 列 `<原文稿文件名>.md L<行号>`，第 3 列逐字摘录
- 结尾 `## 章末选型判据`
- 校验：`cd /mnt/kb && python3 -I markdown/_tools/kb_anchor_check.py "<路径>"`
