// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// System > Clipboard, like Windows 11: clipboard history (Win+V), where its
// panel opens, how long the history is kept, and "Clear clipboard data"
// (except pinned items).
Page {
    CardGroup {
        Card {
            icon: "edit-paste"
            title: qsTr("Clipboard history")
            description: qsTr("Save multiple items to the clipboard to use later. Press Win+V to see and paste them")
            trailing: Toggle {
                checked: Prefs.clipboardHistory
                onToggled: on => Prefs.clipboardHistory = on
            }
        }
        Card {
            icon: "input-mouse"
            title: qsTr("Open next to the pointer")
            description: qsTr("The clipboard and emoji panel (Win+V, Win+.) opens where the mouse pointer is. Off: at the bottom right of the screen")
            trailing: Toggle {
                checked: Prefs.clipboardAtPointer
                onToggled: on => Prefs.clipboardAtPointer = on
            }
        }
    }
    CardGroup {
        title: qsTr("History")
        Card {
            icon: "document-save"
            title: qsTr("Keep the history after restarting")
            description: qsTr("What you copied is still there after you sign out or restart. Off: only pinned items are kept. Copied passwords are stored too")
            trailing: Toggle {
                checked: Prefs.clipboardKeep
                onToggled: on => Prefs.clipboardKeep = on
            }
        }
        Card {
            icon: "view-calendar"
            title: qsTr("Delete items after")
            description: qsTr("Older items leave the history by themselves; pinned items stay")
            trailing: Choice {
                model: [qsTr("Never"), qsTr("1 day"), qsTr("1 week"), qsTr("1 month")]
                currentIndex: Math.max(0, [0, 1, 7, 30].indexOf(Prefs.clipboardDays))
                onChosen: index => Prefs.clipboardDays = [0, 1, 7, 30][index]
            }
        }
        Card {
            icon: "view-list-details"
            title: qsTr("Number of items")
            description: qsTr("Past this many, the oldest item leaves when a new one is copied")
            trailing: Choice {
                model: ["25", "50", "100", "200"]
                currentIndex: Math.max(0, [25, 50, 100, 200].indexOf(Prefs.clipboardLimit))
                onChosen: index => Prefs.clipboardLimit = [25, 50, 100, 200][index]
            }
        }
        Card {
            icon: "edit-clear-history"
            title: qsTr("Clear clipboard data")
            description: qsTr("Everything except pinned items")
            trailing: Button {
                text: qsTr("Clear")
                onClicked: Prefs.clearClipboard()
            }
        }
    }
    CardGroup {
        title: qsTr("Related settings")
        Card {
            icon: "accessories-screenshot"
            title: qsTr("Snipping Tool")
            description: qsTr("Win+Shift+S or Print Screen: a rectangle, a window or the full screen, copied to the clipboard and saved in Pictures > Screenshot")
        }
    }
}
