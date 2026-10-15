#!/usr/bin/env python3
"""docs/icons-style-sheet.html, docs/icon-options/ and icons/vela/scalable/**.

Fluent System Icons (MIT, icons/fluent-source) for the 11 critical icons:
regular goes into the theme, regular and filled into the sheet. The window
buttons are Vela's own: three options (a circle, b chamfered, c pill),
(b) is the theme's default.
"""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / "icons/fluent-source"
# sheet name: (source file stem, context, theme names)
FLUENT = {
    "wifi": ("wifi_1", "status", ["network-wireless", "network-wireless-signal-excellent"]),
    "bluetooth": ("bluetooth", "status", ["bluetooth-active", "network-bluetooth"]),
    "volume": ("speaker_2", "status", ["audio-volume-high"]),
    "brightness": ("brightness_high", "status", ["display-brightness", "brightness-high"]),
    "settings": ("settings", "categories", ["preferences-system", "configure"]),
    "search": ("search", "actions", ["edit-find"]),
    "folder": ("folder", "places", ["folder"]),
    "file": ("document", "mimetypes", ["text-x-generic", "application-x-zerosize"]),
    "power": ("power", "actions", ["system-shutdown"]),
    "bell": ("alert", "status", ["notification-active", "preferences-desktop-notification"]),
    "battery": ("battery_10", "status", ["battery-good", "battery-full"]),
}
WEAKER = {"network-wireless-signal-good": "wifi_2", "network-wireless-signal-ok": "wifi_3",
          "network-wireless-signal-weak": "wifi_4"}

def fluent(stem, style, size):
    t = (src / ("%s_%s.svg" % (stem, style))).read_text().replace("#212121", "currentColor")
    return re.sub(r'width="24" height="24"', 'width="%d" height="%d"' % (size, size), t, count=1)

# Window buttons: glyph(kind, option) and background(option); 24 viewBox.
def glyph(kind, opt):
    if opt == "a":  # ring + small glyph
        ring = '<circle cx="12" cy="12" r="9.25" fill="none" stroke="currentColor" stroke-width="1.25"/>'
        g = {"close": "M9 9l6 6M15 9l-6 6", "minimize": "M8.8 12h6.4"}.get(kind)
        inner = '<path d="%s" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/>' % g if g else \
            '<rect x="8.75" y="8.75" width="6.5" height="6.5" rx="1" fill="none" stroke="currentColor" stroke-width="1.5"/>'
        return ring + inner
    if opt == "b":  # sharp, 2 px, mitered
        st = 'fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="miter"'
        return {"close": '<path %s d="M6 6l12 12M18 6 6 18"/>' % st,
                "minimize": '<path %s d="M5 12h14"/>' % st,
                "maximize": '<path %s d="M8 5h8l3 3v8l-3 3H8l-3-3V8z"/>' % st}[kind]
    st = 'fill="none" stroke="currentColor" stroke-width="1.25" stroke-linecap="round"'  # c, pill
    return {"close": '<path %s d="M9.5 9.5l5 5M14.5 9.5l-5 5"/>' % st,
            "minimize": '<path %s d="M8.5 12h7"/>' % st,
            "maximize": '<rect %s x="8.75" y="9" width="6.5" height="6" rx="1"/>' % st}[kind]

def background(opt, opacity):
    if opacity == 0:
        return ""
    if opt == "a":
        return '<circle cx="12" cy="12" r="10.5" fill="currentColor" opacity="%s"/>' % opacity
    if opt == "b":
        return '<path d="M5 1.5h14L22.5 5v14L19 22.5H5L1.5 19V5z" fill="currentColor" opacity="%s"/>' % opacity
    return '<rect x="0.5" y="5" width="23" height="14" rx="7" fill="currentColor" opacity="%s"/>' % opacity

def button(kind, opt, size=24, state="normal"):
    op = {"normal": 0, "hover": 0.12, "active": 0.24}[state]
    return '<svg width="%d" height="%d" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">%s%s</svg>' % (
        size, size, background(opt, op), glyph(kind, opt))

# theme: Fluent regular + option (b) buttons
sc = root / "icons/vela/scalable"
def put(ctx, name, text):
    d = sc / ctx
    d.mkdir(parents=True, exist_ok=True)
    for suffix in ("", "-symbolic"):
        (d / (name + suffix + ".svg")).write_text(text + "\n")
for stem, ctx, names in FLUENT.values():
    for n in names:
        put(ctx, n, fluent(stem, "regular", 24))
for n, stem in WEAKER.items():
    put("status", n, fluent(stem, "regular", 24))
for kind in ("close", "minimize", "maximize"):
    put("actions", "window-" + kind, button(kind, "b"))

# options
od = root / "docs/icon-options"
od.mkdir(exist_ok=True)
for opt in "abc":
    for kind in ("close", "minimize", "maximize"):
        (od / ("window-%s-%s.svg" % (kind, opt))).write_text(button(kind, opt) + "\n")

rows = []
for name, (stem, _, _) in FLUENT.items():
    c = ["<th>%s</th>" % name]
    for style in ("regular", "filled"):
        for mode in ("light", "dark"):
            c.append('<td class="%s">%s%s</td>' % (mode, fluent(stem, style, 24), fluent(stem, style, 32)))
    c.append("<td class='src'>Fluent (MIT)</td>")
    rows.append("<tr>%s</tr>" % "".join(c))

brows = []
labels = {"a": "a: circle", "b": "b: chamfered, 2 px (default)", "c": "c: pill"}
for opt in "abc":
    for state in ("normal", "hover", "active"):
        c = ["<th>%s</th><td class='src'>%s</td>" % (labels[opt] if state == "normal" else "", state)]
        for mode in ("light", "dark"):
            cell = "".join(button(k, opt, s, state) for k in ("close", "minimize", "maximize") for s in (24, 16))
            c.append('<td class="%s">%s</td>' % (mode, cell))
        brows.append("<tr>%s</tr>" % "".join(c))

html = """<!doctype html>
<meta charset="utf-8">
<title>Vela icon styles</title>
<style>
body{font:14px system-ui,sans-serif;margin:24px;color:#222;background:#fff}
table{border-collapse:collapse;margin-bottom:28px}th,td{padding:8px 14px;text-align:center}
th{text-align:left;font-weight:600}
td.light{background:#f3f3f3;color:#1b1b1b}td.dark{background:#202020;color:#f3f3f6}
td svg{margin:0 5px;vertical-align:middle}.src{font-size:12px;color:#666}
h1{font-size:18px}h2{font-size:15px}p{max-width:72ch;color:#444}
</style>
<h1>Vela icons: Fluent regular vs filled</h1>
<p>Fluent System Icons (Microsoft, MIT), 24 and 32 px, light and dark. The WiFi icon has
one drawing for both styles. Generated by scripts/icon-style-sheet.py.</p>
<table>
<tr><th></th><th colspan=2>Regular</th><th colspan=2>Filled</th><th>Source</th></tr>
%s
</table>
<h2>Window buttons (close, minimize, maximize): three options, not Fluent</h2>
<p>Each at 24 and 16 px; normal, hover and active. Option b is installed in the theme; the
others are in docs/icon-options/.</p>
<table>
<tr><th>Option</th><th>State</th><th>Light</th><th>Dark</th></tr>
%s
</table>
""" % ("\n".join(rows), "\n".join(brows))
(root / "docs/icons-style-sheet.html").write_text(html)
