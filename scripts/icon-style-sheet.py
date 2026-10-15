#!/usr/bin/env python3
"""docs/icons-style-sheet.html (+ PNGs through rsvg-convert), docs/icon-options/ and icons/vela/scalable/**.

Fluent System Icons (MIT, icons/fluent-source) for the 11 critical icons:
regular goes into the theme, regular and filled into the sheet. The window
buttons are Vela's own: three options (a circle, b chamfered, c pill),
(b) is the theme's default.
"""
import re
import base64, subprocess
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

# ---- sheet pieces. Glyph SVGs carry no size: CSS (--s) or place() sizes them.
LIGHT, DARK = "#1b1b1b", "#f3f3f6"
APPS = [("file manager", "system-file-manager"), ("browser", "firefox"), ("discord", "discord"),
        ("terminal", "utilities-terminal")]
REVERSAL = Path.home() / ".local/share/icons/Reversal-orange/apps/scalable"

def glyph_svg(svg):  # a sizeless svg element
    svg = re.sub(r'\s(width|height)="[^"]*"', "", svg, count=2)
    return svg

def place(svg, x, y, size, color):
    return glyph_svg(svg).replace("<svg", '<svg x="%s" y="%s" width="%s" height="%s" color="%s"' % (x, y, size, size, color), 1)

def app_uri(name):
    f = (REVERSAL / (name + ".svg")).resolve()
    return "data:image/svg+xml;base64," + base64.b64encode(f.read_bytes()).decode() if f.is_file() else ""

def png(svg_text, name, width):
    out = root / "docs/screenshots" / name
    subprocess.run(["rsvg-convert", "-w", str(width), "-b", "white", "-o", str(out)], input=svg_text.encode(), check=True)

def doc(w, h, body):
    return '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="sans-serif" font-size="15">%s</svg>' % (w, h, w, h, body)

def panel(x, y, w, h, mode, rx=8):
    return '<rect x="%s" y="%s" width="%s" height="%s" rx="%s" fill="%s"/>' % (x, y, w, h, rx, "#f3f3f3" if mode == "light" else "#202020")

def fg(mode):
    return LIGHT if mode == "light" else DARK

# --- PNG 1: critical icons, regular/filled x light/dark, 48 + 24 px
body = ['<rect width="1200" height="%d" fill="#fff"/>' % (80 + 76 * len(FLUENT))]
cols = {("regular", "light"): 230, ("regular", "dark"): 430, ("filled", "light"): 680, ("filled", "dark"): 880}
body.append('<text x="24" y="36" font-size="22" font-weight="bold">Vela icons: Fluent regular vs filled</text>')
for (style, mode), x in cols.items():
    body.append('<text x="%d" y="64" fill="#444">%s, %s</text>' % (x, style, mode))
for i, (name, (stem, _, _)) in enumerate(FLUENT.items()):
    y = 80 + 76 * i
    body.append('<text x="24" y="%d">%s</text>' % (y + 42, name))
    for (style, mode), x in cols.items():
        body.append(panel(x - 10, y, 190, 68, mode))
        s = fluent(stem, style, 24)
        body.append(place(s, x + 10, y + 10, 48, fg(mode)) + place(s, x + 90, y + 22, 24, fg(mode)))
H1 = 80 + 76 * len(FLUENT)
png(doc(1200, H1, "".join(body)), "icons-style-sheet@2x.png", 2400)

# --- PNG 2: window buttons, 64 px (+ 24)
rows_y = 70
body = ['<rect width="1200" height="760" fill="#fff"/>',
        '<text x="24" y="36" font-size="22" font-weight="bold">Window buttons: a circle, b chamfered (default), c pill</text>']
for oi, opt in enumerate("abc"):
    for si, state in enumerate(("normal", "hover", "active")):
        y = rows_y + (oi * 3 + si) * 78
        if si == 0:
            body.append('<text x="24" y="%d" font-weight="bold">option %s</text>' % (y + 44, opt))
        body.append('<text x="110" y="%d" fill="#666">%s</text>' % (y + 44, state))
        for mi, mode in enumerate(("light", "dark")):
            x0 = 200 + mi * 500
            body.append(panel(x0, y, 480, 72, mode))
            for ki, kind in enumerate(("close", "minimize", "maximize")):
                b = button(kind, opt, 24, state)
                body.append(place(b, x0 + 20 + ki * 100, y + 4, 64, fg(mode)) + place(b, x0 + 340 + ki * 40, y + 24, 24, fg(mode)))
