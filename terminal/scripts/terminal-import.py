#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Turns a macOS Terminal profile (.terminal) into a Vela color scheme: JSON in
# ~/.config/vela-terminal/schemes, where the Terminal's Settings list it by name.
#
#   python3 terminal-import.py ~/Downloads/jellybeans-default.terminal

import json
import os
import plistlib
import re
import sys

ANSI = ["Black", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White"]
NUMBER = rb"([01](?:\.[0-9]+)?) ([01](?:\.[0-9]+)?) ([01](?:\.[0-9]+)?)"


def hex_of(raw):
    # Colors are archived as "r g b" in 0..1 inside a keyed archive; the first triplet is the color.
    match = re.search(NUMBER, raw)
    if not match:
        return None
    return "#" + "".join("%02x" % round(float(part) * 255) for part in match.groups())


def main(path):
    with open(path, "rb") as file:
        profile = plistlib.load(file)
    colors = [hex_of(profile["ANSI%sColor" % name]) for name in ANSI]
    bright = [hex_of(profile["ANSIBright%sColor" % name]) for name in ANSI]
    scheme = {
        "foreground": hex_of(profile["TextColor"]),
        "background": hex_of(profile["BackgroundColor"]),
        "colors": colors + bright,
    }
    if None in scheme["colors"] or not scheme["foreground"] or not scheme["background"]:
        sys.exit("%s: not a color profile this script understands" % path)
    folder = os.path.expanduser("~/.config/vela-terminal/schemes")
    os.makedirs(folder, exist_ok=True)
    name = profile.get("name", os.path.splitext(os.path.basename(path))[0])
    target = os.path.join(folder, name + ".json")
    with open(target, "w", encoding="utf-8") as file:
        json.dump(scheme, file, indent=2)
    print(target)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
