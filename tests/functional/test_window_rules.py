# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Window rules (window-rule= in vela.conf): output, desktop, size, position,
maximized and always-on-top applied when a window maps; windows of other apps
are left alone."""

import os
import unittest

from harness import Session

RULES = """\
window-rule=RuleOut|output=HEADLESS-2,size=500x350
window-rule=ruleat|position=100,80,size=320x240
window-rule=ruletop|always-on-top=yes
window-rule=ruledesk|desktop=2
window-rule=rulemax|maximized=yes
window-rule=ruletitle|title~=Special,size=260x180,bogus=1
window-rule=titleok|title~=PATTERN,size=260x180
"""


class WindowRules(unittest.TestCase):
    def setUp(self):
        self.vela = Session()
        self.vela.start()
        os.makedirs(os.path.dirname(self.vela.config_path()), exist_ok=True)
        with open(self.vela.config_path(), "w") as config:
            config.write(RULES)
        self.vela.command("test-output add 1920x1080")
        self.vela.wait_for(lambda s: len(s["outputs"]) == 2, what="two outputs")
        self.vela.command("reload-config")

    def tearDown(self):
        self.vela.stop()

    def open(self, app_id):
        window = self.vela.open_window(app_id=app_id)
        return self.vela.state().window(window)

    def test_output_and_size(self):
        window = self.open("ruleout")  # the app id matches ignoring case
        self.assertEqual(window["output"], "HEADLESS-2")
        self.assertEqual((window["w"], window["h"]), (500, 350))
        other = self.open("norule")
        self.assertEqual(other["output"], "HEADLESS-1")
        self.assertEqual((other["w"], other["h"]), (400, 300))

    def test_position_and_size(self):
        window = self.open("ruleat")
        self.assertEqual((window["x"], window["y"], window["w"], window["h"]), (100, 80, 320, 240))

    def test_always_on_top(self):
        self.assertTrue(self.open("ruletop")["alwaysOnTop"])
        self.assertFalse(self.open("norule")["alwaysOnTop"])

    def test_desktop(self):
        self.vela.command("workspace new")
        self.vela.wait_for(lambda s: s["workspaces"] == 2, what="two desktops")
        window = self.open("ruledesk")
        self.assertEqual(window["workspace"], 1)
        self.assertEqual(self.vela.state()["workspace"], 0)  # still on the first desktop
        self.assertEqual(self.open("norule")["workspace"], 0)

    def test_maximized(self):
        window = self.open("rulemax")
        self.assertTrue(window["maximized"])
        self.assertFalse(self.open("norule")["maximized"])

    def test_title_pattern(self):
        self.assertEqual(self.open("ruletitle")["w"], 400)  # the title doesn't match
        window = self.open("titleok")  # vela-pattern's title, ignoring case
        self.assertEqual((window["w"], window["h"]), (260, 180))


if __name__ == "__main__":
    unittest.main()