png(doc(1200, 760, "".join(body)), "window-buttons.png", 2400)

# --- PNG 3: taskbar + title bars
TRAY = ["wifi", "volume", "battery", "bell"]
def taskbar(mode, x, y, w):
    c = fg(mode)
    out = ['<rect x="%s" y="%s" width="%s" height="56" rx="10" fill="%s" opacity=".95"/>' % (x, y, w, "#f3f3f3" if mode == "light" else "#202020"),
           '<rect x="%s" y="%s" width="%s" height="56" rx="10" fill="none" stroke="%s" stroke-opacity=".12"/>' % (x, y, w, c)]
    cx = x + w / 2 - 150
    out.append('<rect x="%s" y="%s" width="32" height="32" rx="8" fill="#0067c0"/><text x="%s" y="%s" fill="#fff" font-weight="bold" font-size="18" text-anchor="middle">V</text>' % (cx, y + 12, cx + 16, y + 35))
    for i, (_, name) in enumerate(APPS):
        out.append('<image href="%s" x="%s" y="%s" width="32" height="32"/>' % (app_uri(name), cx + 48 + i * 48, y + 12))
    tx = x + w - 250
    for i, t in enumerate(TRAY):
        stem = FLUENT[t][0]
        out.append(place(fluent(stem, "regular", 24), tx + i * 34, y + 16, 24, c))
    out.append('<text x="%s" y="%s" fill="%s" font-size="13" text-anchor="end">14:32</text><text x="%s" y="%s" fill="%s" font-size="13" text-anchor="end">15.10.2026</text>' % (x + w - 16, y + 24, c, x + w - 16, y + 42, c))
    return "".join(out)

def titlebar(mode, opt, state, x, y, w):
    c = fg(mode)
    out = ['<rect x="%s" y="%s" width="%s" height="40" rx="8" fill="%s"/>' % (x, y, w, "#fafafa" if mode == "light" else "#2b2b2b"),
           '<text x="%s" y="%s" fill="%s" font-size="14">Explorer</text>' % (x + 14, y + 25, c)]
    for ki, kind in enumerate(("minimize", "maximize", "close")):
        out.append(place(button(kind, opt, 24, state), x + w - 124 + ki * 40, y + 4, 32, c))
    return "".join(out)

body = ['<rect width="1200" height="700" fill="#fff"/>',
        '<text x="24" y="36" font-size="22" font-weight="bold">Taskbar preview</text>']
body.append(taskbar("light", 24, 56, 1152) + taskbar("dark", 24, 130, 1152))
body.append('<text x="24" y="238" font-size="18" font-weight="bold">Title bars: window button options, normal and hover</text>')
# simple grid: row per option, four bars (light normal/hover, dark normal/hover)
yy = 256
for opt in "abc":
    xx = 24
    for mode in ("light", "dark"):
        for state in ("normal", "hover"):
            body.append(titlebar(mode, opt, state, xx, yy, 276))
            xx += 288
    body.append('<text x="24" y="%d" fill="#666" font-size="13">option %s</text>' % (yy + 58, opt))
    yy += 96
png(doc(1200, 560, "".join(body)), "taskbar-preview.png", 2400)

# ---- single-file responsive HTML (offline, no external resources)
def h_icon(stem, style):
    return fluent(stem, style, 24).replace("<svg", '<svg class="i"', 1)

def h_btn(kind, opt, state):
    return button(kind, opt, 24, state).replace("<svg", '<svg class="i"', 1)

def cell(content, mode):
    return '<div class="cell %s">%s</div>' % (mode, content)

rows = []
for name, (stem, _, _) in FLUENT.items():
    c = ['<div class="name">%s</div>' % name]
    for style in ("regular", "filled"):
        for mode in ("light", "dark"):
            c.append(cell(h_icon(stem, style), mode))
    rows.append('<div class="row">%s</div>' % "".join(c))
brows = []
for opt in "abc":
    for state in ("normal", "hover", "active"):
        label = {"a": "a: circle", "b": "b: chamfered (default)", "c": "c: pill"}[opt] if state == "normal" else ""
        c = ['<div class="name">%s <small>%s</small></div>' % (label, state)]
        for mode in ("light", "dark"):
            c.append(cell("".join(h_btn(k, opt, state) for k in ("close", "minimize", "maximize")), mode))
        brows.append('<div class="row b">%s</div>' % "".join(c))

