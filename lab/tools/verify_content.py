#!/usr/bin/env python3
"""Content verifier for kb-lab -- enforces the contracts the structure guard cannot.

CONTRACT:
  * A markdown blockquote ("> ") in a topic README is ALWAYS a verbatim lecture quote.
    Lab-authored commentary must be ordinary prose, never "> ".
  * Every quote must appear character for character in one of that book lecture
    files (markdown emphasis and whitespace normalised only).
  * Every topic README must contain at least one verbatim quote.
  * Every implementation file @source must name a lecture file that exists in
    that book, and carry a section marker (e.g. "§2.1.4", "§Item 18", "Item 11")
    that occurs inside that lecture file.

Usage:
  python3 tools/verify_content.py            # all books
  python3 tools/verify_content.py --book X   # one book
  python3 tools/verify_content.py --near 0.7 # NEAR/FAR threshold
"""
import re
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent.parent
BOOKS = ROOT / "books"
LECTURES = ROOT.parent / "书籍" / "教材讲义"

IMPL_EXT = {".cpp", ".c", ".sh", ".py"}
SHINGLE = 12

BOOK_DIRS = {
    "effective-cpp": "Effective C++",
    "effective-modern-cpp": "Effective Modern C++",
    "cpp-concurrency-in-action": "C++ Concurrency in Action",
    "linux-multithread-server": "Linux多线程服务端编程",
    "csapp": "深入理解计算机系统",
    "os-concepts": "操作系统导论",
    "systems-performance": "Systems Performance",
    "game-arch": "网络多人游戏架构与编程",
    "game-dev": "网络多人在线游戏开发实战教程",
    "linux-kernel": "Linux Kernel Programming",
}

META_PREFIXES = ("来源讲义", "讲义来源", "来源:", "出处", "以下引文", "教材原话见")

# section marker after § : "Item 18", "2.1.4", "12", "D.2" (appendix letter)
SECTION_RE = re.compile(r"§\s*((?:Item\s*)?[A-Za-z]?[0-9]+(?:\.[0-9]+)*)", re.I)
# a section marker written without § at the tail of @source: "Item 11", "11.2"
TAIL_RE = re.compile(r"((?:Item\s*)?[A-Za-z]?[0-9]+(?:\.[0-9]+)*)\s*$", re.I)


def strip_pua(s):
    return "".join(ch for ch in s if not (0xE000 <= ord(ch) <= 0xF8FF))


def norm(s):
    s = s.replace("**", "").replace("__", "").replace("`", "")
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def shingles(s):
    if len(s) < SHINGLE:
        return {s} if s else set()
    return {s[i:i + SHINGLE] for i in range(len(s) - SHINGLE + 1)}


def strip_bq(s):
    """Drop markdown blockquote markers from every line of a lecture file.

    Lectures quote the original book text inside "> " blocks, sometimes
    nested one level deeper ("> > item"). A lab README reproduces that
    structure verbatim, so the markers must not take part in the comparison
    -- they are structure, not content.
    """
    out = []
    for line in s.splitlines():
        st = line.lstrip()
        while st.startswith("> "):
            st = st[2:].lstrip()
        if st.startswith(">"):
            st = st[1:].lstrip()
        out.append(st)
    return "\n".join(out)


def load_lectures(slugs):
    out = []
    for slug in slugs:
        d = LECTURES / BOOK_DIRS.get(slug, slug)
        if not d.is_dir():
            continue
        for f in sorted(d.glob("*.md")):
            try:
                txt = norm(strip_bq(f.read_text(encoding="utf-8")))
            except Exception:
                continue
            out.append((f.name, txt, shingles(txt)))
    return out


def extract_blocks(text):
    res = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        s = lines[i].lstrip()
        if s.startswith("> ") or s.rstrip() == ">":
            start = i + 1
            buf = []
            while i < len(lines):
                s2 = lines[i].lstrip()
                if s2.startswith("> ") or s2.rstrip() == ">":
                    inner = s2[2:] if s2.startswith("> ") else ""
                    while inner.startswith("> ") or inner.startswith(">"):
                        inner = inner[2:] if inner.startswith("> ") else inner[1:]
                        inner = inner.lstrip()
                    buf.append(inner)
                    i += 1
                else:
                    break
            q = " ".join(x.strip() for x in buf if x.strip())
            if q.strip():
                res.append((start, q.strip()))
        else:
            i += 1
    return res


def check_quotes(slugs, threshold):
    lectures = load_lectures(slugs)
    problems = []
    checked = 0
    noquote = []
    for slug in slugs:
        root = BOOKS / slug
        if not root.is_dir():
            continue
        for td in sorted(root.iterdir()):
            if not td.is_dir():
                continue
            rm = td / "README.md"
            if not rm.is_file():
                continue
            text = rm.read_text(encoding="utf-8")
            rel = str(rm.relative_to(ROOT)).replace(chr(92), "/")
            real_quotes = 0
            for lineno, q in extract_blocks(text):
                if any(q.startswith(p) for p in META_PREFIXES):
                    continue
                nq = norm(q)
                if len(nq) < 10:
                    continue
                checked += 1
                hit = False
                best = 0.0
                where = ""
                qs = shingles(nq)
                for name, txt, ts in lectures:
                    if nq in txt:
                        hit = True
                        break
                    if qs and ts:
                        r = len(qs & ts) / len(qs)
                        if r > best:
                            best = r
                            where = name
                if hit:
                    real_quotes += 1
                    continue
                kind = "NEAR" if best >= threshold else "FAR"
                problems.append({"file": rel, "line": lineno, "kind": kind,
                                 "ratio": round(best, 2), "near": where,
                                 "text": q[:120]})
            if real_quotes == 0:
                noquote.append(rel)
    return checked, problems, noquote


