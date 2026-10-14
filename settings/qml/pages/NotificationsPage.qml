// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Notifications.
Page {
    CardGroup {
        Card {
            icon: "preferences-desktop-notification"
            title: qsTr("Notifications")
            description: qsTr("Alerts from apps and the system, in the bottom-right corner and in the notification center (Win+N)")
        }
        Card {
            icon: "notifications-disabled"
            title: qsTr("Do not disturb")
            description: qsTr("Notifications go straight to the notification center, without popping up")
            trailing: Toggle {
                checked: Prefs.doNotDisturb
                onToggled: on => Prefs.doNotDisturb = on
            }
        }
    }

    CardGroup {
        title: qsTr("Do not disturb")
        Card {
            icon: "view-fullscreen"
            title: qsTr("Silence notifications during full-screen apps")
            description: qsTr("Notifications wait quietly while an app is full screen")
            trailing: Toggle {
                checked: Prefs.fullscreenDnd
                onToggled: on => Prefs.fullscreenDnd = on
            }
        }
        Card {
            icon: "notifications-disabled"
            enabled: Prefs.captureIndicator
            opacity: enabled ? 1 : 0.45
            title: qsTr("Silence notifications while sharing the screen")
            description: qsTr("Needs the sharing indicator, in Display settings")
            trailing: Toggle {
                checked: Prefs.captureDnd
                onToggled: on => Prefs.captureDnd = on
            }
        }
    }
}
