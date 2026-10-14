# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Remembered window positions (remember-window-positions in vela.conf): an app
reopens where it closed, unless the setting is off or a window rule says
otherwise; a damaged state file is ignored."""

import json
import os
import unittest

from harness import Session


class WindowMemory(unittest.TestCase):
    def setUp(self):
        self.vela = Session()
        self.vela.start()
        os.makedirs(os.path.dirname(self.vela.config_path()), exist_ok=True)
        self.vela.command("test-output add 1920x1080")
        self.vela.wait_for(lambda s: len(s["outputs"]) == 2, what="two outputs")

    def tearDown(self):
        self.vela.stop()

    def config(self, text):
        with open(self.vela.config_path(), "w") as config:
            config.write(text)
        self.vela.command("reload-config")

    def state_file(self):
        return os.path.join(self.vela.env["XDG_STATE_HOME"], "vela", "window-positions.json")

    def opened(self, app):
        window = self.vela.open_window(app_id=app, decorated=True)
        return self.vela.state().window(window)

    def frame(self, window):
        w = self.vela.state().window(window)
        return (w["x"], w["y"], w["w"], w["h"])

    def close(self, window):
        self.vela.command(f"window {window} close")
        self.vela.wait_for(lambda s: not s.has(window), what="closed")

    def moved_and_closed(self, app):
        """Opens the app, moves and resizes it, closes it; returns the frame it had."""
        window = self.vela.open_window(app_id=app, decorated=True)
        self.vela.command(f"window {window} move")
        self.vela.keys("Right", "Right", "Right", "Right", "Right", "Down", "Down", "Down", "Return")
        self.vela.command(f"window {window} resize")
        self.vela.keys("Right", "Right", "Right", "Return")
        self.vela.wait_still(window)
        frame = self.frame(window)
        self.close(window)
        return frame

    def test_reopens_where_it_closed(self):
        first = self.opened("memo")
        self.close(first["id"])
        frame = self.moved_and_closed("memo")
        self.assertNotEqual(frame[:2], (first["x"], first["y"]))
        window = self.vela.open_window(app_id="memo", decorated=True)
        self.assertEqual(self.frame(window), frame)
        self.assertTrue(os.path.exists(self.state_file()))
        self.assertEqual(self.frame(self.vela.open_window(app_id="other", decorated=True))[:2],
                         (first["x"], first["y"]))  # another app is placed as usual

    def test_maximized(self):
        window = self.vela.open_window(app_id="memomax", decorated=True)
        self.vela.command(f"window {window} maximize")
        self.vela.wait_for(lambda s: s.window(window)["maximized"], what="maximized")
        self.close(window)
        window = self.vela.open_window(app_id="memomax", decorated=True)
        self.vela.wait_for(lambda s: s.window(window)["maximized"], what="reopened maximized")

    def test_disabled(self):
        self.config("remember-window-positions=no\n")
        first = self.opened("memoff")
        self.close(first["id"])
        self.moved_and_closed("memoff")
        window = self.vela.open_window(app_id="memoff", decorated=True)
        self.assertEqual(self.frame(window), (first["x"], first["y"], first["w"], first["h"]))

    def test_rule_wins(self):
        self.moved_and_closed("memorule")
        self.config("window-rule=memorule|position=60,40,size=320x240\n")
        window = self.vela.open_window(app_id="memorule", decorated=True)
        self.assertEqual(self.frame(window), (60, 40, 320, 240))

    def test_output_from_the_file(self):
        os.makedirs(os.path.dirname(self.state_file()), exist_ok=True)
        entry = {"app": "memoout", "output": "HEADLESS-2", "x": 100, "y": 80, "w": 500, "h": 350,
                 "maximized": 0, "desktop": 0}
        with open(self.state_file(), "w") as out:
            out.write('{"version":1,"windows":[\n' + json.dumps(entry, separators=(",", ":")) + "\n]}\n")
        window = self.opened("memoout")
        self.assertEqual(window["output"], "HEADLESS-2")
        self.assertEqual((window["w"], window["h"]), (500, 350))

    def test_broken_file(self):
        os.makedirs(os.path.dirname(self.state_file()), exist_ok=True)
        with open(self.state_file(), "wb") as out:
            out.write(b'{"windows":[{"app":"broken","x":\n\x00\xff garbage {{{ "app":"b","w":-5\n')
        window = self.vela.open_window(app_id="broken", decorated=True)
        self.close(window)
        self.assertIsNone(self.vela.process.poll())  # still running
        with open(self.state_file()) as saved:
            self.assertEqual(json.loads(saved.read())["version"], 1)  # rewritten as valid json


if __name__ == "__main__":
    unittest.main()
