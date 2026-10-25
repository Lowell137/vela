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
            description: qsTr("Extra icons wait behind an arrow, to keep the taskbar tidy")
            trailing: Toggle {
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
            trailing: Toggle {
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
            trailing: Toggle {
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
            description: qsTr("With more than one monitor, each gets its own taskbar")
            trailing: Toggle {
                checked: Prefs.taskbarAllScreens
                onToggled: on => Prefs.taskbarAllScreens = on
            }
        }
        Card {
            icon: "view-grid"
            title: qsTr("Show windows from all desktops")
            description: qsTr("The taskbar lists the open apps of every desktop, not only the current one")
            trailing: Toggle {
                checked: Prefs.taskbarAllDesktops
                onToggled: on => Prefs.taskbarAllDesktops = on
            }
        }
        Card {
            icon: "view-fullscreen"
            title: qsTr("Automatically hide the taskbar")
            description: qsTr("Hides until you move the pointer to the screen edge")
            trailing: Toggle {
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
            trailing: Toggle {
                checked: Prefs.showDesktopCorner
                onToggled: on => Prefs.showDesktopCorner = on
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
            description: qsTr("Adds End task to the right-click menu, for apps that stop responding")
            trailing: Toggle {
                checked: Prefs.endTask
                onToggled: on => Prefs.endTask = on
            }
        }
    }
}