def resolve_lecture(raw, files):
    """Find which lecture file an @source value refers to, by containment."""
    nraw = norm(strip_pua(raw))
    best = None
    best_len = 0
    for f in files:
        stem = norm(strip_pua(f.stem))
        if not stem:
            continue
        if stem in nraw and len(stem) > best_len:
            best = f
            best_len = len(stem)
    if best is not None:
        rest = nraw.replace(norm(strip_pua(best.stem)), " ")
        return best, rest
    # fall back: longest common prefix
    best = None
    best_len = 0
    for f in files:
        stem = norm(strip_pua(f.stem))
        if not stem:
            continue
        n = 0
        while n < min(len(stem), len(nraw)) and stem[n] == nraw[n]:
            n += 1
        if n > best_len and n >= 6:
            best = f
            best_len = n
    if best is not None:
        return best, ""
    return None, nraw


def extract_markers(rest):
    marks = [m.group(1).strip() for m in SECTION_RE.finditer(rest)]
    if not marks:
        tail = rest.strip(" .;:()（）")
        m = TAIL_RE.search(tail)
        if m:
            marks = [m.group(1).strip()]
    return marks


def marker_in_body(marker, raw):
    """Does the section marker occur inside the lecture body?

    raw keeps newlines so heading-anchored patterns can match; the previous
    implementation ran on whitespace-collapsed text, which made every
    "^#+..." pattern unreachable.
    """
    low = raw.lower()
    mk = marker.lower()
    if ("§" + mk) in low:
        return True
    # appendix-style markers such as "D.2" appear in prose as "D.2 ..."
    if re.search(r"(?<![A-Za-z0-9.])" + re.escape(mk) + r"(?![0-9])", low):
        return True
    if len(mk) >= 6 and mk in low:
        return True
    if mk.isdigit() and ("item " + mk) in low:
        return True
    for line in raw.splitlines():
        s = line.lstrip()
        if not s.startswith("#"):
            continue
        h = s.lstrip("#").strip()
        # drop a leading outline number such as "3. " or "2.8 "
        h = re.sub(r"^(?:[0-9]+(?:\.[0-9]+)*\.?)\s+", "", h)
        if re.match(r"^" + re.escape(mk) + r"(?:\s|[.、:：]|$)", h, re.I):
            return True
        m = re.search(r"§\s*([0-9][0-9.]*(?:\s*[-–—~]\s*[0-9][0-9.]*)*)", h)
        if m:
            for num in re.findall(r"[0-9]+(?:\.[0-9]+)*", m.group(1)):
                if num == mk:
                    return True
    return False


def check_sources(slugs):
    problems = []
    checked = 0
    for slug in slugs:
        root = BOOKS / slug
        if not root.is_dir():
            continue
        lecture_dir = LECTURES / BOOK_DIRS.get(slug, slug)
        files = list(lecture_dir.glob("*.md")) if lecture_dir.is_dir() else []
        if not files:
            continue
        for td in sorted(root.iterdir()):
            if not td.is_dir():
                continue
            for impl in sorted(td.iterdir()):
                if not impl.is_file() or impl.suffix.lower() not in IMPL_EXT:
                    continue
                rel = str(impl.relative_to(ROOT)).replace(chr(92), "/")
                head = chr(10).join(impl.read_text(encoding="utf-8", errors="replace").splitlines()[:20])
                m = re.search(r"@source:\s*(.+)", head)
                if not m:
                    continue
                checked += 1
                raw = m.group(1).strip()
                target, rest = resolve_lecture(raw, files)
                if target is None:
                    problems.append((rel, "FILE-NOT-FOUND", raw))
                    continue
                marks = extract_markers(rest)
                if not marks:
                    problems.append((rel, "NO-SECTION", "no section marker in @source"))
                    continue
                raw = target.read_text(encoding="utf-8", errors="replace")
                missing = [t for t in marks if not marker_in_body(t, raw)]
                if missing:
                    problems.append((rel, "SECTION-NOT-FOUND",
                                     ",".join(missing) + "  in  " + target.name))
    return checked, problems


def main():
    args = sys.argv[1:]
    slugs = [d.name for d in sorted(BOOKS.iterdir()) if d.is_dir()]
    threshold = 0.70
    if "--book" in args:
        want = args[args.index("--book") + 1]
        slugs = [s for s in slugs if want in s]
    if "--near" in args:
        threshold = float(args[args.index("--near") + 1])
    qc, qp, nq_files = check_quotes(slugs, threshold)
    sc, sp = check_sources(slugs)
    near = [p for p in qp if p["kind"] == "NEAR"]
    far = [p for p in qp if p["kind"] == "FAR"]
    print("=" * 60)
    print("quote : " + str(qc) + " blocks | " + str(len(near)) + " NEAR | " + str(len(far)) + " FAR | " + str(len(nq_files)) + " README without any verbatim quote")
    print("source: " + str(sc) + " @source | " + str(len(sp)) + " unresolvable")
    print("=" * 60)
    for p in qp:
        print("[" + p["kind"] + " " + str(p["ratio"]) + "] " + p["file"] + ":" + str(p["line"]) + "  " + p["text"])
    for rel in nq_files:
        print("[NO-QUOTE] " + rel)
    for f, kind, q in sp:
        print("[" + kind + "] " + f + "  " + q)
    if qp or sp or nq_files:
        return 1
    print("OK: quotes verbatim, @source resolvable, every topic has a quote")
    return 0


if __name__ == "__main__":
    sys.exit(main())
