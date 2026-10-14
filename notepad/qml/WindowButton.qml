// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Vela.Controls

// A title bar button of an app that draws its own bar (Notepad, Calculator):
// Windows 11's thin glyphs, drawn here because the icon theme's are heavier.
Rectangle {
    id: windowButton
    property string glyph // an SVG path in a 10×10 box
    property bool closes: false // red when hovered, like Windows
    signal clicked()
    width: 46
    height: 32
    color: buttonMouse.containsMouse ? (closes ? "#c42b1c" : buttonMouse.pressed ? Theme.subtlePressed : Theme.subtleHover) : "transparent"
    Shape {
        anchors.centerIn: parent
        width: 10
        height: 10
        preferredRendererType: Shape.CurveRenderer
        opacity: windowButton.Window.active ? 1 : 0.5
        ShapePath {
            strokeColor: windowButton.closes && buttonMouse.containsMouse ? "white" : Theme.text
            strokeWidth: 1
            fillColor: "transparent"
            PathSvg { path: windowButton.glyph }
        }
    }
    MouseArea {
        id: buttonMouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: windowButton.clicked()
    }
}
