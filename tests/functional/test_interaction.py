# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pointer and keyboard on windows (interact.c, bindings.c, snap.c): the
invisible resize borders, keyboard "Move" and "Size", the window menu (right
click on the title, Alt+Space), snapping by dragging against the edge with
Snap Assist, snap layouts (Win+Z, and dragging to the top) and snap groups.
The shell is a FakeShell collecting the messages."""

import json
import os
import subprocess
import time
import unittest

from harness import Session, tool


class Interaction(unittest.TestCase):
    def setUp(self):
        self.vela = Session()
        self.vela.start()
        self.shell = self.vela.fake_shell()

    def tearDown(self):
        self.vela.stop()

    def frame(self, window):
        """The window's frame, title bar included."""
        w = self.vela.state().window(window)
        return w["x"], w["y"], w["w"], w["h"]

    def test_resize_from_invisible_border(self):
        window = self.vela.open_window(300, 200, decorated=True)
        x, y, w, h = self.frame(window)
        # 4 pixels outside the right edge, at half height.
        grab_x, grab_y = x + w + 4, y + h // 2
        self.vela.input("move", grab_x, grab_y, "sleep", "50", "down", "sleep", "50",
                        "move", grab_x + 50, grab_y, "sleep", "50", "move", grab_x + 100, grab_y, "sleep", "100", "up")
        state = self.vela.wait_for(lambda s: s.window(window)["w"] == w + 100, what="100 wider")
        self.assertEqual(state.window(window)["x"], x)

    def test_keyboard_move_and_cancel(self):
        window = self.vela.open_window(300, 200, decorated=True)
        x, y, _, _ = self.frame(window)
        self.vela.command("window active move")
        self.vela.keys("Right", "Right", "Right", "Down", "Return")
        state = self.vela.wait_for(lambda s: s.window(window)["x"] == x + 30, what="moved 30 to the right")
        self.assertEqual(self.frame(window)[1], y + 10)
        # Esc: back to how it was.
        self.vela.command("window active move")
        self.vela.keys("Left", "Left", "Escape")
        time.sleep(0.2)
        self.assertEqual(self.vela.state().window(window)["x"], x + 30)

    def test_keyboard_resize(self):
        window = self.vela.open_window(300, 200, decorated=True)
        _, _, w, _ = self.frame(window)
        self.vela.command("window active resize")
        # The first key chooses the edge, the next ones move it (Ctrl: by one
        # unit).
        self.vela.keys("Right", "Right", "Right", "ctrl+Right", "Return")
        self.vela.wait_for(lambda s: s.window(window)["w"] == w + 21, what="21 wider")

    def test_media_keys_reach_the_shell_whatever_has_focus(self):
        window = self.vela.open_window(300, 200)
        self.vela.wait_for(lambda s: s["focused"] == window)
        self.vela.keys("XF86AudioPlay", "XF86AudioNext", "XF86AudioRaiseVolume", "XF86AudioLowerVolume",
                       "XF86AudioMute", "XF86AudioMicMute")
        self.shell.wait_for("volume mic-mute")
        self.assertEqual(self.shell.received("media") + self.shell.received("volume"),
                         ["media play-pause", "media next", "volume up", "volume down", "volume mute",
                          "volume mic-mute"])

    def test_held_volume_key_repeats(self):
        # With the keyboard's delay and rate (the virtual one: 600 ms, 25 a
        # second); released, it stops.
        self.vela.input("keydown", "XF86AudioRaiseVolume", "sleep", "1000", "keyup", "XF86AudioRaiseVolume")
        time.sleep(0.3)
        held = len(self.shell.received("volume up"))
        self.assertGreaterEqual(held, 6)
        self.assertLessEqual(held, 25)
        time.sleep(0.3)
        self.assertEqual(len(self.shell.received("volume up")), held)
        # A knob's notch (pressed and released at once) is one step.
        self.shell.clear()
        self.vela.keys("XF86AudioLowerVolume")
        time.sleep(0.6)
        self.assertEqual(self.shell.received("volume down"), ["volume down"])

    def test_win_ctrl_v_opens_the_sound_page(self):
        self.vela.keys("super+ctrl+v")
        self.shell.wait_for("sound-output")
        self.vela.keys("super+v")
        self.shell.wait_for("clipboard")
        self.assertEqual(self.shell.received("sound-output"), ["sound-output"])

    def test_window_menu(self):
        window = self.vela.open_window(300, 200, decorated=True)
        x, y, _, _ = self.frame(window)
        self.vela.input("move", x + 120, y + 16, "sleep", "50", "click", "right")
        line = self.shell.wait_for("window-menu ")
        self.assertEqual(line.split()[1:3], [window, "HEADLESS-1"])
        self.assertEqual(line.split()[3:], [str(x + 120), str(y + 16), "0", "1", "0", "0", "0"])  # ... maximized, resizable, keyboard, always on top, hidden from capture
        self.shell.clear()
        self.vela.keys("alt+space")
        line = self.shell.wait_for("window-menu ")
        self.assertEqual(line.split()[1], window)
        self.assertEqual(line.split()[-3], "1") # from the keyboard

    def test_drag_to_edge_snaps_and_offers_assist(self):
        other = self.vela.open_window(401, 301)
        window = self.vela.open_window(300, 200, decorated=True)
        x, y, _, _ = self.frame(window)
        grab_x, grab_y = x + 100, y + 16
        self.vela.input("move", grab_x, grab_y, "sleep", "50", "down", "sleep", "50",
                        "move", grab_x - 200, grab_y + 100, "sleep", "50", "move", 0, 500, "sleep", "300", "up")
        self.vela.wait_for(lambda s: s.window(window)["snap"] == [0, 0, 6, 12], what="snapped left")
        line = self.shell.wait_for("snap-assist ")
        self.assertIn(f'"window":"{window}"', line)
        self.assertIn(f'"candidates":["{other}"]', line)

    def test_snap_preview_uses_the_accent_the_shell_sends(self):
        self.vela.command("theme dark dark #ff0000")
        window = self.vela.open_window(300, 200, decorated=True)
        x, y, _, _ = self.frame(window)
        grab_x, grab_y = x + 100, y + 16
        # Held at the left edge: the preview stays until the button goes up.
        pointer = subprocess.Popen([tool("vela-input"), *map(str, ["move", grab_x, grab_y, "sleep", "50", "down", "sleep", "50",
                                    "move", grab_x - 200, grab_y + 100, "sleep", "50", "move", 0, 500,
                                    "sleep", "1500", "up"])], env=self.vela.client_env)
        try:
            def red(_):
                r, g, b = self.vela.pixels().at(60, 1000)
                return r > 40 and g < 10 and b < 10
            self.vela.wait_for(red, timeout=1.4, read_state=False, what="the preview in the accent (red)")
        finally:
            pointer.wait(timeout=5)

    def drag_to_top(self, window, then=(), top=4):
        """Grabs the title bar and drags to the top center of the output."""
        x, y, _, _ = self.frame(window)
        grab_x, grab_y = x + 100, y + 16
        self.vela.input("move", grab_x, grab_y, "sleep", "50", "down", "sleep", "50",
                        "move", grab_x + 50, grab_y + 50, "sleep", "50", "move", 960, top, "sleep", "100", *then)

    def drag_messages(self):
        """The "snap-drag" lines, the "show" one as (output, its JSON)."""
        out = []
        for line in self.shell.received("snap-drag"):
            if line.startswith("snap-drag show "):
                _, _, output, data = line.split(" ", 3)
                line = ("show", output, json.loads(data))
            out.append(line)
        return out

    def check_show(self, message, x, y, scale):
        self.assertEqual(message[:2], ("show", "HEADLESS-1"))
        data = message[2]
        self.assertEqual((data["x"], data["y"], data["scale"]), (x, y, scale))
        self.assertEqual(len(data["layouts"]), 8)
        self.assertEqual(data["layouts"][3], [[0, 0, 4, 12], [4, 0, 8, 12], [8, 0, 12, 12]])

    def test_drag_to_top_opens_layouts_and_drops_on_a_zone(self):
        self.vela.open_window(401, 301)
        window = self.vela.open_window(300, 200, decorated=True)
        # The pill, 600 wide, centered 8 below the top; then the fourth
        # layout (three columns), its middle zone. One gesture: vela-input
        # lets the button go when it exits.
        self.drag_to_top(window, ["move", 924, 36, "sleep", "100", "up"])
        self.vela.wait_for(lambda s: s.window(window)["snap"] == [4, 0, 8, 12], what="snapped in the middle")
        messages = self.drag_messages()
        self.check_show(messages[0], 660, 8, 1)
        self.assertEqual(messages[1:], ["snap-drag hover 3 1", f"snap-drag drop {window} 3"])
        self.shell.wait_for("snap-assist ")

    def test_drag_layouts_trigger_distance_and_scale(self):
        os.makedirs(os.path.dirname(self.vela.config_path()), exist_ok=True)
        with open(self.vela.config_path(), "w") as f:
            f.write("snap-drag-trigger-distance=100\nsnap-drag-scale=150\n")
        self.vela.command("reload-config")
        window = self.vela.open_window(300, 200, decorated=True)
        # 90 from the top opens them now; at 150% the pill is 900 wide, 12
        # below the top, the same zone farther right and lower.
        self.drag_to_top(window, ["move", 906, 54, "sleep", "100", "up"], top=90)
        self.vela.wait_for(lambda s: s.window(window)["snap"] == [4, 0, 8, 12], what="snapped in the middle")
        messages = self.drag_messages()
        self.check_show(messages[0], 510, 12, 1.5)
        self.assertEqual(messages[1:], ["snap-drag hover 3 1", f"snap-drag drop {window} 3"])

    def test_drag_layouts_esc_and_setting(self):
        window = self.vela.open_window(300, 200, decorated=True)
        # Esc closes the layouts; released at the top outside them it
        # maximizes, as before.
        self.drag_to_top(window, ["key", "escape", "sleep", "60", "move", 961, 3, "sleep", "100", "up"])
        self.vela.wait_for(lambda s: s.window(window)["maximized"], what="maximized")
        messages = self.drag_messages()
        self.check_show(messages[0], 660, 8, 1)
        self.assertEqual(messages[1:], ["snap-drag hide"])
        os.makedirs(os.path.dirname(self.vela.config_path()), exist_ok=True)
        with open(self.vela.config_path(), "w") as f:
            f.write("snap-drag-layouts=no\n")
        self.vela.command("reload-config")
        self.shell.clear()
        other = self.vela.open_window(300, 200, decorated=True)
        self.drag_to_top(other, ["move", 924, 36, "sleep", "100", "up"])
        self.vela.wait_still(other)
        self.assertEqual(self.vela.state().window(other)["snap"], None)
        self.assertEqual(self.shell.received("snap-drag"), [])

    def test_snap_layouts_and_groups(self):
        first = self.vela.open_window(401, 301)
        second = self.vela.open_window(300, 200)
        self.vela.keys("super+z")
        line = self.shell.wait_for("snap-layouts ")
        self.assertEqual(line.split()[1:3], [second, "HEADLESS-1"])
        self.assertEqual(line.split()[-1], "1")
        # As from Snap Assist: the first on the left, the second next to it.
        self.vela.command(f"window {first} snap 0 0 6 12")
        self.vela.command(f"window {second} snap 6 0 12 12 quiet {first}")
        state = self.vela.wait_for(lambda s: s.window(second)["snapGroup"] != 0, what="a snap group")
        self.assertEqual(state.window(first)["snapGroup"], state.window(second)["snapGroup"])
        # The shell may still have an older "workspaces" line, without the
        # group: wait for the one that has it.
        self.shell.wait_for("workspaces ", containing='"snapGroups":[{"output":"HEADLESS-1"')
        # The group from the taskbar: all forward, the chosen one focused.
        self.vela.command(f"window {first} activate-group")
        self.vela.wait_for(lambda s: s["focused"] == first, what="the chosen window focused")
        # Moving one away ends the group of two.
        self.vela.command(f"window {second} restore")
        self.vela.wait_for(lambda s: s.window(first)["snapGroup"] == 0, what="the group dissolved")


if __name__ == "__main__":
    unittest.main()
