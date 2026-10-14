// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Multitasking: how windows are arranged (snap layouts, the Task
// View hot corner, rules for where an app's windows open).
Page {
    CardGroup {
        title: qsTr("Related settings")
        LinkCard {
            icon: "window"
            title: qsTr("Window rules")
            description: qsTr("Where and how an app's windows open")
            onClicked: root.navigate("window-rules")
        }
    }
}
