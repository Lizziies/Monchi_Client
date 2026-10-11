#!/usr/bin/env python3
"""Lists English UI strings in dll/src that have no German entry."""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent / "dll" / "src"
lit = r'"((?:[^"\\]|\\.)*)"'

def unescape(s):
    return bytes(s, "utf-8").decode("unicode_escape").encode("latin-1", "ignore").decode("utf-8", "ignore") if "\\" in s else s

known = set()
for f in list(root.rglob("Lang*.cpp")) + [root.parent.parent / "common" / "I18n.cpp"]:
    if not f.exists():
        continue
    text = f.read_text(encoding="utf-8", errors="ignore")
    for m in re.finditer(r"\{\s*" + lit + r"\s*,\s*" + lit + r"\s*\}", text):
        known.add(m.group(1))

patterns = [
    r'i18n::(?:tr|fmt)\(\s*' + lit,
    r'\bModule\(\s*' + lit + r'\s*,\s*' + lit,
    r'\b(?:toggleSetting|keySetting|textSetting|colorSetting)\(\s*"[^"]*"\s*,\s*' + lit,
    r'\b(?:slider|intSlider|choice)\(\s*"[^"]*"\s*,\s*' + lit,
    r'\bsub\(\s*' + lit,
    r'widgets::(?:button|row|sectionTitle|hint)\(\s*' + lit,
]

missing = {}
for f in root.rglob("*"):
    if f.suffix not in (".cpp", ".hpp") or f.name.startswith("Lang"):
        continue
    text = f.read_text(encoding="utf-8", errors="ignore")
    for pat in patterns:
        for m in re.finditer(pat, text):
            for g in m.groups():
                s = g
                if not s or s in known or len(s) < 3 or not re.search(r"[A-Za-z]{3}", s):
                    continue
                if re.fullmatch(r"[\W\d_]*(%[sdf.\d]+|\{\}|[\W\d_])*", s):
                    continue
                missing.setdefault(s, set()).add(f.name)

for s, files in sorted(missing.items()):
    print(f"{s!r}  ({', '.join(sorted(files))})")
print(f"\n{len(missing)} strings without a German entry, {len(known)} known", file=sys.stderr)
