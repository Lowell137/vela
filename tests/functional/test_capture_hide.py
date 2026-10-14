# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Windows hidden from screen capture (capture=no, docs/renderer.md §15): an
output capture (ext-image-copy-capture with vela-screen-dump, wlr-screencopy
with wf-recorder) shows what lies under them, their popups, subsurfaces and
dialogs included, the same as if they weren't there; a toplevel capture gets
nothing, except on the shell's own connection; no extra drawing without a
capture."""

import os
import shutil
import signal
import subprocess
import tempfile
import time
import unittest

from harness import BUILD, Session, tool

SIZE = "640x480@60"
RULES = """\
window-rule=hideme|capture=no,position=40,40
window-rule=showme|position=340,40
window-rule=clearwin|position=140,90
"""
HIDDEN = (40, 40, 290, 220) # the hidden window's frame: x0, y0, x1, y1
SHOWN = (340, 40, 590, 220)
MAGENTA, CYAN, YELLOW = (255, 0, 255), (0, 255, 255), (255, 255, 0)


def pattern(app_id, *options):
    return [tool("vela-pattern"), "--app-id", app_id, *options, "250", "180"]


def colors(image, box):
    x0, y0, x1, y1 = box
    return image.colors(x0, y0, x1, y1)


class CaptureHide(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir, ignore_errors=True)

    def session(self, extra="", **kwargs):
        vela = Session(size=SIZE, **kwargs)
        vela.start()
        self.addCleanup(vela.stop)
        os.makedirs(os.path.dirname(vela.config_path()), exist_ok=True)
        with open(vela.config_path(), "w") as config:
            config.write(RULES + extra)
        vela.command("reload-config")
        return vela

    def open(self, vela, app_id, *options):
        window = vela.open_window(command=pattern(app_id, *options))
        return window

    def settle(self, vela):
        time.sleep(0.4) # open and close animations, the next frame

    # ------------------------------------------------- output capture --

    def test_output_capture_shows_what_lies_under(self):
        vela = self.session()
        hidden = self.open(vela, "hideme")
        shown = self.open(vela, "showme")
        state = vela.state()
        self.assertTrue(state.window(hidden)["captureHidden"])
        self.assertFalse(state.window(shown)["captureHidden"])
        self.settle(vela)
        image = vela.pixels()
        self.assertEqual(colors(image, HIDDEN), {(0, 0, 0)}, "the hidden window is in the capture")
        self.assertGreater(len(colors(image, SHOWN)), 100, "the other window is missing")
        # The same picture as without the window at all.
        vela.command(f"window {hidden} close")
        vela.wait_for(lambda s: not s.has(hidden))
        self.settle(vela)
        self.assertEqual(vela.pixels().rows, image.rows)

    def test_popup_subsurface_and_dialog_are_hidden_too(self):
        def capture(app_id):
            vela = self.session()
            before = {w["id"] for w in vela.state().windows}
            client = subprocess.Popen(pattern(app_id, "--popup", "200,100", "--subsurface", "60,40", "--child"),
                                      env=vela.client_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            vela.clients.append(client)
            state = vela.wait_for(lambda s: len({w["id"] for w in s.windows} - before) == 2, what="window and dialog")
            self.settle(vela)
            image = vela.pixels()
            seen = image.colors(0, 0, image.width, image.height)
            vela.stop()
            return state, seen
        state, seen = capture("showme")
        self.assertTrue({MAGENTA, CYAN, YELLOW} <= seen, f"the test client didn't draw popup, subsurface and dialog: {({MAGENTA, CYAN, YELLOW} & seen)} {state.windows}")
        state, seen = capture("hideme")
        self.assertTrue(all(w["captureHidden"] for w in state.windows), "the dialog isn't hidden with its parent")
        self.assertFalse({MAGENTA, CYAN, YELLOW} & seen, "popup, subsurface or dialog in the capture")
        self.assertEqual(seen, {(0, 0, 0)})

    def test_blur_above_does_not_show_it(self):
        # A transparent window with forced blur over the hidden one: its blur
        # in the capture must be that of what lies under, not of the window.
        vela = self.session("window-blur=yes\nwindow-blur-include=clearwin\n")
        hidden = self.open(vela, "hideme")
        self.open(vela, "clearwin", "--clear")
        self.settle(vela)
        image = vela.pixels()
        vela.command(f"window {hidden} close")
        vela.wait_for(lambda s: not s.has(hidden))
        self.settle(vela)
        self.assertEqual(vela.pixels().rows, image.rows)

    def test_fullscreen(self):
        vela = self.session()
        self.open(vela, "showme")
        self.open(vela, "hideme", "--fullscreen")
        vela.wait_for(lambda s: any(w["fullscreen"] for w in s.windows))
        self.settle(vela)
        image = vela.pixels()
        self.assertGreater(len(colors(image, SHOWN)), 100, "the capture should show the window under it")

    @unittest.skipUnless(shutil.which("wf-recorder") and shutil.which("ffmpeg"), "wf-recorder or ffmpeg missing")
    def test_screencopy_and_marking_while_recording(self):
        """wlr-screencopy (wf-recorder, lossless): a window marked from the
        window menu while the recording runs leaves the stream at once."""
        vela = self.session()
        window = self.open(vela, "showme")
        out = os.path.join(self.dir, "out.mkv")
        recorder = subprocess.Popen(["wf-recorder", "-y", "-c", "ffv1", "-x", "bgr0", "-f", out],
                                    env=vela.client_env, stdin=subprocess.DEVNULL,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            wiggle = lambda n: [vela.input("move", str(600 - i % 5), "450") or time.sleep(0.1) for i in range(n)]
            wiggle(8) # frames come with damage: the pointer makes some
            vela.command(f"window {window} capture-hide on")
            vela.wait_for(lambda s: s.window(window)["captureHidden"])
            wiggle(8)
            recorder.send_signal(signal.SIGINT)
            wiggle(6) # it finishes with one more frame
            recorder.wait(timeout=10)
        finally:
            if recorder.poll() is None:
                recorder.kill()
                recorder.wait()
        first, last = self.frame(out, first=True), self.frame(out, first=False)
        self.assertGreater(len(colors(first, SHOWN)), 100, "the recording didn't capture the window before")
        self.assertEqual(colors(last, SHOWN), {(0, 0, 0)}, "the window stays in the recording")
        self.assertGreater(vela.state().output["capturePasses"], 0)

    def frame(self, video, first):
        path = os.path.join(self.dir, "first.ppm" if first else "last.ppm")
        options = ["-frames:v", "1"] if first else ["-update", "1"]
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", video, *options, "-pix_fmt", "rgb24", path], check=True)
        return Ppm.read(path)

    # ----------------------------------------------- toplevel capture --

    @unittest.skipUnless(shutil.which("grim"), "grim is not installed")
    def test_toplevel_capture_refused(self):
        vela = self.session()
        hidden = self.open(vela, "hideme")
        shown = self.open(vela, "showme")
        def grim(identifier, name):
            # grim 1.5 waits forever after a session that stops at once: that,
            # too, is "nothing captured".
            try:
                return subprocess.run(["grim", "-T", identifier, os.path.join(self.dir, name)], env=vela.client_env,
                                      stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5).returncode
            except subprocess.TimeoutExpired:
                return "timeout"
        self.assertEqual(grim(shown, "shown.png"), 0)
        self.assertNotEqual(grim(hidden, "hidden.png"), 0)
        self.assertFalse(os.path.exists(os.path.join(self.dir, "hidden.png")))
        # Shown again: captured again.
        vela.command(f"window {hidden} capture-hide off")
        vela.wait_for(lambda s: not s.window(hidden)["captureHidden"])
        self.assertEqual(grim(hidden, "again.png"), 0)

    @unittest.skipUnless(shutil.which("grim"), "grim is not installed")
    def test_shell_connection_sees_hidden_windows(self):
        """The client launched with -s (the shell) gets its connection from the
        compositor (WAYLAND_SOCKET): its toplevel captures see hidden windows."""
        ask = os.path.join(self.dir, "id")
        done = os.path.join(self.dir, "rc")
        shot = os.path.join(self.dir, "trusted.png")
        startup = (f"while [ ! -s {ask} ]; do sleep 0.05; done; grim -T \"$(cat {ask})\" {shot}; "
                   f"echo $? > {done}; exit 0")
        vela = self.session(startup=startup)
        hidden = self.open(vela, "hideme")
        with open(ask + ".tmp", "w") as f:
            f.write(hidden)
        os.rename(ask + ".tmp", ask)
        deadline = time.monotonic() + 10
        while not os.path.exists(done) and time.monotonic() < deadline:
            time.sleep(0.05)
        with open(done) as f:
            self.assertEqual(f.read().strip(), "0")
        self.assertTrue(os.path.getsize(shot) > 0)

    @unittest.skipUnless(shutil.which("grim"), "grim is not installed")
    def test_shell_previews_hidden_from_output_capture(self):
        """The trusted shell's preview panels (here a fake one with the
        taskbar tooltip's namespace) leave captures while a window is hidden."""
        panel = f"{tool('vela-panel')} taskbar --namespace vela-taskbar-tooltip"
        vela = self.session(startup=panel)
        vela.wait_for(lambda s: s["layers"], what="the panel")
        self.settle(vela)
        bar = (0, 480 - 48, 640, 480)
        self.assertNotEqual(colors(vela.pixels(), bar), {(0, 0, 0)}, "the panel isn't drawn")
        window = self.open(vela, "hideme")
        self.settle(vela)
        self.assertEqual(colors(vela.pixels(), bar), {(0, 0, 0)}, "a preview panel is in the capture")
        vela.command(f"window {window} close")
        vela.wait_for(lambda s: not s.has(window))
        self.settle(vela)
        self.assertNotEqual(colors(vela.pixels(), bar), {(0, 0, 0)})

    # --------------------------------------------- rules and toggling --

    def test_rule_removed_or_added_reaches_open_windows(self):
        vela = self.session()
        hidden = self.open(vela, "hideme")
        shown = self.open(vela, "showme")
        with open(vela.config_path(), "w") as config:
            config.write("window-rule=showme|capture=no\n")
        vela.command("reload-config")
        state = vela.wait_for(lambda s: s.window(shown)["captureHidden"], what="the new rule on the open window")
        self.assertFalse(state.window(hidden)["captureHidden"])
        self.settle(vela)
        image = vela.pixels()
        self.assertGreater(len(colors(image, HIDDEN)), 100, "visible again once the rule is gone")
        self.assertEqual(colors(image, SHOWN), {(0, 0, 0)})

    def test_window_menu_choice(self):
        vela = self.session()
        window = self.open(vela, "showme")
        vela.command(f"window {window} capture-hide toggle")
        vela.wait_for(lambda s: s.window(window)["captureHidden"])
        self.settle(vela)
        self.assertEqual(colors(vela.pixels(), SHOWN), {(0, 0, 0)})
        vela.command(f"window {window} capture-hide toggle")
        vela.wait_for(lambda s: not s.window(window)["captureHidden"])
        self.settle(vela)
        self.assertGreater(len(colors(vela.pixels(), SHOWN)), 100)

    # ----------------------------------------------------------- cost --

    def test_no_extra_drawing_without_capture(self):
        vela = self.session()
        self.open(vela, "showme")
        vela.pixels() # a capture with nothing hidden: no extra drawing either
        self.assertEqual(vela.state().output["capturePasses"], 0)
        window = self.open(vela, "hideme")
        for x in range(60, 260, 20): # frames over the hidden window
            vela.input("move", str(x), "100")
        vela.command(f"window {window} capture-hide off")
        vela.command(f"window {window} capture-hide on")
        self.settle(vela)
        self.assertGreater(vela.state().output["frames"], 0)
        self.assertEqual(vela.state().output["capturePasses"], 0, "extra drawing without a capture")
        vela.pixels()
        self.assertGreater(vela.state().output["capturePasses"], 0)


