# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""fullscreen-dnd: a fullscreen window sends the shell "fullscreen on" and
its end "fullscreen off"; fullscreen-dnd=no sends nothing."""

import os
import time
import unittest

from harness import Session, tool


class FullscreenDnd(unittest.TestCase):
    def fullscreen(self, vela):
        w = vela.open_window(command=[tool("vela-pattern"), "--fullscreen"])
        vela.wait_for(lambda s: s.window(w)["fullscreen"], what="fullscreen")
        return w

    def test_on_while_fullscreen_off_after(self):
        with Session() as vela:
            shell = vela.fake_shell()
            w = self.fullscreen(vela)
            shell.wait_for("fullscreen on", timeout=4)
            vela.command(f"window {w} close")
            shell.wait_for("fullscreen off", timeout=4)

    def test_turned_off_in_config(self):
        with Session() as vela:
            shell = vela.fake_shell()
            os.makedirs(os.path.dirname(vela.config_path()), exist_ok=True)
            with open(vela.config_path(), "a") as config:
                config.write("fullscreen-dnd=no\n")
            vela.command("reload-config")
            self.fullscreen(vela)
            time.sleep(2)
            self.assertFalse(shell.received("fullscreen"))


if __name__ == "__main__":
    unittest.main()
