// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// A file's tags as small dots (`tags`: "red,blue"). A tag that is not a color
// is drawn gray.
Row {
    id: dots
    property string tags
    property int dot: 10
    readonly property var colors: ({ red: "#e5484d", orange: "#f76b15", yellow: "#f5c518", green: "#30a46c",
        blue: "#3e8bff", purple: "#8e4ec6", gray: "#8b8d98" })
    spacing: -3
    visible: tags !== ""

    Repeater {
        model: dots.tags === "" ? [] : dots.tags.split(",")
        delegate: Rectangle {
            required property string modelData
            width: dots.dot
            height: dots.dot
            radius: dots.dot / 2
            color: dots.colors[modelData] ?? dots.colors.gray
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.45)
        }
    }
}
