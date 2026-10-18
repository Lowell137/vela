// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import Vela.Controls
import "../qml"

// The quick preview's zoom: the wheel and a pinch zoom around the pointer
// between 1x and 8x, a drag pans without leaving the picture, a double click
// fits it again, and another item starts fitted.
Item {
    id: root
    width: 700
    height: 500

    QuickPeek { id: peek; tab: null }

    TestCase {
        name: "QuickPeekZoom"
        when: windowShown

        property var box
        property var content

        function init() {
            peek.show(TestImage)
            tryVerify(() => peek.kind === "image")
            box = findChild(peek, "zoomBox")
            content = findChild(peek, "zoomContent")
            verify(box && box.visible)
        }
        function cleanup() { peek.path = ""; peek.hide() }

        // The point of the picture under (x, y) of the box.
        function under(x, y) {
            return Qt.point((x - content.x) / box.zoom, (y - content.y) / box.zoom)
        }

        function test_wheel_zooms_around_the_pointer() {
            const x = box.width * 0.7, y = box.height * 0.3
            const before = under(x, y)
            mouseWheel(box, x, y, 0, 120)
            verify(box.zoom > 1.1)
            const after = under(x, y)
            fuzzyCompare(after.x, before.x, 0.5)
            fuzzyCompare(after.y, before.y, 0.5)
            mouseWheel(box, x, y, 0, -120)
            fuzzyCompare(box.zoom, 1, 0.001)
        }

        function test_zoom_stays_between_1x_and_8x() {
            for (let i = 0; i < 30; ++i) mouseWheel(box, 10, 10, 0, 240)
            compare(box.zoom, 8)
            for (let i = 0; i < 30; ++i) mouseWheel(box, 10, 10, 0, -240)
            compare(box.zoom, 1)
            compare(content.x, 0)
            compare(content.y, 0)
        }

        function test_drag_pans_inside_the_picture() {
            const w = box.width, h = box.height
            mouseWheel(box, w / 2, h / 2, 0, 480)
            const x = content.x, y = content.y
            mouseDrag(box, w / 2, h / 2, 40, 30)
            verify(content.x > x && content.y > y)
            mouseDrag(box, w / 2, h / 2, w * 4, h * 4) // far past the edge
            compare(content.x, 0)
            compare(content.y, 0)
            mouseDrag(box, w / 2, h / 2, -w * 4, -h * 4)
            fuzzyCompare(content.x, w - w * box.zoom, 0.5)
            fuzzyCompare(content.y, h - h * box.zoom, 0.5)
        }

        function test_double_click_fits_again() {
            mouseWheel(box, 50, 50, 0, 480)
            verify(box.zoom > 1)
            mouseDoubleClickSequence(box, 50, 50)
            compare(box.zoom, 1)
            compare(content.x, 0)
        }

        function test_another_item_starts_fitted() {
            mouseWheel(box, 50, 50, 0, 480)
            peek.path = TestImage + ".other"
            compare(box.zoom, 1)
            compare(content.x, 0)
        }

        function test_pinch_zooms() {
            const y = box.height / 2, cx = box.width / 2
            const touch = touchEvent(box)
            touch.press(0, box, cx - 40, y).press(1, box, cx + 40, y).commit()
            for (let d = 40; d <= 160; d += 20) {
                touch.move(0, box, cx - d, y).move(1, box, cx + d, y).commit()
                wait(10)
            }
            touch.release(0, box, cx - 160, y).release(1, box, cx + 160, y).commit()
            verify(box.zoom > 2, "zoom after spreading two fingers: " + box.zoom)
        }
    }
}
