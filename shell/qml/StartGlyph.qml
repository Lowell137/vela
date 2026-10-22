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

    // taskbar-start-glyph=1: Windows 7's orb, drawn: a glass ball and the flag.
    Item {
        visible: Theme.taskbarStartGlyph === 1
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

    Shape {
        visible: Theme.taskbarStartGlyph !== 1
        anchors.fill: parent
        // Soft antialiasing on the slanted edges too.
        layer.enabled: true
        layer.samples: 4

        // Big sail
        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: LinearGradient {
                x1: 12; y1: 1
                x2: 21; y2: 16
                GradientStop { position: 0; color: Theme.accentFill }
                GradientStop { position: 1; color: Theme.accent }
            }
            startX: 12; startY: 1
            PathLine { x: 21; y: 16 }
            PathLine { x: 12; y: 16 }
            PathLine { x: 12; y: 1 }
        }

        // Small sail
        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: Theme.accentFill
            startX: 10; startY: 5
            PathLine { x: 10; y: 16 }
            PathLine { x: 3; y: 16 }
            PathLine { x: 10; y: 5 }
        }

        // Hull
        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillColor: Theme.accent
            startX: 1; startY: 18
            PathLine { x: 21; y: 18 }
            PathLine { x: 17.5; y: 21.5 }
            PathLine { x: 4.5; y: 21.5 }
            PathLine { x: 1; y: 18 }
        }
    }
}
