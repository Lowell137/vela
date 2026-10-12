# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Vela's title bar (docs/renderer.md §9) for apps that accept it through
xdg-decoration: 32 high, above the content, with the title written by the
compositor and the three buttons; closing, maximizing with a double click.
It's found on screen, not from the state."""

import unittest

from harness import Session

BAR = 32
BUTTON = 46


class Decorations(unittest.TestCase):
    def setUp(self):
        self.vela = Session()
        self.vela.start()

    def tearDown(self):
        self.vela.stop()

    def open(self):
        window = self.vela.open_window(400, 300, decorated=True)
        state = self.vela.wait_for(lambda s: s["focused"] == window)
        w = state.window(window)
        image = self.vela.pixels()
        # From the top, in the column halfway along the title: wallpaper and
        # shadow are black, the bar isn't.
        column = w["x"] + 200
        top = next(y for y in range(image.height) if image.at(column, y) != (0, 0, 0))
        return window, w, image, top

    def test_bar_above_the_content(self):
        window, w, image, top = self.open()
        # The app's content starts right below: vela-pattern encodes the pixel
        # coordinates (R = x, G = y).
        self.assertEqual(image.at(w["x"] + 100, top + BAR), (100, 0, 0))
        self.assertEqual(image.at(w["x"] + 100, top + BAR - 1)[:2] != (100, 0), True)
        # The title ("vela-pattern") is written: antialiasing, several colors.
        self.assertGreaterEqual(len(image.colors(w["x"] + 12, top, w["x"] + 200, top + BAR)), 4)
        # And the button glyphs.
        for i in range(1, 4):
            left = w["x"] + w["w"] - i * BUTTON
            self.assertGreaterEqual(len(image.colors(left, top, left + BUTTON, top + BAR)), 2, f"button {i}")

    def test_close_button(self):
        window, w, image, top = self.open()
        self.vela.input("move", w["x"] + w["w"] - BUTTON // 2, top + BAR // 2, "sleep", "50", "click")
        self.vela.wait_for(lambda s: not s.has(window), what="closed by its close button")

    def test_double_click_on_the_title_maximizes(self):
        window, w, image, top = self.open()
        self.vela.input("move", w["x"] + 150, top + BAR // 2, "sleep", "50", "click", "sleep", "60", "click")
        self.vela.wait_for(lambda s: s.window(window)["maximized"], what="maximized")


class BarImagesAtEveryPosition(unittest.TestCase):
    """Title, icon and glyphs are drawn at the output's scale, so they are
    copied 1:1 (docs/renderer.md §3.4) wherever the window is: the same raster
    gives the same pixels at every position, also where the window's edge falls
    on half a pixel. Dragged by whole units, the window's edge goes through
    every phase a scale has (at 125%: 0, ¼, ½, ¾ of a pixel)."""

    POSITIONS = 40

    def test_same_pixels_at_every_position(self):
        for scale in (1.25, 1.5, 1.75, 2.25):
            with self.subTest(scale=scale), Session(scale=scale, size="2560x1440@60") as vela:
                window = vela.open_window(400, 300, decorated=True)
                vela.wait_for(lambda s: s["focused"] == window)
                reference = bar_images(vela.pixels(), vela.state().window(window), scale)
                self.assertEqual(len(reference), 4, "title and three glyphs")
                for i in range(1, self.POSITIONS):
                    # Back and forth, so the window stays away from the edges
                    # (and their snap zones).
                    step = 1 + i % 5
                    sign = 1 if (i // 10) % 2 == 0 else -1
                    drag(vela, vela.state().window(window), sign * step, sign * (i % 3))
                    w = vela.state().window(window)
                    self.assertFalse(w["maximized"])
                    images = bar_images(vela.pixels(), w, scale)
                    self.assertEqual(len(images), len(reference), f"position {i}")
                    for name, image, expected in zip(("title", "minimize", "maximize", "close"), images, reference):
                        self.assertTrue(image == expected, f"position {i} ({w['x']}, {w['y']}): the {name} changed")


def bar_top(image, w, scale):
    """The bar's first row, in pixels: above it, wallpaper and shadow are black."""
    column = round((w["x"] + 200) * scale)
    return next(y for y in range(image.height) if image.at(column, y) != (0, 0, 0))


def drag(vela, w, dx, dy):
    """Drags the window by its bar by (dx, dy) units. The drag starts past 4
    units, where the window is grabbed: from there the cursor moves by (dx, dy)."""
    x = w["x"] + 200
    y = w["y"] + BAR // 2
    vela.input("move", x, y, "sleep", "50", "down", "sleep", "50", "move", x, y + 10, "sleep", "50",
               "move", x + dx, y + 10 + dy, "sleep", "50", "up", "sleep", "400") # no double click with the next one
    def moved(state):
        now = state.window(w["id"])
        return abs(now["x"] - w["x"] - dx) <= 1 and abs(now["y"] - w["y"] - dy) <= 1
    vela.wait_for(moved, what=f"the window moved by {dx}, {dy}")


def bar_images(image, w, scale):
    """The bar's images (icon, title, glyphs), each cropped to its own pixels:
    runs of columns with something drawn on the bar's color, separated by at
    least 5 units. Away from the rounded corners and from the content."""
    top = bar_top(image, w, scale)
    middle = top + round(BAR / 2 * scale)
    background = image.at(round((w["x"] + 200) * scale), middle)
    left = right = round((w["x"] + 200) * scale)
    while image.at(left - 1, middle) != (0, 0, 0):
        left -= 1
    while image.at(right, middle) != (0, 0, 0):
        right += 1
    rows = range(top + 2, top + int(BAR * scale) - 2)
    columns = range(left + round(9 * scale), right - round(9 * scale))
    drawn = [x for x in columns if any(image.at(x, y) != background for y in rows)]
    runs = []
    for x in drawn:
        if runs and x - runs[-1][1] <= 5 * scale:
            runs[-1][1] = x
        else:
            runs.append([x, x])
    crops = []
    for x0, x1 in runs:
        used = [y for y in rows if any(image.at(x, y) != background for x in range(x0, x1 + 1))]
        crops.append(tuple(image.rows[y][x0 * 3:(x1 + 1) * 3] for y in range(used[0], used[-1] + 1)))
    return crops


if __name__ == "__main__":
    unittest.main()
