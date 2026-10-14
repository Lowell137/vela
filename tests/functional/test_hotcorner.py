# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""The hot corner stays at the top-left corner, whichever edge the taskbar
(and its Start button) is on: the shell is a FakeShell collecting messages."""

import time
import unittest

from harness import Session


class HotCorner(unittest.TestCase):
    def test_top_left_corner_opens_task_view_with_the_taskbar_anywhere(self):
        for position in ("bottom", "top", "left", "right"):
            with self.subTest(position=position), Session() as vela:
                shell = vela.fake_shell()
                vela.command(f"taskbar-position {position}")
                w, h = vela.state().output["w"], vela.state().output["h"]
                vela.input("move", 500, 500, "sleep", "100")
                # The other three corners do nothing.
                for x, y in ((w - 2, 1), (1, h - 2), (w - 2, h - 2)):
                    vela.input("move", x, y, "sleep", "150", "move", 500, 500, "sleep", "100")
                time.sleep(0.2)
                self.assertEqual(shell.received("task-view"), [])
                vela.input("move", 1, 1, "sleep", "150")
                shell.wait_for("task-view")


if __name__ == "__main__":
    unittest.main()
