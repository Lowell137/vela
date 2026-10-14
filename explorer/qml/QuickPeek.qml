// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Quick preview (Space): a card over the folder, about 70% of the window, for
// the chosen item. Space or Esc closes it; the arrows keep moving the
// selection in the view and the card follows. Images, text (the first 64 KB),
// the first page of a PDF, folders and everything else by icon and properties.
Item {
    id: peek
    property var tab
    property string path: ""
    property var result: null // answer of Ops.peek() for `path`
    readonly property string kind: result ? result.kind : ""

    anchors.fill: parent
    visible: false
    z: 100

    function show(p) { path = p; visible = true }
    function hide() { visible = false; result = null }
    onPathChanged: { result = null; if (path !== "") Ops.peek(path) }

    Connections {
        target: Ops
        function onPeekReady(p, r) { if (p === peek.path) peek.result = r }
    }

    function fileUrl(p) { return "file://" + encodeURI(p).replace(/#/g, "%23").replace(/\?/g, "%3F") }

    Rectangle { anchors.fill: parent; color: Theme.smoke
        MouseArea { anchors.fill: parent; onClicked: peek.hide() } }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.7)
        height: Math.round(parent.height * 0.7)
        radius: Theme.radiusOverlay
        color: Theme.dialog
        border.width: 1
        border.color: Theme.dialogStroke
        clip: true
        MouseArea { anchors.fill: parent } // clicks on the card do not close it

        Text {
            id: title
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            text: peek.result ? peek.result.name : ""
            color: Theme.text
            font.pixelSize: Theme.fontBody + 2
            font.weight: Font.DemiBold
            elide: Text.ElideMiddle
        }
        Item {
            id: body
            anchors { left: parent.left; right: parent.right; top: title.bottom; bottom: meta.top; margins: 12 }

            Image {
                anchors.fill: parent
                visible: peek.kind === "image"
                source: visible ? peek.fileUrl(peek.path) : ""
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                autoTransform: true
                smooth: true
            }
            Flickable {
                anchors.fill: parent
                visible: peek.kind === "text"
                contentHeight: textView.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Text {
                    id: textView
                    width: parent.width
                    text: peek.result && peek.result.text !== undefined ? peek.result.text : ""
                    textFormat: Text.PlainText
                    color: Theme.text
                    font.family: "monospace"
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WrapAnywhere
                }
            }
            Loader {
                anchors.fill: parent
                active: peek.kind === "pdf"
                source: active ? "PeekPdf.qml" : ""
                onLoaded: item.path = Qt.binding(() => peek.path)
            }
            // Folder, other types, or a PDF without QtPdf: the large icon.
            Column {
                anchors.centerIn: parent
                visible: peek.kind === "folder" || peek.kind === "other"
                spacing: 12
                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 128
                    height: 128
                    source: peek.result ? "image://fileicon/" + encodeURIComponent(peek.result.icon) : ""
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: peek.kind === "folder" && !!peek.result
                    text: peek.result ? peek.result.items + qsTr(" items, ") + (peek.result.sizeText || "") : ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
            }
        }
        Text {
            id: meta
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
            text: {
                const r = peek.result
                if (!r) return ""
                const parts = [r.type]
                if (r.kind !== "folder" && r.sizeText) parts.push(r.sizeText)
                if (r.width) parts.push(r.width + qsTr(" x ") + r.height)
                if (r.modified) parts.push(Qt.formatDateTime(r.modified, "dd/MM/yyyy HH:mm"))
                if (r.truncated) parts.push(qsTr("first 64 KB"))
                return parts.join("  ·  ")
            }
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
    }
}
