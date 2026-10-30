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
            trailing: Switch {
                checked: Prefs.taskView
                onToggled: Prefs.taskView = checked
            }
        }
        Card {
            icon: "preferences-desktop-screensaver"
            title: qsTr("Caffeine")
            description: qsTr("The icon next to the clock that keeps the screen awake")
            trailing: Choice {
                readonly property var modes: ["on", "always", "never"]
                model: [qsTr("Only while on"), qsTr("Always"), qsTr("Never")]
                currentIndex: Math.max(0, modes.indexOf(Prefs.taskbarCaffeine))
                onChosen: index => Prefs.taskbarCaffeine = modes[index]
            }
        }
    }

    CardGroup {
        title: qsTr("Other system tray icons")
        Card {
            icon: "go-up"
            title: qsTr("Hidden icon menu")
            description: qsTr("Extra icons wait behind an arrow, to keep the taskbar tidy")
            trailing: Switch {
                checked: Prefs.trayOverflow
                onToggled: on => Prefs.trayOverflow = on
            }
        }
    }

    CardGroup {
        title: qsTr("Weather")
        Card {
            icon: "weather-few-clouds"
            title: qsTr("Show the weather on the taskbar")
            description: qsTr("Asks open-meteo.com for the forecast of the city you choose below, every 30 minutes. Off, nothing is requested")
            trailing: Switch {
                checked: Prefs.weather
                onToggled: on => Prefs.weather = on
            }
        }
        Card {
            icon: "mark-location"
            title: qsTr("City")
            description: Prefs.weatherCity !== "" ? Prefs.weatherCity : qsTr("No city chosen: the place is guessed from your IP address")
            trailing: TextBox {
                id: cityField
                width: 220
                placeholderText: qsTr("Search for a city")
                onTextChanged: searchDelay.restart()
                Timer {
                    id: searchDelay
                    interval: 500
                    onTriggered: Places.search(cityField.text)
                }
            }
        }
        Card {
            visible: cityField.text.trim().length >= 2 && Places.failed
            icon: "dialog-warning"
            title: qsTr("Couldn't search")
            description: qsTr("Check the network connection and try again")
        }
        Card {
            visible: cityField.text.trim().length >= 2 && !Places.busy && !Places.failed && Places.results.length === 0
            icon: "edit-find"
            title: qsTr("No cities found")
        }
        Repeater {
            model: cityField.text.trim().length >= 2 ? Places.results : []
            delegate: Card {
                required property var modelData
                icon: "mark-location"
                title: modelData.name
                description: modelData.detail
                clickable: true
                onClicked: {
                    Prefs.setWeatherPlace(modelData.name + (modelData.detail !== "" ? ", " + modelData.detail.split(", ").pop() : ""),
                        modelData.lat, modelData.lon)
                    cityField.text = ""
                }
            }
        }
        Card {
            icon: "utilities-system-monitor"
            title: qsTr("Show system info in the weather panel")
            description: qsTr("The graphics card, processor, memory and disk cards beside the forecast")
            trailing: Switch {
                checked: Prefs.weatherSystemInfo
                onToggled: on => Prefs.weatherSystemInfo = on
            }
        }
        Card {
            icon: "weather-clear"
            title: qsTr("Temperature unit")
            trailing: Choice {
                model: [qsTr("Celsius (°C)"), qsTr("Fahrenheit (°F)")]
                currentIndex: Prefs.weatherUnit === "f" ? 1 : 0
                onChosen: index => Prefs.weatherUnit = index === 1 ? "f" : "c"
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
            icon: "view-grid"
            title: qsTr("Taskbar style")
            description: qsTr("Islands floats three separate surfaces, Dock is a macOS-like dock with a top panel; on the left and right they fall back to Classic")
            trailing: Choice {
                readonly property var styles: ["classic", "islands", "dock"]
                model: [qsTr("Classic"), qsTr("Islands"), qsTr("Dock and top panel")]
                currentIndex: Math.max(0, styles.indexOf(Prefs.taskbarStyle))
                onChosen: index => Prefs.taskbarStyle = styles[index]
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
            trailing: Switch {
                checked: Prefs.taskbarAllScreens
                onToggled: Prefs.taskbarAllScreens = checked
            }
        }
        Card {
            icon: "view-grid"
            title: qsTr("Show windows from all desktops")
            description: qsTr("The taskbar lists the open apps of every desktop, not only the current one")
            trailing: Switch {
                checked: Prefs.taskbarAllDesktops
                onToggled: on => Prefs.taskbarAllDesktops = on
            }
        }
        Card {
            icon: "view-fullscreen"
            title: qsTr("Automatically hide the taskbar")
            description: qsTr("Hides until you move the pointer to the screen edge")
            trailing: Switch {
                checked: Prefs.taskbarAutoHide
                onToggled: on => Prefs.taskbarAutoHide = on
            }
        }
        Card {
            icon: "view-list-icons"
            title: qsTr("Combine taskbar buttons")
            description: qsTr("Group an app's windows under one button, or keep one per window")
            trailing: Choice {
                model: [qsTr("Always"), qsTr("Never")]
                currentIndex: Prefs.taskbarCombine === "never" ? 1 : 0
                onChosen: index => Prefs.taskbarCombine = index === 1 ? "never" : "always"
            }
        }
        Card {
            icon: "user-desktop"
            title: qsTr("Show desktop from taskbar corner")
            description: qsTr("Click the far-right edge of any taskbar to show or restore your windows (also with Super+D)")
            trailing: Switch {
                checked: Prefs.showDesktopCorner
                onToggled: Prefs.showDesktopCorner = checked
            }
        }
        Card {
            icon: "edit-opacity"
            title: qsTr("Start menu background")
            description: qsTr("More solid than the theme: %1%. 0 keeps the theme's own look").arg(Prefs.startSolid)
            trailing: Slider {
                width: 220
                from: 0
                to: 100
                stepSize: 1
                live: false
                value: Prefs.startSolid
                onReleased: value => Prefs.startSolid = Math.round(value)
            }
        }
        Card {
            icon: "process-stop"
            title: qsTr("End task")
            description: qsTr("In the taskbar buttons' menu, to close an app that isn't responding right away")
            trailing: Switch {
                checked: Prefs.endTask
                onToggled: Prefs.endTask = checked
            }
        }
    }
}
