// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic as C
import Vela.Controls

// The Windows 11 drop-down (ComboBox). `model` is a list of strings or of
// objects with `text`; `currentIndex` the chosen one.
C.ComboBox {
    id: combo
    property bool usable: true
    signal chosen(int index)

    enabled: usable
    implicitWidth: Math.max(160, Math.min(320, contentWidth + 56))
    implicitHeight: 32
    textRole: model && model.length > 0 && typeof model[0] === "object" ? "text" : ""
    font.pixelSize: Theme.fontBody
    onActivated: index => combo.chosen(index)

    property real contentWidth: {
        let widest = 0
        const items = model || []
        for (let i = 0; i < items.length; ++i) {
            widest = Math.max(widest, metrics.advanceWidth(typeof items[i] === "object" ? items[i].text : items[i]))
        }
        return widest
    }
    FontMetrics { id: metrics; font.pixelSize: Theme.fontBody }

    background: Rectangle {
        radius: Theme.radius
        color: combo.pressed ? Theme.controlPressed : combo.hovered ? Theme.controlHover : Theme.control
        border.width: 1
        border.color: Theme.controlStroke
        opacity: combo.enabled ? 1 : 0.45
    }
    contentItem: Text {
        leftPadding: 11
        rightPadding: 32
        text: combo.displayText
        color: Theme.text
        font.pixelSize: Theme.fontBody
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    // Fluent-Qt ComboBox.cpp: on a press the chevron dips 3 px and fades to
    // half, then comes back (400 ms OutCubic).
    indicator: Text {
        id: chevron
        property real press: 0
        x: combo.width - width - 12
        y: (combo.height - height) / 2 + 3 * Math.sin(Math.PI * press)
        opacity: 1 - 0.5 * Math.sin(Math.PI * press)
        text: "⌄"
        color: Theme.textSecondary
        font.pixelSize: 14
        topPadding: -6
        NumberAnimation on press {
            id: chevronDip
            running: false
            from: 0
            to: 1
            duration: Theme.slow
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.decelerate
        }
    }
    onPressedChanged: if (pressed) chevronDip.restart()
    delegate: C.ItemDelegate {
        id: entry
        required property int index
        required property var modelData
        // Fluent-Qt: 40 px rows, the background inset 5 / 3, the pill 4 px in.
        width: combo.popup.width - 4
        x: 2
        height: 40
        highlighted: combo.highlightedIndex === index
        background: Item {
            Rectangle {
                anchors { fill: parent; leftMargin: 5; rightMargin: 5; topMargin: 3; bottomMargin: 3 }
                radius: Theme.radius
                color: entry.pressed ? Theme.subtlePressed : entry.highlighted || entry.hovered ? Theme.subtleHover : "transparent"
            }
            Rectangle {
                visible: combo.currentIndex === entry.index
                x: 9
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: 16
                radius: 1.5
                color: Theme.accentFill
            }
        }
        contentItem: Text {
            leftPadding: 14
            text: typeof entry.modelData === "object" ? entry.modelData.text : entry.modelData
            color: Theme.text
            font.pixelSize: Theme.fontBody
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    popup: C.Popup {
        y: combo.height + 8
        width: Math.max(combo.width, 160)
        implicitHeight: Math.min(contentItem.implicitHeight + 4, 6 * 40 + 4) // at most 6 rows
        padding: 2
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: combo.popup.visible ? combo.delegateModel : null
            currentIndex: combo.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            C.ScrollBar.vertical: C.ScrollBar { policy: C.ScrollBar.AsNeeded }
        }
        background: Rectangle {
            color: Theme.flyout
            radius: Theme.radiusOverlay
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.4)
        }
        // no enter animation: Fluent-Qt's ComboBox popup opens at once
    }
}
