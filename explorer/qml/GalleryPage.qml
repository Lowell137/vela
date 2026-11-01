// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Files.Backend
import Vela.Controls

// File Explorer's Gallery: the newest pictures under Pictures, month by month.
// The search runs in the background and only the rows on screen exist, so a
// large collection doesn't stop the window.
ListView {
    id: page
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    cacheBuffer: 240
    leftMargin: 24
    topMargin: 16
    bottomMargin: 24

    readonly property int tileWidth: 160
    readonly property int tileHeight: 120
    property var images: []
    property bool loading: true
    readonly property int perRow: Math.max(1, Math.floor((width - 48) / (tileWidth + 4)))
    // [{month ("" if it goes on), images}]
    model: {
        const rows = []
        let month = ""
        for (const image of images) {
            const last = rows[rows.length - 1]
            if (image.month !== month || last.images.length === perRow) {
                rows.push({ month: image.month !== month ? image.month : "", images: [] })
                month = image.month
            }
            rows[rows.length - 1].images.push(image.path)
        }
        return rows
    }

    Component.onCompleted: Ops.loadGallery()
    Connections {
        target: Ops
        function onGalleryReady(found) {
            page.images = found
            page.loading = false
        }
    }

    Text {
        visible: page.count === 0
        parent: page
        anchors.horizontalCenter: parent.horizontalCenter
        y: 24
        text: page.loading ? qsTr("Working on it...") : qsTr("No pictures here yet.")
        color: Theme.text
        font.pixelSize: Theme.fontCaption
    }

    delegate: Column {
        id: row
        required property var modelData
        spacing: 4
        bottomPadding: 4
        Text {
            visible: row.modelData.month !== ""
            topPadding: 8
            bottomPadding: 4
            text: row.modelData.month
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
        }
        Row {
            spacing: 4
            Repeater {
                model: row.modelData.images
                delegate: Rectangle {
                    id: tile
                    required property string modelData
                    width: page.tileWidth
                    height: page.tileHeight
                    color: mouse.containsMouse ? Theme.filesItemHover : "transparent"
                    Image {
                        anchors { fill: parent; margins: 2 }
                        source: "file://" + tile.modelData.split("/").map(encodeURIComponent).join("/")
                        sourceSize: Qt.size(page.tileWidth, page.tileHeight)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        clip: true
                    }
                    MouseArea {
                        id: mouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: Ops.open([tile.modelData])
                    }
                }
            }
        }
    }
}
