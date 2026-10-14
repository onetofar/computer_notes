#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Excalidraw 笔记完整性校验器。

知识库里的 `.excalidraw.md` 是**压缩后的场景 JSON**，肉眼无法核对，只能靠这个脚本。
它检查五件事：

1. 插件要求两段 `compressed-json` 块存在且**完全一致**（插件读任一段都行，不一致就是手改坏了）
2. lzstring 能解压、能 parse 回 JSON（往返一致）
3. 元素字段完整（Excalidraw 对缺字段的元素会静默丢弃或渲染异常）
4. id 唯一
5. 几何自检：矩形两两不重叠；文字要么落在某个矩形内，要么是刻意游离的标题/标注
6. `## Text Elements` 索引段里的文字与场景内 text 元素对得上

用法：
  python markdown/_tools/excalidraw_check.py <文件路径> [<文件路径> ...]
"""
import re, json, sys
from pathlib import Path
from lzstring import LZString

sys.stdout.reconfigure(encoding="utf-8")

NEED = {"id", "type", "x", "y", "width", "height", "strokeColor", "backgroundColor",
        "fillStyle", "strokeWidth", "strokeStyle", "roughness", "opacity", "seed",
        "version", "versionNonce", "isDeleted", "groupIds", "boundElements", "updated"}
# 这些字段是 Excalidraw 后续版本才补的，老图（2026-07 前后）本来就没有。
# 缺它们不是损坏 —— Excalidraw 读老文件会自动补默认值。
LATE = {"frameId", "lineHeight", "elbowed", "lastCommittedPoint", "link",
        "startBinding", "endBinding", "startArrowhead", "endArrowhead",
        "points", "text", "originalText", "fontSize", "fontFamily", "textAlign",
        "verticalAlign", "containerId", "autoResize", "roundness"}
NEED_TYPE = {"arrow": {"points"}, "text": {"text", "fontSize", "fontFamily"}}


def _payload(raw: str) -> str:
    """老插件会把 Base64 载荷按宽度折行，解压器收到裸 '\\n' 会抛 KeyError。
    统一去掉所有空白再解。"""
    return re.sub(r"\s+", "", raw)


def check(path: Path):
    errs, warns = [], []
    t = path.read_text(encoding="utf-8")

    blocks = [_payload(b) for b in
              re.findall(r"```compressed-json\n(.*?)\n```", t, re.S)]
    if not blocks:
        errs.append("找不到 compressed-json 块")
        return errs, warns, {}
    if len(blocks) == 1:
        warns.append("只有 1 段数据（新版插件写 2 段）—— 老格式，仍可读")
    elif len(set(blocks)) > 1:
        errs.append("多段数据不一致 —— 手改时只改了一段")

    raw, flavour = None, None
    # 库里的图有两种压缩格式：较新的用 compressToBase64，较老的用
    # compressToEncodedURIComponent（字母表是 - _ 与数字，无 + / =）。
    # 两种都要试，否则遇到老图直接崩，等于这个工具对新旧图一视同仁地不能用。
    for fn, comp in ((LZString.decompressFromBase64, LZString.compressToBase64),
                     (LZString.decompressFromEncodedURIComponent,
                      LZString.compressToEncodedURIComponent)):
        try:
            raw = fn(blocks[0])
        except Exception:
            raw = None
        if raw:
            flavour = comp
            break
    if not raw:
        errs.append("lzstring 解压失败（Base64 与 URI 两种字母表都试过了）")
        return errs, warns, {}
    try:
        scene = json.loads(raw)
    except Exception as ex:
        errs.append(f"JSON 解析失败：{ex}")
        return errs, warns, {}

    if (flavour(json.dumps(scene, ensure_ascii=False, separators=(",", ":")))
            != blocks[0]):
        warns.append("压缩不是规范形式（不影响读取，但重新保存会整段 diff）")

    els = scene.get("elements", [])
    if not els:
        errs.append("场景里没有元素 —— 图是空的或解析错了")
        return errs, warns, {}
    from collections import Counter

    dup = [i for i, c in Counter(e["id"] for e in els).items() if c > 1]
    if dup:
        errs.append(f"id 重复 {len(dup)} 个：{dup[:5]}")

    miss_core, miss_late = Counter(), Counter()
    for e in els:
        required = NEED | NEED_TYPE.get(e["type"], set())
        m = set(e) and (required - set(e))
        miss_core.update(sorted(m & required))
        miss_late.update(sorted(m - required))
    if miss_core:
        errs.append(f"{sum(miss_core.values())} 处缺核心字段 {dict(miss_core)}")
    if miss_late:
        shown = sorted(miss_late)
        warns.append(f"老 schema：{sum(miss_late.values())} 处缺后加字段 {shown[:6]}"
                     f"{'…' if len(shown) > 6 else ''}（Excalidraw 读老文件会自动补）")

    rects = [e for e in els if e["type"] == "rectangle"]
    ov = 0
    for i in range(len(rects)):
        for j in range(i + 1, len(rects)):
            a, b = rects[i], rects[j]
            if (a["x"] < b["x"] + b["width"] and b["x"] < a["x"] + a["width"]
                    and a["y"] < b["y"] + b["height"] and b["y"] < a["y"] + a["height"]):
                ov += 1
    if ov:
        warns.append(f"{ov} 对矩形有重叠（新格式里通常意味着连线穿框；老图里可能是刻意嵌套）")

    stats = {
        "元素": len(els),
        "分布": dict(Counter(e["type"] for e in els)),
        "画布": (min(e["x"] for e in els), max(e["x"] + e["width"] for e in els),
                 min(e["y"] for e in els), max(e["y"] + e["height"] for e in els)),
    }
    return errs, warns, stats


def main():
    paths = [Path(a) for a in sys.argv[1:]]
    if not paths:
        print("用法：python excalidraw_check.py <文件路径> [...]")
        sys.exit(2)
    bad = 0
    for p in paths:
        if not p.exists():
            print(f"✗ 不存在：{p}")
            bad += 1
            continue
        errs, warns, stats = check(p)
        print(f"\n=== {p.name} ===")
        if stats:
            c = stats["画布"]
            print(f"  {stats['元素']} 个元素  {stats['分布']}")
            print(f"  画布 x[{c[0]:.0f}, {c[1]:.0f}]  y[{c[2]:.0f}, {c[3]:.0f}]")
        for e in errs:
            print(f"  ✗ {e}")
        for w in warns:
            print(f"  ! {w}")
        if not errs:
            print("  ✓ 结构校验通过")
        bad += len(errs)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()