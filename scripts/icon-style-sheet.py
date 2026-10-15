#!/usr/bin/env python3
"""Draws the 12 critical icons in two Fluent-like styles (regular, filled).

Writes docs/icons-style-sheet.html and icons/vela/scalable/** (the regular
set, currentColor; the build derives vela-dark). Where common/fluent has the
Fluent System Icons drawing (MIT) the sheet and the theme use it as is.
Everything else is drawn here, with the same idea (rounded, 1.5 px at 24).
"""
import math, re
from pathlib import Path

root = Path(__file__).resolve().parents[1]

def gear(filled):
    pts = []
    n = 8
    for i in range(n):
        a = 2 * math.pi * i / n
        for da, r in [(-0.30, 7.4), (-0.17, 9.6), (0.17, 9.6), (0.30, 7.4)]:
            pts.append((12 + r * math.cos(a + da), 12 + r * math.sin(a + da)))
    d = "M" + " L".join("%.2f %.2f" % p for p in pts) + "Z"
    hole = "M15 12a3 3 0 1 0-6 0 3 3 0 0 0 6 0Z"
    if filled:
        return '<path fill-rule="evenodd" d="%s%s"/>' % (d, hole)
    return '<path d="%s" stroke-linejoin="round"/><circle cx="12" cy="12" r="3"/>' % d

S = 'fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"'
# name: (regular body, filled body, context, [theme names])
ICONS = {
 "wifi": ('<path d="M2.5 9.2a14 14 0 0 1 19 0M5.6 12.6a9.6 9.6 0 0 1 12.8 0M8.8 16a5 5 0 0 1 6.4 0"/><circle cx="12" cy="19.5" r=".6"/>',
          '<path d="M12 21.3 1.9 10.2a14.7 14.7 0 0 1 20.2 0Z"/>',
          "status", ["network-wireless", "network-wireless-signal-good"]),
 "bluetooth": ('<path d="M6.5 7.5 17 16.5 12 21V3l5 4.5L6.5 16.5"/>',
          '<path fill-rule="evenodd" d="M11 2.5v7.4L7.4 6.7 6.3 8l4.1 4-4.1 4 1.1 1.3 3.6-3.2v7.4l7.3-6.2L14 12l4.3-3.3zM13 6.7v3.2l1.9-1.6zm0 7.4v3.2l1.9-1.6z"/>',
          "status", ["bluetooth-active", "network-bluetooth"]),
 "volume": ('<path d="M4 9.5h3.2L12 5.5v13l-4.8-4H4z"/><path d="M15.5 9a4.2 4.2 0 0 1 0 6M18.2 6.5a8 8 0 0 1 0 11"/>',
          '<path d="M4 9.5h3.2L12 5.5v13l-4.8-4H4z"/><path %s d="M15.5 9a4.2 4.2 0 0 1 0 6M18.2 6.5a8 8 0 0 1 0 11"/>' % S,
          "status", ["audio-volume-high"]),
 "brightness": ('<circle cx="12" cy="12" r="4"/><path d="M12 2.8v2.4M12 18.8v2.4M2.8 12h2.4M18.8 12h2.4M5.5 5.5l1.7 1.7M16.8 16.8l1.7 1.7M5.5 18.5l1.7-1.7M16.8 7.2l1.7-1.7"/>',
          '<circle cx="12" cy="12" r="4.6"/><path %s d="M12 2.8v2.4M12 18.8v2.4M2.8 12h2.4M18.8 12h2.4M5.5 5.5l1.7 1.7M16.8 16.8l1.7 1.7M5.5 18.5l1.7-1.7M16.8 7.2l1.7-1.7"/>' % S,
          "status", ["display-brightness", "brightness-high"]),
 "settings": (gear(False), gear(True), "categories", ["preferences-system", "configure"]),
 "search": ('<circle cx="10.5" cy="10.5" r="6.5"/><path d="m15.4 15.4 5.8 5.8"/>',
          '<circle %s cx="10.5" cy="10.5" r="6.2" stroke-width="2.8"/><path %s stroke-width="2.8" d="m15.4 15.4 5.6 5.6"/>' % (S, S),
          "actions", ["edit-find"]),
 "folder": ('<path d="M3 6.5A1.5 1.5 0 0 1 4.5 5h4.2l2 2.5h8.8A1.5 1.5 0 0 1 21 9v9a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18z"/>',
          '<path opacity=".5" d="M3 6.5A1.5 1.5 0 0 1 4.5 5h4.2l2 2.5H19.5A1.5 1.5 0 0 1 21 9v1H3z"/><path d="M3 10h18v8a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18z"/>',
          "places", ["folder"]),
 "file": ('<path d="M6.5 3H14l5 5v11.5a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 5 19.5v-15A1.5 1.5 0 0 1 6.5 3Z"/><path d="M14 3v4a1 1 0 0 0 1 1h4"/>',
          '<path d="M6.5 3H13v5.5a1 1 0 0 0 1 1h5v10a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 5 19.5v-15A1.5 1.5 0 0 1 6.5 3Z"/><path opacity=".5" d="M14.5 3.3 18.7 7.5h-3.2a1 1 0 0 1-1-1z"/>',
          "mimetypes", ["text-x-generic", "application-x-zerosize"]),
 "close": ('<path d="M6 6l12 12M18 6 6 18"/>', '<path %s stroke-width="2.4" d="M6 6l12 12M18 6 6 18"/>' % S,
          "actions", ["window-close"]),
 "minimize": ('<path d="M5 12h14"/>', '<rect x="5" y="10.5" width="14" height="3" rx="1.5"/>',
          "actions", ["window-minimize"]),
 "maximize": ('<rect x="5.5" y="5.5" width="13" height="13" rx="1.5"/>', '<rect x="5" y="5" width="14" height="14" rx="2"/>',
          "actions", ["window-maximize"]),
 "power": ('<path d="M12 3v8.5M7.2 6.5a8 8 0 1 0 9.6 0"/>', '<path %s stroke-width="2.4" d="M12 3v8.5M7.2 6.5a8 8 0 1 0 9.6 0"/>' % S,
          "actions", ["system-shutdown"]),
 "bell": ('<path d="M6 10a6 6 0 0 1 12 0c0 4.5 1.5 6 2 6.5H4c.5-.5 2-2 2-6.5Z"/><path d="M10 19.5a2.2 2.2 0 0 0 4 0"/>',
          '<path d="M6 10a6 6 0 0 1 12 0c0 4.5 1.5 6 2 6.5H4c.5-.5 2-2 2-6.5Z"/><path d="M9.6 18.5h4.8a2.4 2.4 0 0 1-4.8 0Z"/>',
          "status", ["notification-active", "preferences-desktop-notification"]),
 "battery": ('<rect x="2.5" y="7.5" width="16" height="9" rx="2"/><path d="M21.5 10.5v3"/><rect x="5" y="10" width="8" height="4" rx=".8" fill="currentColor" stroke="none"/>',
          '<rect opacity=".4" x="2" y="7" width="17" height="10" rx="2.4"/><rect x="4.5" y="9.5" width="9" height="5" rx="1"/><rect x="20.5" y="10" width="2" height="4" rx="1"/>',
          "status", ["battery-good", "battery-full"]),
}
# Fluent System Icons (MIT) already vendored in common/fluent, regular only.
VENDORED = {"volume": "audio-volume-high", "search": "edit-find", "settings": "configure", "close": "window-close"}

