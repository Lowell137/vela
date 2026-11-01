// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Files.Backend
import Vela.Controls

// File Explorer's Gallery: the newest pictures under Pictures, month by month.
Flickable {
    id: page
    clip: true
    contentHeight: column.height + 24
    boundsBehavior: Flickable.StopAtBounds

    readonly property var months: {
        const out = []
        for (const image of Ops.galleryImages()) {
            if (out.length === 0 || out[out.length - 1].month !== image.month) out.push({ month: image.month, images: [] })
            out[out.length - 1].images.push(image.path)
        }
        return out
    }

    Column {
        id: column
        x: 24
        y: 16
        width: page.width - 48
        spacing: 8
        Text {
            visible: page.months.length === 0
            text: qsTr("No pictures here yet.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Repeater {
            model: page.months
            delegate: Column {
                required property var modelData
                width: column.width
                spacing: 8
                Text {
                    text: parent.modelData.month
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.DemiBold
                }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: parent.parent.modelData.images
                        delegate: Rectangle {
                            id: tile
                            required property string modelData
                            width: 160
                            height: 120
                            color: mouse.containsMouse ? Theme.filesItemHover : "transparent"
                            Image {
                                anchors { fill: parent; margins: 2 }
                                source: "file://" + tile.modelData.split("/").map(encodeURIComponent).join("/")
                                sourceSize: Qt.size(160, 120)
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
    }
}
