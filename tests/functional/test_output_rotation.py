# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

"""Mixed landscape/portrait regression tests for the real output-management path.

Requires a Vulkan-capable GPU: the functional runner skips on GPU-less CI.
The assertions check layout, window placement AND rendered pixels; a correct
logical output box alone cannot prove the rotated output actually draws.
"""

import time
import unittest

from harness import Session


class MixedOutputRotation(unittest.TestCase):
    def setUp(self):
        self.vela = Session(scale="HEADLESS-1=1,HEADLESS-2=1.5", size="1920x1080@60")
        self.vela.start()

    def tearDown(self):
        self.vela.stop()

    def add_output(self, name="HEADLESS-2"):
        self.vela.command("test-output add 1920x1080")
        return self.vela.wait_for(lambda s: any(o["name"] == name for o in s["outputs"]),
                                  what=f"{name} connected")

    @staticmethod
    def output(state, name):
        return next(o for o in state["outputs"] if o["name"] == name)

    def assert_disjoint(self, state):
        """Output rectangles must not overlap, regardless of sign or orientation."""
        enabled = [o for o in state["outputs"] if o["enabled"]]
        for i, left in enumerate(enabled):
            for right in enabled[i + 1:]:
                separated = (left["x"] + left["w"] <= right["x"]
                             or right["x"] + right["w"] <= left["x"]
                             or left["y"] + left["h"] <= right["y"]
                             or right["y"] + right["h"] <= left["y"])
                self.assertTrue(separated, f"output boxes overlap: {left} / {right}")

    def rotate(self, angle, x, y=0, scale=1):
        self.vela.randr("--output", "HEADLESS-2", "--scale", str(scale),
                        "--pos", f"{x},{y}", "--transform", str(angle))
        expected = 0 if angle == "normal" else {"90": 1, "180": 2, "270": 3}[str(angle)]
        width, height = (1920, 1080) if expected in (0, 2) else (1080, 1920)
        width, height = round(width / scale), round(height / scale)
        return self.vela.wait_for(
            lambda s: (lambda o: (o["x"], o["y"], o["w"], o["h"], o["transform"])
                       == (x, y, width, height, expected))(self.output(s, "HEADLESS-2")),
            what=f"rotation {angle} applied and layout resized")

    def drag_to_second(self, window, state):
        start = state.window(window)
        second = self.output(state, "HEADLESS-2")
        sx, sy = start["x"] + 50, start["y"] + 50
        tx, ty = second["x"] + second["w"] // 2, second["y"] + 200
        self.vela.input("move", sx, sy, "keydown", "super", "down", "sleep", "50",
                        "move", (sx + tx) // 2, (sy + ty) // 2, "sleep", "50",
                        "move", tx, ty, "sleep", "100", "up", "keyup", "super")
        return self.vela.wait_for(lambda s: s.window(window)["output"] == "HEADLESS-2",
                                  what="window moved to the portrait output")

    def assert_picture_stable(self, index=1):
        # A static test-pattern window must not flicker between partial redraws.
        time.sleep(0.25)
        images = [self.vela.pixels(output=index)]
        for _ in range(2):
            time.sleep(0.12)
            images.append(self.vela.pixels(output=index))
        for image in images[1:]:
            self.assertEqual((image.width, image.height), (images[0].width, images[0].height))
            self.assertEqual(image.rows, images[0].rows, "static portrait output flickered")

    def test_portrait_90_renders_maximized_window_without_overlap(self):
        self.add_output()
        state = self.rotate("90", 1920, scale=1)
        self.assert_disjoint(state)
        window = self.vela.open_window(300, 200)
        state = self.drag_to_second(window, self.vela.state())
        self.vela.keys("super+up")
        state = self.vela.wait_for(lambda s: s.window(window)["maximized"],
                                   what="window maximized on portrait output")
        second = self.output(state, "HEADLESS-2")
        w = state.window(window)
        self.assertEqual((w["x"], w["y"], w["w"], w["h"]),
                         (second["x"], second["y"], second["w"], second["h"]))
        image = self.vela.pixels(output=1)
        self.assertEqual((image.width, image.height), (1080, 1920))
        self.assertNotEqual(image.at(540, 960), (0, 0, 0), "portrait rendering is blank")
        self.assert_picture_stable()

    def test_270_and_rotation_changes_keep_the_image_and_layout(self):
        self.add_output()
        for angle, expected in (("90", (1080, 1920)), ("270", (1080, 1920)),
                                ("180", (1920, 1080)), ("normal", (1920, 1080))):
            state = self.rotate(angle, 1920, scale=1)
            self.assert_disjoint(state)
            image = self.vela.pixels(output=1)
            self.assertEqual((image.width, image.height), expected)
        # A fresh window rendered after the transitions must remain visible.
        window = self.vela.open_window(300, 200)
        self.drag_to_second(window, self.vela.state())
        self.assert_picture_stable()

    def test_portrait_left_of_landscape_at_fractional_scale(self):
        self.add_output()
        state = self.rotate("90", -720, scale=1.5)
        self.assert_disjoint(state)
        second = self.output(state, "HEADLESS-2")
        self.assertEqual((second["x"], second["y"], second["w"], second["h"]),
                         (-720, 0, 720, 1280))
        image = self.vela.pixels(output=1)
        self.assertEqual((image.width, image.height), (1080, 1920))

    def test_four_outputs_mixed_orientation(self):
        self.add_output()
        self.vela.randr("--output", "HEADLESS-2", "--pos", "1920,0")
        self.add_output("HEADLESS-3")
        self.vela.randr("--output", "HEADLESS-3", "--scale", "1",
                        "--pos", "-1080,0", "--transform", "90")
        self.add_output("HEADLESS-4")
        self.vela.randr("--output", "HEADLESS-4", "--scale", "1",
                        "--pos", "3200,0", "--transform", "270")
        state = self.vela.wait_for(lambda s: len(s["outputs"]) == 4
                                  and self.output(s, "HEADLESS-3")["w"] == 1080
                                  and self.output(s, "HEADLESS-4")["w"] == 1080,
                                  what="two landscape and two portrait outputs")
        self.assert_disjoint(state)
        for name, geometry in {
            "HEADLESS-1": (0, 0, 1920, 1080),
            "HEADLESS-2": (1920, 0, 1280, 720),
            "HEADLESS-3": (-1080, 0, 1080, 1920),
            "HEADLESS-4": (3200, 0, 1080, 1920),
        }.items():
            o = self.output(state, name)
            self.assertEqual((o["x"], o["y"], o["w"], o["h"]), geometry, name)


if __name__ == "__main__":
    unittest.main()