def svg(body, filled, size=24):
    attrs = 'fill="currentColor"' if filled else \
        'fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"'
    return '<svg width="%d" height="%d" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg" %s>%s</svg>' % (size, size, attrs, body)

def vendored(name, size):
    t = (root / "common/fluent" / (VENDORED[name] + ".svg")).read_text()
    t = t.replace("#212121", "currentColor")
    return re.sub(r'width="20" height="20"', 'width="%d" height="%d"' % (size, size), t, count=1)

def regular(name, size=24):
    return vendored(name, size) if name in VENDORED else svg(ICONS[name][0], False, size)

# theme
theme = root / "icons/vela"
for name, (reg, _, ctx, names) in ICONS.items():
    d = theme / "scalable" / ctx
    d.mkdir(parents=True, exist_ok=True)
    body = regular(name)
    for n in names:
        for suffix in ("", "-symbolic"):
            (d / (n + suffix + ".svg")).write_text(body + "\n")

# sheet
cells = []
for name in ICONS:
    row = ['<th>%s</th>' % name]
    for variant in ("regular", "filled"):
        for mode in ("light", "dark"):
            if variant == "regular":
                icons = regular(name, 24) + regular(name, 32)
            else:
                icons = svg(ICONS[name][1], True, 24) + svg(ICONS[name][1], True, 32)
            row.append('<td class="%s">%s</td>' % (mode, icons))
    src = "Fluent (MIT)" if name in VENDORED else "own"
    row.append("<td class='src'>%s</td>" % src)
    cells.append("<tr>%s</tr>" % "".join(row))
html = """<!doctype html>
<meta charset="utf-8">
<title>Vela icon styles</title>
<style>
body{font:14px system-ui,sans-serif;margin:24px;color:#222}
table{border-collapse:collapse}th,td{padding:8px 14px;text-align:center}
th{text-align:left;font-weight:600}
td.light{background:#f3f3f3;color:#1b1b1b}td.dark{background:#202020;color:#f3f3f6}
td svg{margin:0 6px;vertical-align:middle}.src{font-size:12px;color:#666}
h1{font-size:18px}p{max-width:70ch;color:#444}
</style>
<h1>Vela icon styles: Fluent regular vs filled</h1>
<p>24 and 32 px, light and dark. Regular: outline, 1.5 px at 24, round caps (Fluent System
Icons' own drawings where common/fluent has them, "Fluent (MIT)", otherwise drawn here in the
same idea). Filled: solid or half-solid. Generated by scripts/icon-style-sheet.py.</p>
<table>
<tr><th></th><th colspan=2>Regular</th><th colspan=2>Filled</th><th></th></tr>
%s
</table>
""" % "\n".join(cells)
(root / "docs/icons-style-sheet.html").write_text(html)
