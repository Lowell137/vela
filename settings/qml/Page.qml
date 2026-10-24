// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic as C
import Vela.Controls

// A page's content: it scrolls, at most 1000 px wide as on Windows (a page
// with a grid, such as Icons, takes the whole width). Children go in a column.
Flickable {
    id: page
    default property alias content: column.data
    property alias spacing: column.spacing
    property real maxContentWidth: 1000
    contentWidth: width
    contentHeight: column.height + 40
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    C.ScrollBar.vertical: C.ScrollBar {
        policy: page.contentHeight > page.height ? C.ScrollBar.AsNeeded : C.ScrollBar.AlwaysOff
    }

    Column {
        id: column
        x: 0
        width: Math.min(page.width - 24, page.maxContentWidth)
        spacing: 4
    }
}
