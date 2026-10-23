// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import Vela.Controls

// A panel's soft shadow, drawn only *outside* the panel: acrylic panels are
// semi-transparent and must show the blurred background below, not a shadow.
// RectangularShadow (analytic, like the compositor's) with the panel's rounded
// shape cut out of it, so the corners get their shadow too, antialiased. The
// cut is a pixel wider than the panel: the compositor's blur follows the
// panel's alpha, and there the shadow's would square its corners.
Item {
    id: root

    required property Item target
    property real radius: Theme.radiusOverlay
    property real blur: 32
    property vector2d offset: Qt.vector2d(0, 6)
    property color color: Qt.rgba(0, 0, 0, 0.45)
    readonly property real reach: blur + Math.max(Math.abs(offset.x), Math.abs(offset.y)) + 8

    anchors.fill: target
    anchors.margins: -reach

    Item {
        id: cast
        anchors.fill: parent
        visible: false
        layer.enabled: true
        RectangularShadow {
            x: root.reach
            y: root.reach
            width: root.target.width
            height: root.target.height
            radius: root.radius
            blur: root.blur
            offset: root.offset
            color: root.color
        }
    }
    Item {
        id: hole
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            x: root.reach - 1
            y: root.reach - 1
            width: root.target.width + 2
            height: root.target.height + 2
            radius: root.radius + 1
            antialiasing: true
        }
    }
    MultiEffect {
        anchors.fill: parent
        source: cast
        maskEnabled: true
        maskSource: hole
        maskInverted: true
        maskThresholdMin: 0.0
        maskSpreadAtMin: 1.0
    }
}
