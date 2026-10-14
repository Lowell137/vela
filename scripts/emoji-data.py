#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes shell/data/emoji.tsv, the emoji of the Win+. panel, from Unicode's files.

    scripts/emoji-data.py DIR

DIR holds emoji-test.txt (https://unicode.org/Public/emoji/latest/) and CLDR's
common/annotations/{en,tr,it}.xml (https://github.com/unicode-org/cldr). One
line per emoji: the emoji, its group's number, its English, Turkish and
Italian names, then every word it's found by. Skin tones are left out: the
panel shows each emoji once.
"""
import os
import re
import sys
import xml.etree.ElementTree as ET

source = sys.argv[1]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def annotations(language):
    names, words = {}, {}
    for node in ET.parse(os.path.join(source, language + ".xml")).getroot().iter("annotation"):
        key = node.get("cp").replace("️", "")
        if node.get("type") == "tts":
            names[key] = node.text.strip()
        else:
            words[key] = node.text.replace("|", " ")
    return names, words


languages = [annotations(language) for language in ("en", "tr", "it")]
groups = []
lines = []
for line in open(os.path.join(source, "emoji-test.txt"), encoding="utf-8"):
    if line.startswith("# group:"):
        groups.append(line.split(":", 1)[1].strip())
        continue
    match = re.match(r"^[0-9A-F ]+; fully-qualified\s+# (\S+) E[\d.]+ (.+)$", line)
    if not match or groups[-1] == "Component" or "skin tone" in match.group(2):
        continue
    emoji, english = match.groups()
    key = emoji.replace("️", "")
    names = [english] + [language[0].get(key, "") for language in languages[1:]]
    found_by = " ".join([english] + [language[0].get(key, "") + " " + language[1].get(key, "") for language in languages])
    found_by = " ".join(dict.fromkeys(found_by.lower().split()))
    group = [g for g in groups if g != "Component"].index(groups[-1])
    lines.append("\t".join([emoji, str(group)] + names + [found_by]))

with open(os.path.join(root, "shell", "data", "emoji.tsv"), "w", encoding="utf-8") as out:
    out.write("\n".join(lines) + "\n")
print(len(lines), "emoji")
