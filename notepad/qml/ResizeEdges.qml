// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// Without Vela's bar a window has no resize borders either: its own edges
// resize it. Goes over everything else in the window.
Item {
    id: edges
    anchors.fill: parent
    z: 90

    Repeater {
        model: edges.Window.visibility === Window.Maximized ? [] : [
            Qt.TopEdge, Qt.BottomEdge, Qt.LeftEdge, Qt.RightEdge,
            Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge, Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge
        ]
        delegate: MouseArea {
            required property int modelData
            readonly property bool corner: (modelData & (Qt.TopEdge | Qt.BottomEdge)) && (modelData & (Qt.LeftEdge | Qt.RightEdge))
            readonly property int grip: corner ? 10 : 4
            z: corner ? 1 : 0
            x: modelData & Qt.RightEdge ? edges.width - grip : 0
            y: modelData & Qt.BottomEdge ? edges.height - grip : 0
            width: modelData & (Qt.LeftEdge | Qt.RightEdge) ? grip : edges.width
            height: modelData & (Qt.TopEdge | Qt.BottomEdge) ? grip : edges.height
            cursorShape: !corner ? (modelData & (Qt.LeftEdge | Qt.RightEdge) ? Qt.SizeHorCursor : Qt.SizeVerCursor)
                : modelData === (Qt.TopEdge | Qt.LeftEdge) || modelData === (Qt.BottomEdge | Qt.RightEdge) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
            onPressed: edges.Window.window.startSystemResize(modelData)
        }
    }
}
