// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// A titled panel of a page: Windows 11's card.
Rectangle {
    id: card
    property string title
    default property alias content: body.data

    implicitHeight: column.implicitHeight + 28
    radius: Theme.radiusCard
    color: Theme.card
    border.width: 1
    border.color: Theme.cardStroke

    Column {
        id: column
        x: 16
        y: 14
        width: parent.width - 32
        spacing: 10
        Text {
            visible: card.title !== ""
            text: card.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.weight: Font.DemiBold
        }
        Item {
            id: body
            width: parent.width
            height: childrenRect.height
        }
    }
}
