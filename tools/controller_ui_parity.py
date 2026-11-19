#!/usr/bin/env python3
"""Check that bundled mappings only use library/playlist/skin controls the QML
UI implements.

Static approximation: it reads key/group pairs from res/controllers/*.xml and
the ControlProxy bindings from res/qml/**/*.qml. A control a mapping uses must
either have a QML binding or an entry in controller_ui_parity_allowlist.txt
with a reason.

Usage:
    python tools/controller_ui_parity.py
    python tools/controller_ui_parity.py --check-stale   # also fail on unused allowlist entries
    python tools/controller_ui_parity.py --self-check    # prove the parsers work

Exit codes: 0 clean, 1 gaps found, 2 bad allowlist or self-check failure.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAPPINGS = ROOT / "res" / "controllers"
QML = ROOT / "res" / "qml"
ALLOWLIST = Path(__file__).with_name("controller_ui_parity_allowlist.txt")

BLOCK_RE = re.compile(r"<(control|output)>(.*?)</\1>", re.S)
SCRIPT_RE = re.compile(r"<script-binding\s*/>", re.IGNORECASE)
KEY_RE = re.compile(
    r"<group>\[(Library|Playlist|Skin)\]</group>\s*<key>([^<]+)</key>")
QML_RE = re.compile(
    r'group:\s*"\[(Library|Playlist|Skin)\]"\s+key:\s*"([^"]+)"')
ALLOW_RE = re.compile(r"\[(\w+)\]\s+(\S+)\s+--\s+(.+)")


def mapping_controls(text: str) -> set[tuple[str, str]]:
    used = set()
    for block in BLOCK_RE.finditer(text):
        body = block.group(2)
        if SCRIPT_RE.search(body):
            continue
        for group, key in KEY_RE.findall(body):
            used.add((group, key))
    return used


def collect_mapping_controls() -> dict[tuple[str, str], set[str]]:
    used: dict[tuple[str, str], set[str]] = {}
    for path in sorted(MAPPINGS.glob("*.xml")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for key in mapping_controls(text):
            used.setdefault(key, set()).add(path.name)
    return used


def collect_qml_controls() -> set[tuple[str, str]]:
    bound = set()
    for path in QML.rglob("*.qml"):
        text = path.read_text(encoding="utf-8", errors="replace")
        bound.update(QML_RE.findall(text))
    return bound


def load_allowlist() -> dict[tuple[str, str], str]:
    allowed: dict[tuple[str, str], str] = {}
    if not ALLOWLIST.exists():
        return allowed
    for number, line in enumerate(ALLOWLIST.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = ALLOW_RE.fullmatch(line)
        if not match:
            raise SystemExit(
                f"{ALLOWLIST.name}:{number}: expected '[Group] key -- reason', got {line!r}")
        allowed[(match.group(1), match.group(2))] = match.group(3)
    return allowed


def find_gaps(used, bound, allowed):
    return sorted(key for key in used if key not in bound and key not in allowed)


def self_check() -> int:
    sample = (
        "<control><group>[Library]</group><key>MoveVertical</key></control>"
        "<control><group>[Playlist]</group><key>Evil.scriptKey</key>"
        "<options><script-binding/></options></control>")
    found = mapping_controls(sample)
    assert found == {("Library", "MoveVertical")}, found
    assert QML_RE.findall('group: "[Playlist]"\n        key: "SelectNextPlaylist"') == [
        ("Playlist", "SelectNextPlaylist")], "qml regex"
    assert find_gaps({("Library", "MoveFocus")}, set(), {}) == [
        ("Library", "MoveFocus")], "gap detection"
    assert find_gaps({("Library", "MoveFocus")}, {("Library", "MoveFocus")}, {}) == [], \
        "bound key must not be a gap"
    print("self-check ok")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-stale", action="store_true",
                        help="fail when an allowlist entry is no longer needed")
    parser.add_argument("--self-check", action="store_true",
                        help="check the parsers against sample snippets")
    args = parser.parse_args()

    if args.self_check:
        return self_check()

    used = collect_mapping_controls()
    bound = collect_qml_controls()
    allowed = load_allowlist()

    gaps = find_gaps(used, bound, allowed)
    stale = sorted(key for key in allowed if key in bound or key not in used)

    for group, key in gaps:
        files = ", ".join(sorted(used[(group, key)]))
        print(f"MISSING [{group}] {key}: used by {files}")
    if args.check_stale:
        for group, key in stale:
            print(f"STALE   [{group}] {key}: bound or unused, remove from the allowlist")

    print(f"{len(used)} controls used by mappings, {len(bound)} bound in QML, "
          f"{len(allowed)} allowlisted")
    return 1 if gaps or (args.check_stale and stale) else 0


if __name__ == "__main__":
    sys.exit(main())