def h_taskbar(mode):
    apps = "".join('<img class="app" src="%s" alt="%s">' % (app_uri(n), t) for t, n in APPS)
    tray = "".join(h_icon(FLUENT[t][0], "regular") for t in TRAY)
    return ('<div class="bar %s"><span class="start">V</span>%s<span class="grow"></span>%s'
            '<span class="clock">14:32<br>15.10.2026</span></div>') % (mode, apps, tray)

def h_title(mode, opt, state):
    return '<div class="title %s"><span>Explorer</span><span class="grow"></span>%s</div>' % (
        mode, "".join(h_btn(k, opt, state) for k in ("minimize", "maximize", "close")))

titles = "".join('<div class="trow"><b>%s</b>%s</div>' % (opt, "".join(
    h_title(m, opt, s) for m in ("light", "dark") for s in ("normal", "hover"))) for opt in "abc")

html = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Vela icon styles</title>
<style>
:root{--s:24px}
body{font:16px system-ui,sans-serif;margin:0;padding:1rem;color:#222;background:#fff}
h1{font-size:1.4rem}h2{font-size:1.15rem;margin-top:2rem}p{color:#444;max-width:60em}
.ctl{position:sticky;top:0;background:#fff;padding:.5rem 0;display:flex;gap:.5rem;align-items:center;flex-wrap:wrap;z-index:2;border-bottom:1px solid #ddd}
.ctl button{font:inherit;padding:.3rem .8rem;cursor:pointer}.ctl input{width:12em}
.i{width:var(--s);height:var(--s);flex:none}
.row{display:flex;gap:.5rem;align-items:center;margin:.3rem 0;flex-wrap:wrap}
.name{width:9em;font-weight:600}.name small{font-weight:400;color:#666}
.cell{display:flex;gap:.6rem;align-items:center;padding:.6rem 1rem;border-radius:.5rem;min-width:calc(var(--s) * 1.5)}
.row.b .cell{gap:calc(var(--s) * .5)}
.light{background:#f3f3f3;color:#1b1b1b}.dark{background:#202020;color:#f3f3f6}
.bar{display:flex;align-items:center;gap:.6rem;padding:.5rem 1rem;border-radius:.7rem;margin:.5rem 0;border:1px solid #8884}
.bar .app{width:calc(var(--s) * 1.33);height:calc(var(--s) * 1.33)}
.start{width:calc(var(--s) * 1.33);height:calc(var(--s) * 1.33);border-radius:.5rem;background:#0067c0;color:#fff;font-weight:700;display:grid;place-items:center}
.clock{font-size:.8rem;text-align:right;line-height:1.2}.grow{flex:1}
.trow{display:flex;gap:.6rem;flex-wrap:wrap;align-items:center;margin:.5rem 0}.trow b{width:2em}
.title{display:flex;gap:.4rem;align-items:center;padding:.4rem .7rem;border-radius:.5rem;min-width:16em;flex:1}
.title.light{background:#fafafa;color:#1b1b1b}.title.dark{background:#2b2b2b;color:#f3f3f6}
</style></head><body>
<h1>Vela icons</h1>
<div class="ctl"><b>Icon size</b>
<button data-s="24">24</button><button data-s="32">32</button><button data-s="48">48</button><button data-s="64">64</button>
<input id="r" type="range" min="16" max="96" value="24"><output id="o">24 px</output></div>
<p>Fluent System Icons (Microsoft, MIT), regular and filled; the WiFi icon has one drawing. The
window buttons are Vela's own, three options. App icons are from Reversal-orange (GPL-3.0).
Generated by scripts/icon-style-sheet.py; works offline.</p>
<h2>Critical icons</h2>
<div class="row"><div class="name"></div><b>Regular</b><b style="margin-left:calc(var(--s) * 3.4 + 4rem)">Filled</b></div>
%s
<h2>Window buttons (close, minimize, maximize): not Fluent</h2>
<p>Option b is installed in the theme; the others are in docs/icon-options/.</p>
%s
<h2>Taskbar preview</h2>
%s%s
<h2>Title bars (minimize, maximize, close; light/dark, normal/hover)</h2>
%s
<script>
const r=document.getElementById('r'),o=document.getElementById('o');
function set(v){document.documentElement.style.setProperty('--s',v+'px');r.value=v;o.textContent=v+' px'}
r.oninput=()=>set(r.value);
document.querySelectorAll('[data-s]').forEach(b=>b.onclick=()=>set(b.dataset.s));
</script></body></html>
""" % ("\n".join(rows), "\n".join(brows), h_taskbar("light"), h_taskbar("dark"), titles)
(root / "docs/icons-style-sheet.html").write_text(html)