SHELL = os.path.join(BUILD, "shell", "vela-shell")


@unittest.skipUnless(os.path.exists(SHELL) and shutil.which("dbus-daemon"), "vela-shell not built")
class QtShellTrust(unittest.TestCase):
    """The real Qt shell, launched with -s, uses the connection the compositor
    made for it (WAYLAND_SOCKET): its panels are the trusted shell's. The same
    shell started like any app is not."""

    def bus(self):
        bus = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--nopidfile", "--print-address=1"],
                               stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        def stop():
            bus.terminate()
            bus.wait(timeout=3)
            bus.stdout.close()
        self.addCleanup(stop)
        return bus.stdout.readline().strip()

    QT = {"QT_QPA_PLATFORM": "wayland", "QT_WAYLAND_SHELL_INTEGRATION": "layer-shell", "QT_WAYLAND_RECONNECT": "0",
          "QSG_RHI_BACKEND": "opengl", "VELA_SYNC_FILE": "0"}

    def taskbar(self, vela):
        state = vela.wait_for(lambda s: any(l["namespace"] == "vela-taskbar" and l["mapped"] for l in s["layers"]),
                              timeout=20, what="the real taskbar")
        return next(l for l in state["layers"] if l["namespace"] == "vela-taskbar")

    def test_shell_launched_by_the_compositor_is_trusted(self):
        env = dict(self.QT, DBUS_SESSION_BUS_ADDRESS=self.bus())
        vela = Session(size="1280x800@60", env=env, startup=SHELL)
        vela.start()
        self.addCleanup(vela.stop)
        self.assertTrue(self.taskbar(vela)["shell"])

    def test_shell_started_as_an_app_is_not(self):
        vela = Session(size="1280x800@60", env={"DBUS_SESSION_BUS_ADDRESS": self.bus()})
        vela.start()
        self.addCleanup(vela.stop)
        shell = subprocess.Popen([SHELL], env=dict(vela.client_env, **self.QT), stdout=vela.log, stderr=vela.log)
        vela.clients.append(shell)
        self.assertFalse(self.taskbar(vela)["shell"])


class Ppm:
    """A binary PPM (P6, 8 bit), as ffmpeg writes it."""

    def __init__(self, width, height, rows):
        self.width, self.height, self.rows = width, height, rows

    @staticmethod
    def read(path):
        with open(path, "rb") as file:
            data = file.read()
        fields, position = [], 0
        while len(fields) < 4:
            while data[position:position + 1].isspace():
                position += 1
            end = position
            while not data[end:end + 1].isspace():
                end += 1
            fields.append(data[position:end])
            position = end
        assert fields[0] == b"P6" and fields[3] == b"255", "not an 8-bit P6"
        width, height = int(fields[1]), int(fields[2])
        pixels = data[position + 1:]
        stride = width * 3
        return Ppm(width, height, [pixels[y * stride:(y + 1) * stride] for y in range(height)])

    def at(self, x, y):
        return tuple(self.rows[y][x * 3:x * 3 + 3])

    def colors(self, x0, y0, x1, y1):
        return {self.at(x, y) for y in range(y0, y1) for x in range(x0, x1)}


if __name__ == "__main__":
    unittest.main()
