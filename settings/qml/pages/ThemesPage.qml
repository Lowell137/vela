// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtCore
import Vela.Controls

// Personalization > Themes: a theme file (theme= in vela.conf) that changes
// the colors and corner radii of the shell and the apps. Your own go in
// ~/.config/vela/themes/<name>.conf. Below it the surfaces' own styles, like
// Windhawk's three stylers: themes/taskbar/, startmenu/, notifications/.
Page {
    id: page
    readonly property var choices: [{ name: "", text: qsTr("Vela (default)") }].concat(Prefs.visualThemes())
    readonly property int chosenIndex: {
        for (let i = 0; i < choices.length; ++i) {
            if (choices[i].name === Prefs.visualTheme) return i
        }
        return 0 // not installed (any more): Vela's own
    }

    CardGroup {
        Card {
            icon: "preferences-desktop-theme"
            title: qsTr("Theme")
            description: Prefs.visualTheme !== "" && page.chosenIndex === 0
                ? qsTr("\"%1\" isn't installed: Vela's own look is used").arg(Prefs.visualTheme)
                : qsTr("Colors and corners of the shell and the apps. Your own themes: ~/.config/vela/themes/<name>.conf")
            trailing: Choice {
                model: page.choices
                currentIndex: page.chosenIndex
                onChosen: index => Prefs.visualTheme = page.choices[index].name
            }
        }
    }

    // start-icon= of vela.conf: an icon of the theme or a file of the user's.
    CardGroup {
        title: qsTr("Start button")
        Card {
            title: qsTr("Icon")
            description: Prefs.startIcon === "" ? qsTr("Vela's symbol") : Prefs.startIcon
            trailing: Row {
                spacing: 8
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    visible: Prefs.startIcon !== ""
                    source: Prefs.startIcon === "" ? ""
                        : Prefs.startIcon.startsWith("/") ? "file://" + encodeURI(Prefs.startIcon)
                        : Theme.icons + encodeURIComponent(Prefs.startIcon)
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                }
                Button {
                    text: qsTr("Choose a file")
                    onClicked: startIconDialog.open()
                }
                Button {
                    text: qsTr("Reset")
                    usable: Prefs.startIcon !== ""
                    onClicked: Prefs.startIcon = ""
                }
            }
        }
        Card {
            title: qsTr("Size")
            description: qsTr("%1 px").arg(Prefs.startIconSize)
            trailing: Slider {
                width: 220
                from: 16
                to: 40
                stepSize: 1
                live: false
                value: Prefs.startIconSize
                onReleased: value => Prefs.startIconSize = Math.round(value)
            }
        }
        Card {
            title: qsTr("From the icon theme")
            description: qsTr("Search the current icon theme")
            contentItem: Column {
                readonly property real avail: parent ? parent.width : 0
                width: avail
                spacing: 12
                TextBox {
                    id: startIconSearch
                    width: Math.min(320, parent.width)
                    placeholderText: qsTr("Search icons")
                }
                GridView {
                    width: parent.width
                    height: 320
                    clip: true
                    cellWidth: 48
                    cellHeight: 48
                    boundsBehavior: Flickable.StopAtBounds
                    QQC.ScrollBar.vertical: QQC.ScrollBar {}
                    model: { Prefs.iconTheme; return Prefs.startIconNames(startIconSearch.text) }
                    delegate: Rectangle {
                        required property string modelData
                        width: 44
                        height: 44
                        radius: Theme.radius
                        color: modelData === Prefs.startIcon ? Theme.accent
                            : tileMouse.containsMouse ? Theme.subtleHover : "transparent"
                        Image {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            source: Theme.icons + encodeURIComponent(parent.modelData)
                            sourceSize: Qt.size(width, height)
                            asynchronous: true
                        }
                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: Prefs.startIcon = parent.modelData
                        }
                    }
                }
            }
        }
    }
    FileDialog {
        id: startIconDialog
        title: qsTr("Choose an icon")
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        nameFilters: [qsTr("Images (*.svg *.png *.webp *.jpg *.jpeg)")]
        onAccepted: Prefs.startIcon = selectedFile.toString()
    }

    // The surfaces' own styles (themes/taskbar/, startmenu/, notifications/),
    // one surface at a time with their screenshots and a button to use each.
    property string surface: "taskbar"
    readonly property var surfaces: [
        { surface: "taskbar", text: qsTr("Taskbar") },
        { surface: "startmenu", text: qsTr("Start menu") },
        { surface: "notifications", text: qsTr("Quick settings") }
    ]
    // Re-read when the surface changes or a style is chosen.
    readonly property var gallery: { Prefs.surfaceStyles; return Prefs.themeGallery(surface) }
    readonly property string chosenStyle: Prefs.surfaceStyles[surface] || ""

    Row {
        spacing: 8
        Repeater {
            model: page.surfaces
            Button {
                required property var modelData
                text: modelData.text
                accent: page.surface === modelData.surface
                onClicked: page.surface = modelData.surface
            }
        }
    }

    CardGroup {
        title: qsTr("Styles")
        Card {
            title: qsTr("From the theme")
            description: qsTr("Your own styles: ~/.config/vela/themes/%1/<name>.conf").arg(page.surface)
            trailing: Button {
                text: page.chosenStyle === "" ? qsTr("In use") : qsTr("Use")
                accent: page.chosenStyle !== ""
                usable: page.chosenStyle !== ""
                onClicked: Prefs.setSurfaceStyle(page.surface, "")
            }
        }
        Repeater {
            model: page.gallery
            Card {
                id: styleCard
                required property var modelData
                title: modelData.text
                description: page.chosenStyle === modelData.name ? qsTr("In use") : ""
                trailing: Button {
                    text: page.chosenStyle === styleCard.modelData.name ? qsTr("In use") : qsTr("Use")
                    accent: page.chosenStyle !== styleCard.modelData.name
                    usable: page.chosenStyle !== styleCard.modelData.name
                    onClicked: Prefs.setSurfaceStyle(page.surface, styleCard.modelData.name)
                }
                contentItem: Row {
                    width: parent.width
                    spacing: 12
                    // The shots side by side (a taskbar's one is wide, quick settings' two are tall).
                    readonly property real each: (width - spacing * (Math.max(count, 1) - 1)) / Math.max(count, 1)
                    readonly property int count: styleCard.modelData.images.length
                    Repeater {
                        model: styleCard.modelData.images
                        Image {
                            required property string modelData
                            width: parent.each
                            height: implicitWidth > 0 ? Math.min(width * implicitHeight / implicitWidth, 340) : 0
                            fillMode: Image.PreserveAspectFit
                            source: modelData
                            asynchronous: true
                            smooth: true
                        }
                    }
                }
            }
        }
    }

    CardGroup {
        title: qsTr("Effects")
        Card {
            icon: "preferences-desktop-panel"
            title: qsTr("Blur behind the taskbar")
            description: qsTr("A floating taskbar: the strip around it blurred while a window is maximized")
            trailing: Switch {
                checked: Prefs.taskbarBackdrop
                onToggled: on => Prefs.taskbarBackdrop = on
            }
        }
    }

    CardGroup {
        Card {
            icon: "edit-undo"
            title: qsTr("Reset")
            description: qsTr("Vela's own theme and styles, the default accent and the switches above. Light or dark stays")
            trailing: Button {
                text: qsTr("Reset")
                onClicked: Prefs.resetAppearance()
            }
        }
    }
}
