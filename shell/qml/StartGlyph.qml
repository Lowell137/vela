// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Vela.Controls

// Vela's symbol: a small stylized sail. It's the Start button.
Item {
    id: root
    width: 22
    height: 22

    // taskbar-orb: the theme's picture of the orb, over the drawn one.
    Image {
        anchors.centerIn: parent
        width: 54
        height: 54
        visible: source != "" && Theme.taskbarStartGlyph === 1
        source: Shell.startMenuOpen ? Theme.taskbarOrbPressed : Theme.taskbarOrb
        smooth: true
    }

    // taskbar-start-glyph=1: Windows 7's orb, drawn: a glass ball and the flag.
    Item {
        visible: Theme.taskbarStartGlyph === 1 && Theme.taskbarOrb === ""
        anchors.centerIn: parent
        width: 34
        height: 34
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            border.width: 1
            border.color: "#B3FFFFFF"
            gradient: Gradient {
                GradientStop { position: 0; color: "#5DB4E8" }
                GradientStop { position: 0.55; color: "#1A64A8" }
                GradientStop { position: 1; color: "#0A3566" }
            }
        }
        Rectangle { // the ball's gloss
            x: 4; y: 2
            width: 26; height: 14
            radius: 13
            gradient: Gradient {
                GradientStop { position: 0; color: "#B3FFFFFF" }
                GradientStop { position: 1; color: "#10FFFFFF" }
            }
        }
        Grid {
            anchors.centerIn: parent
            columns: 2
            spacing: 1
            rotation: -8
            Repeater {
                model: ["#F35325", "#81BC06", "#05A6F0", "#FFBA08"]
                Rectangle { required property string modelData; width: 8; height: 8; radius: 2; color: modelData }
            }
        }
    }

    // Nyxel's mark: a sparkle made of pixels. Arms and diagonals are squares
    // on a 5x5 grid; the center takes the accent, the diagonals fade.
    Item {
        visible: Theme.taskbarStartGlyph !== 1
        anchors.fill: parent
        Repeater {
            // [column, row, kind]: 0 arm, 1 center, 2 diagonal
            model: [
                [2, 0, 0], [2, 1, 0], [2, 3, 0], [2, 4, 0],
                [0, 2, 0], [1, 2, 0], [3, 2, 0], [4, 2, 0],
                [2, 2, 1],
                [1, 1, 2], [3, 1, 2], [1, 3, 2], [3, 3, 2]
            ]
            Rectangle {
                required property var modelData
                x: modelData[0] * 4.5
                y: modelData[1] * 4.5
                width: 4
                height: 4
                radius: 1
                color: modelData[2] === 1 ? Theme.accent : Theme.accentFill
                opacity: modelData[2] === 2 ? 0.6 : 1
            }
        }
    }
}
