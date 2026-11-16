#!/usr/bin/env python3
"""Report overlapping and overflowing items in a QML layout dump.

The dumps come from QmlResponsiveBoundsTest with PARAMIX_QML_DUMP_DIR set
(see docs/qml-responsive-layout.md). Usage:

    qml_layout_audit.py DUMP_DIR [--max N]

Exit status is 1 when any finding is reported.
"""
import json
import re
import sys
from collections import Counter
from pathlib import Path

TOLERANCE = 2.0


def short(cls):
    return re.sub(r"_QMLTYPE_\d+|_QML_\d+", "", cls)


def intersect(a, b):
    x0, y0 = max(a[0], b[0]), max(a[1], b[1])
    x1, y1 = min(a[2], b[2]), min(a[3], b[3])
    return (x0, y0, x1, y1) if x1 - x0 > TOLERANCE and y1 - y0 > TOLERANCE else None


def load(path):
    items = json.loads(path.read_text(encoding="utf-8"))
    for i, it in enumerate(items):
        it["i"] = i
        it["r"] = (it["x"], it["y"], it["x"] + it["w"], it["y"] + it["h"])
        parent = items[it["p"]] if it["p"] >= 0 else None
        # Visible rect: the item clipped by every clipping ancestor.
        clip = parent["cliprect"] if parent else None
        vis = it["r"]
        if clip:
            vis = intersect(vis, clip) or (0, 0, 0, 0)
        it["vis"] = vis
        own = it["r"] if it["clip"] else None
        if own and clip:
            own = intersect(own, clip) or (0, 0, 0, 0)
        it["cliprect"] = own or clip
        named = short(it["cls"])
        custom = "_QML" in it["cls"]
        it["path"] = ((parent["path"] + ">") if parent else "") + named if custom or not parent \
            else parent["path"]
    return items


def where(it):
    r = it["r"]
    return f"{it['path']} @{r[0]:.0f},{r[1]:.0f} {it['w']:.0f}x{it['h']:.0f}"


def audit(items):
    findings = []
    texts = [it for it in items if "text" in it and it["op"] > 0.05
             and it["vis"][2] - it["vis"][0] > 0]

    for t in texts:
        # Painted box: where the glyphs actually land, which is what overlaps.
        cw, ch = t["cw"], t["ch"]
        if cw > t["w"] + TOLERANCE and t["w"] > 0 and not t["trunc"]:
            findings.append(("text-wider-than-box",
                             f"'{t['text'][:30]}' needs {cw:.0f} has {t['w']:.0f}  {where(t)}"))
        if ch > t["h"] + TOLERANCE and t["h"] > 0:
            findings.append(("text-taller-than-box",
                             f"'{t['text'][:30]}' needs {ch:.0f} has {t['h']:.0f}  {where(t)}"))
        if t["trunc"] and len(t["text"]) <= 14:
            findings.append(("short-label-elided",
                             f"'{t['text']}' in {t['w']:.0f}px  {where(t)}"))

    for a_index, a in enumerate(texts):
        for b in texts[a_index + 1:]:
            hit = intersect(a["vis"], b["vis"])
            if hit:
                findings.append(("text-overlaps-text",
                                 f"'{a['text'][:20]}' x '{b['text'][:20]}' by "
                                 f"{hit[2] - hit[0]:.0f}x{hit[3] - hit[1]:.0f}  "
                                 f"{where(a)} | {where(b)}"))

    for it in items:
        if it["p"] < 0 or it["op"] <= 0.05 or it["w"] <= 0 or it["h"] <= 0:
            continue
        parent = items[it["p"]]
        if parent["w"] <= 0 or parent["h"] <= 0:
            continue
        # Shadows and blurs are meant to bleed past the item they decorate.
        if re.search(r"DropShadow|GaussianBlur|ShaderEffect", it["path"] + it["cls"]):
            continue
        v, p = it["vis"], parent["r"]
        over = max(p[0] - v[0], p[1] - v[1], v[2] - p[2], v[3] - p[3])
        if over > TOLERANCE and v[2] > v[0]:
            findings.append(("child-escapes-parent",
                             f"{short(it['cls'])} '{it['name']}' by {over:.0f}px  {where(it)} "
                             f"parent {short(parent['cls'])} {parent['w']:.0f}x{parent['h']:.0f}"))
    return findings


def main(argv):
    limit = 12
    if "--max" in argv:
        limit = int(argv[argv.index("--max") + 1])
    dumps = sorted(Path(argv[1]).glob("*.json"),
                   key=lambda p: [int(n) for n in p.stem.split("x")])
    total = 0
    for dump in dumps:
        items = load(dump)
        findings = audit(items)
        total += len(findings)
        fonts = Counter((t["family"], t["resolved"]) for t in items if "text" in t)
        print(f"\n=== {dump.stem}: {len(items)} items, {len(findings)} findings")
        print("    fonts requested -> resolved: "
              + ", ".join(f"{a} -> {b} ({n})" for (a, b), n in fonts.most_common()))
        by_kind = Counter(kind for kind, _ in findings)
        for kind, count in by_kind.most_common():
            print(f"  [{kind}] {count}")
            for line in [text for k, text in findings if k == kind][:limit]:
                print(f"      {line}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
