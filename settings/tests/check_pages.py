#!/usr/bin/env python3
"""Page names, parents, files, aliases and search keys must agree.

Reads settings/qml/Main.qml and shell/qml/StartMenu.qml; no Qt needed.
"""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[2]
main = (root / "settings/qml/Main.qml").read_text()
start = (root / "shell/qml/StartMenu.qml").read_text()
errors = []

pages = {}
for m in re.finditer(r'^\s*"([a-z-]+)": \{ title: qsTr\("[^"]*"\), parent: "([a-z-]*)", file: "(\w+\.qml)"', main, re.M):
    pages[m.group(1)] = (m.group(2), m.group(3))
if len(pages) < 20:
    errors.append("couldn't read the pages table (%d)" % len(pages))

for name, (parent, file) in pages.items():
    if parent and parent not in pages:
        errors.append("page %s: unknown parent %s" % (name, parent))
    if not (root / "settings/qml/pages" / file).exists():
        errors.append("page %s: missing file %s" % (name, file))
    # No parent loops.
    seen, n = set(), name
    while n:
        if n in seen:
            errors.append("page %s: parent loop" % name)
            break
        seen.add(n)
        n = pages[n][0] if n in pages else ""

m = re.search(r'readonly property var sections: \[([^\]]*)\]', main)
sections = re.findall(r'"([a-z-]+)"', m.group(1)) if m else []
for s in sections:
    if s not in pages or pages[s][0]:
        errors.append("section %s is not a top-level page" % s)
for name, (parent, _) in pages.items():
    top = name
    while pages[top][0]:
        top = pages[top][0]
    if top not in sections:
        errors.append("page %s is under %s, which is not a sidebar section" % (name, top))

m = re.search(r'readonly property var aliases: \(\{(.*?)\}\)', main, re.S)
for old, new in re.findall(r'"([a-z-]+)": "([a-z-]+)"', m.group(1)):
    if new not in pages:
        errors.append("alias %s -> unknown page %s" % (old, new))

for label, text in (("Main.qml", main), ("StartMenu.qml", start)):
    for page in re.findall(r'\{ page: "([a-z-]+)", text: qsTr', text):
        if page not in pages:
            errors.append("%s search index: unknown page %s" % (label, page))

# The names the shell opens (systemactions.cpp, menus) must resolve.
known = set(pages) | set(re.findall(r'"([a-z-]+)": "[a-z-]+"', m.group(1)))
for f in (root / "shell").rglob("*"):
    if f.suffix in (".qml", ".cpp"):
        for page in re.findall(r'openSettingsPage\("([a-z-]+)"\)', f.read_text(errors="ignore")):
            if page not in known:
                errors.append("%s opens unknown settings page %s" % (f.name, page))

for e in errors:
    print("FAIL:", e)
sys.exit(1 if errors else 0)
