# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Shell pieces (wlr-layer-shell) with vela-panel in place of the Qt shell:
the wallpaper, the taskbar reserving its space and blurring what's behind it
(ext-background-effect, docs/renderer.md §8.3); and damage: a frame drawn
only where something changed must be identical to one redrawn in full
(VELA_DEBUG_DAMAGE=1)."""

import os
import subprocess
import time
import unittest

from harness import Session, tool

BAR = 48


def shell(vela, blur=True, autohide=False):
    """Wallpaper and taskbar, and the time to appear."""
    taskbar = ["taskbar"] + (["--blur"] if blur else []) + (["--autohide"] if autohide else [])
    for args in (["wallpaper"], taskbar):
        vela.clients.append(subprocess.Popen([tool("vela-panel"), *args], env=vela.client_env))
    vela.wait_for(lambda s: s.output["frames"] > 0)
    time.sleep(0.6)


def drag_over_the_taskbar(vela):
    """A decorated window dragged halfway over the taskbar; the screen at the end."""
    vela.open_window(401, 301)
    window = vela.open_window(300, 200, decorated=True)
    w = vela.state().window(window)
    out = vela.state().output
    x, y = w["x"] + 60, w["y"] + 10
    steps = ["move", x, y, "keydown", "super", "down"]
    for i in range(1, 21):
        steps += ["move", x + i * 7, y + i * (out["h"] - 120 - y) // 20, "sleep", "10"]
    vela.input(*steps, "up", "keyup", "super", "move", 5, 5)
    time.sleep(0.5)
    return vela.pixels()


class Layers(unittest.TestCase):
    def test_taskbar_reserves_its_space(self):
        with Session() as vela:
            shell(vela)
            window = vela.open_window()
            vela.keys("super+Up")
            state = vela.wait_for(lambda s: s.window(window)["maximized"], what="maximized")
            w, out = state.window(window), state.output
            self.assertEqual((w["y"], w["h"]), (out["y"], out["h"] - BAR))

    def test_auto_hidden_taskbar_gives_windows_the_whole_output(self):
        """taskbar-autohide: the taskbar slid down still reserves its space
        for panels, but maximized windows use the whole output; switched off
        (or never told) they stop above it again."""
        with Session() as vela:
            shell(vela, autohide=True)
            window = vela.open_window()
            vela.command("taskbar-autohide yes")
            vela.keys("super+Up")
            state = vela.wait_for(lambda s: s.window(window)["maximized"], what="maximized")
            w, out = state.window(window), state.output
            self.assertEqual((w["y"], w["h"]), (out["y"], out["h"]))
            vela.command("taskbar-autohide no")
            state = vela.wait_for(lambda s: s.window(window)["h"] == s.output["h"] - BAR, what="back above the taskbar")

    def test_taskbar_blurs_what_is_behind(self):
        with Session() as vela:
            shell(vela, blur=False)
            plain = vela.pixels()
        with Session() as vela:
            shell(vela, blur=True)
            blurred = vela.pixels()
        # The wallpaper has sharp bands every 24 rows: behind the bar, only
        # when blurred does it become a gradient, with many colors.
        column = lambda image: {image.at(900, y) for y in range(image.height - BAR, image.height)}
        self.assertLessEqual(len(column(plain)), 4)
        self.assertGreaterEqual(len(column(blurred)), 12)
        # Outside the bar nothing changes.
        self.assertEqual(plain.rows[:plain.height - BAR - 40], blurred.rows[:blurred.height - BAR - 40])

    def test_window_blur_behind_transparent_pixels(self):
        """window-blur=yes blurs behind a window even where its pixels are
        fully transparent (Firefox/Zen draw like that)."""
        def column(conf):
            with Session() as vela:
                os.makedirs(os.path.dirname(vela.config_path()), exist_ok=True)
                with open(vela.config_path(), "w") as f:
                    f.write(conf)
                vela.command("reload-config")
                shell(vela, blur=False)
                window = vela.open_window(400, 300, command=[tool("vela-pattern"), "--clear", "400", "300"])
                w = vela.state().window(window)
                image = vela.pixels()
                return [image.at(w["x"] + 200, y) for y in range(w["y"] + 20, w["y"] + 280)]
        # The wallpaper's bands: sharp steps between rows, gone when blurred
        # (the window's shadow alone only shades them).
        step = lambda pixels: max(max(abs(a - b) for a, b in zip(p, q)) for p, q in zip(pixels, pixels[1:]))
        self.assertGreater(step(column("window-blur=no\n")), 40)
        self.assertLess(step(column("window-blur=yes\nwindow-blur-all=yes\n")), 12)

    def test_window_opacity(self):
        """window-opacity: the forced windows mixed (in linear light) with the
        blur behind them, kept at full strength; not when fullscreen; at once
        on reload-config."""
        blur = "window-blur=yes\nwindow-blur-all=yes\n"
        def configure(vela, conf):
            os.makedirs(os.path.dirname(vela.config_path()), exist_ok=True)
            with open(vela.config_path(), "w") as f:
                f.write(conf)
            vela.command("reload-config")
            time.sleep(0.3)
        def center(args, conf, then=None):
            with Session() as vela:
                configure(vela, conf)
                shell(vela, blur=False)
                window = vela.open_window(400, 300, command=[tool("vela-pattern"), *args, "400", "300"])
                w = vela.state().window(window)
                x, y = (960, 540) if w["fullscreen"] else (w["x"] + 200, w["y"] + 150)
                before = vela.pixels().at(x, y)
                if then is None:
                    return before
                configure(vela, then)
                return before, vela.pixels().at(x, y)
        # vela-pattern's pixel (x, y) is (x & 255, y & 255, ...): opaque, as it is.
        opaque, translucent = center([], blur, blur + "window-opacity=60\n")
        self.assertEqual(opaque, (200, 150, 0))
        behind = center(["--clear"], blur + "window-opacity=60\n") # the blur alone
        to_linear = lambda c: c / 255 / 12.92 if c <= 10 else ((c / 255 + 0.055) / 1.055) ** 2.4
        to_srgb = lambda l: 255 * (l * 12.92 if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055)
        for got, front, back in zip(translucent, opaque, behind):
            self.assertAlmostEqual(got, to_srgb(0.6 * to_linear(front) + 0.4 * to_linear(back)), delta=3)
        self.assertEqual(center(["--fullscreen"], blur + "window-opacity=60\n"), (192, 28, 50))


class Damage(unittest.TestCase):
    def check(self, scale):
        with Session(scale=scale) as vela:
            shell(vela)
            partial = drag_over_the_taskbar(vela)
        with Session(scale=scale, env={"VELA_DEBUG_DAMAGE": "1"}) as vela:
            shell(vela)
            full = drag_over_the_taskbar(vela)
        different = sum(1 for y in range(full.height) if partial.rows[y] != full.rows[y])
        self.assertEqual(different, 0, f"{different} rows differ from a full redraw")

    def test_partial_redraw_equals_full_redraw(self):
        self.check(1.0)

    def test_partial_redraw_equals_full_redraw_at_125(self):
        self.check(1.25)


if __name__ == "__main__":
    unittest.main()
