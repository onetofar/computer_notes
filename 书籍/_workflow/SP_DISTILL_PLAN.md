# SP 精简层派工表（还差 18 篇）

原文稿：`/mnt/kb/markdown/书籍/教材原文/Systems Performance/`
输出目录：`/mnt/kb/markdown/书籍/教材精简/Systems Performance/`
输出文件名 = 原文稿 basename **去数字前缀**（`02-` … `16-`），保留其余全部字符。

**注意**：SP 原文稿目录名里用了**全角冒号 `：`**，文件名里要原样保留：
`附录A-Appendix A： USE Method： Linux.md` 等。

| 组 | 篇数 | 输出文件 |
|---|---|---|
| A | 3 | `Chapter 2. Methodologies.md` / `Chapter 3. Operating Systems.md` / `Chapter 4. Observability Tools.md` |
| B | 3 | `Chapter 5. Applications.md` / `Chapter 6. CPUs.md` / `Chapter 7. Memory.md` |
| C | 2 | `Chapter 8. File Systems.md` / `Chapter 10. Network.md` |
| D | 3 | `Chapter 11. Cloud Computing.md` / `Chapter 12. Benchmarking.md` / `Chapter 13. perf.md` |
| E | 3 | `Chapter 14. Ftrace.md` / `Chapter 15. BPF.md` / `Chapter 16. Case Study.md` |
| F | 4 | `附录A-…USE Method： Linux.md` / `附录B-…sar Summary.md` / `附录C-…bpftrace One-Liners.md` / `附录D-…Solutions to Selected Exercises.md` |
| G | 1 | `附录E-Appendix E： Systems Performance Who’s Who.md` |

已存在：`Chapter 1. Introduction.md`、`Chapter 9. Disks.md`。

## 每篇统一要求
- frontmatter `category: 性能工程`
- `source`: `《Systems Performance 第二版》（Brendan Gregg）第N章 <标题>`
- `source_section`: `精读 §X.Y / §Z.W；<不立篇的节> 不立篇（<理由>）`
- 锚点 12–20 条，第 2 列 `<原文稿文件名>.md L<行号>`，第 3 列逐字摘录
- 结尾 `## 章末选型判据`
- 校验：`cd /mnt/kb && python3 -I markdown/_tools/kb_anchor_check.py "<路径>"`
- SP 的 §-定位器：`§6.2.1`（原文稿 `#### 6.1 …` / `##### 6.2.1 …`）；SP 附录用 `§附录 A`（只有一级标题）
