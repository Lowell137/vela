// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Taskbar. Snap layouts and the hot corner are in System >
// Multitasking, screen sharing in System > Display.
Page {
    PersonalizationPage.DesktopPreview {}

    CardGroup {
        title: qsTr("Taskbar items")
        Card {
            icon: "view-grid"
            title: qsTr("Task view")
            description: qsTr("Open windows and virtual desktops (also with Win+Tab)")
            trailing: Toggle {
                checked: Prefs.taskView
                onToggled: on => Prefs.taskView = on
            }
        }
    }

    CardGroup {
        title: qsTr("Other system tray icons")
        Card {
            icon: "go-up"
            title: qsTr("Hidden icon menu")
            description: qsTr("Apps' icons go behind the arrow next to the system icons")
            trailing: Toggle {
                checked: Prefs.trayOverflow
                onToggled: on => Prefs.trayOverflow = on
            }
        }
    }

    CardGroup {
        title: qsTr("Taskbar behaviors")
        Card {
            icon: "format-justify-center"
            title: qsTr("Taskbar alignment")
            trailing: Choice {
                model: [qsTr("Center"), qsTr("Left")]
                currentIndex: Prefs.taskbarAlignment === "left" ? 1 : 0
                onChosen: index => Prefs.taskbarAlignment = index === 1 ? "left" : "center"
            }
        }
        Card {
            icon: "go-up"
            title: qsTr("Taskbar position")
            trailing: Choice {
                readonly property var positions: ["bottom", "top", "left", "right"]
                model: [qsTr("Bottom"), qsTr("Top"), qsTr("Left"), qsTr("Right")]
                currentIndex: Math.max(0, positions.indexOf(Prefs.taskbarPosition))
                onChosen: index => Prefs.taskbarPosition = positions[index]
            }
        }
        Card {
            icon: "video-display"
            title: qsTr("Show the taskbar on all displays")
            description: qsTr("With multiple monitors: Start, apps and the clock on each one; notification area on the main one")
            trailing: Toggle {
                checked: Prefs.taskbarAllScreens
                onToggled: on => Prefs.taskbarAllScreens = on
            }
        }
        Card {
            icon: "view-fullscreen"
            title: qsTr("Automatically hide the taskbar")
            description: qsTr("It slides away, and comes back when the pointer reaches the edge of the screen")
            trailing: Toggle {
                checked: Prefs.taskbarAutoHide
                onToggled: on => Prefs.taskbarAutoHide = on
            }
        }
        Card {
            icon: "view-list-icons"
            title: qsTr("Combine taskbar buttons")
            description: qsTr("Windows of the same app under one button, or each with its own")
            trailing: Choice {
                model: [qsTr("Always"), qsTr("Never")]
                currentIndex: Prefs.taskbarCombine === "never" ? 1 : 0
                onChosen: index => Prefs.taskbarCombine = index === 1 ? "never" : "always"
            }
        }
        Card {
            icon: "process-stop"
            title: qsTr("End task")
            description: qsTr("In the taskbar buttons' menu, to close an app that isn't responding right away")
            trailing: Toggle {
                checked: Prefs.endTask
                onToggled: on => Prefs.endTask = on
            }
        }
    }
}
