# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""pick-window: the command's connection waits until the user clicks a window;
the click reaches nobody, panels are ignored, Esc and the right button cancel."""

import json
import os
import socket
import subprocess
import unittest

from harness import Session, tool

RULES = """\
window-rule=pick-a|position=100,100,size=300x200
window-rule=pick-b|position=800,400,size=300x200
"""


class PickWindow(unittest.TestCase):
    def setUp(self):
        self.vela = Session()
        self.vela.start()
        os.makedirs(os.path.dirname(self.vela.config_path()), exist_ok=True)
        with open(self.vela.config_path(), "w") as config:
            config.write(RULES)
        self.vela.command("reload-config")
        self.a = self.vela.open_window(app_id="pick-a")
        self.b = self.vela.open_window(app_id="pick-b")
        self.vela.wait_for(lambda s: s["focused"] == self.b, what="b active")

    def tearDown(self):
        self.vela.stop()

    def pick(self):
        connection = socket.socket(socket.AF_UNIX)
        connection.settimeout(5)
        connection.connect(self.vela.socket_path)
        connection.sendall(b"pick-window\n")
        # The compositor is in the mode once the cursor is a crosshair; the
        # input events below are queued behind the command anyway.
        self.vela.input("sleep", "150")
        return connection

    def answer(self, connection):
        data = b""
        while not data.endswith(b"\n"):
            data += connection.recv(512)
        connection.close()
        return data.decode().strip()

    def center(self, window):
        w = self.vela.state().window(window)
        return str(w["x"] + w["w"] // 2), str(w["y"] + w["h"] // 2)

    def test_click_picks_the_window_and_is_swallowed(self):
        connection = self.pick()
        x, y = self.center(self.a)
        self.vela.input("move", x, y, "sleep", "50", "click")
        reply = json.loads(self.answer(connection))
        self.assertEqual(reply["appId"], "pick-a")
        self.assertEqual(reply["output"], "HEADLESS-1")
        # A normal click would have activated it: this one reached nobody.
        self.assertEqual(self.vela.state()["focused"], self.b)
        # Back to normal afterwards.
        self.vela.input("move", x, y, "sleep", "50", "click")
        self.vela.wait_for(lambda s: s["focused"] == self.a, what="a active after a normal click")

    def test_escape_cancels(self):
        connection = self.pick()
        self.vela.input("key", "Escape")
        self.assertEqual(self.answer(connection), "cancel")

    def test_right_click_cancels(self):
        connection = self.pick()
        x, y = self.center(self.a)
        self.vela.input("move", x, y, "sleep", "50", "click", "right")
        self.assertEqual(self.answer(connection), "cancel")
        self.assertEqual(self.vela.state()["focused"], self.b)

    def test_panels_are_ignored(self):
        panel = subprocess.Popen([tool("vela-panel"), "taskbar", "--namespace", "vela-taskbar"],
                                 env=self.vela.client_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.vela.clients.append(panel)
        self.vela.input("sleep", "300")
        connection = self.pick()
        self.vela.input("move", "960", "1060", "sleep", "50", "click")  # the bar
        self.vela.input("sleep", "100")
        connection.settimeout(0.5)
        with self.assertRaises(socket.timeout):
            connection.recv(16)  # still waiting
        x, y = self.center(self.b)
        self.vela.input("move", x, y, "sleep", "50", "click")
        connection.settimeout(5)
        self.assertEqual(json.loads(self.answer(connection))["appId"], "pick-b")


if __name__ == "__main__":
    unittest.main()
