// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Vela.Controls

// Personalization > Window blur: the blur behind see-through windows, like
// Windows 11's acrylic (window-blur*= in vela.conf). Apps that ask for blur
// themselves always get it; this forces it on the others, for all windows or
// only the chosen apps, never the excluded ones; the same windows can also be
// drawn translucent (window-opacity=), so the blur shows through them. The
// compositor applies it right away.
Page {
    id: page

    CardGroup {
        Card {
            icon: "preferences-desktop-effects"
            title: qsTr("Window blur")
            description: qsTr("Blur what is behind see-through windows")
            trailing: Toggle {
                checked: Prefs.windowBlur
                onToggled: on => Prefs.windowBlur = on
            }
        }
        Card {
            icon: "preferences-desktop-effects"
            title: qsTr("Blur style")
            description: qsTr("Acrylic like Windows 11, or liquid glass: clearer, bending the light at the edges")
            trailing: Choice {
                model: [qsTr("Acrylic"), qsTr("Liquid Glass")]
                currentIndex: Prefs.blurStyle === "liquid" ? 1 : 0
                onChosen: index => Prefs.blurStyle = index === 1 ? "liquid" : "acrylic"
            }
        }
        Card {
            icon: "window"
            title: qsTr("Blur")
            description: Prefs.windowBlurAll
                ? qsTr("Every window, except the apps below")
                : qsTr("Apps that ask for it, and the apps you add below")
            trailing: Choice {
                usable: Prefs.windowBlur
                model: [qsTr("Only apps that ask for it"), qsTr("All windows")]
                currentIndex: Prefs.windowBlurAll ? 1 : 0
                onChosen: index => Prefs.windowBlurAll = index === 1
            }
        }
        Card {
            enabled: Prefs.windowBlur
            opacity: enabled ? 1 : 0.45
            icon: "edit-opacity"
            title: qsTr("Window opacity")
            description: qsTr("%1%").arg(Prefs.windowOpacity)
            trailing: Slider {
                width: 220
                from: 30
                to: 100
                stepSize: 1
                live: false
                value: Prefs.windowOpacity
                onReleased: value => Prefs.windowOpacity = Math.round(value)
            }
        }
    }

    AppList {
        visible: !Prefs.windowBlurAll
        title: qsTr("Also blur these apps")
        apps: Prefs.windowBlurInclude
        emptyText: qsTr("No apps added")
        onAdd: addDialog.show(false)
        onRemove: app => Prefs.windowBlurInclude = Prefs.windowBlurInclude.filter(a => a !== app)
    }

    AppList {
        title: qsTr("Never blur these apps")
        apps: Prefs.windowBlurExclude
        emptyText: qsTr("No apps excluded")
        onAdd: addDialog.show(true)
        onRemove: app => Prefs.windowBlurExclude = Prefs.windowBlurExclude.filter(a => a !== app)
    }

    CardGroup {
        Card {
            icon: "dialog-information"
            title: qsTr("Where blur shows")
            description: qsTr("Only behind the see-through parts of a window, never behind its shadow. Opaque and fullscreen windows aren't blurred, unless window opacity makes them see-through; fullscreen windows always stay opaque. Apps that ask for blur themselves keep it.")
        }
    }

    // A list of app ids with "Add an app" and "Remove".
    component AppList: CardGroup {
        id: list
        property var apps: []
        property string emptyText
        signal add()
        signal remove(string app)
        Card {
            icon: "list-add"
            title: list.apps.length === 0 ? list.emptyText : list.apps.length + (list.apps.length === 1 ? qsTr(" app") : qsTr(" apps"))
            trailing: Button {
                text: qsTr("Add an app")
                usable: Prefs.windowBlur
                onClicked: list.add()
            }
        }
        Repeater {
            model: list.apps
            delegate: Card {
                id: appCard
                required property string modelData
                icon: Apps.iconForAppId(modelData)
                title: modelData
                trailing: Button {
                    subtle: true
                    text: qsTr("Remove")
                    onClicked: list.remove(appCard.modelData)
                }
            }
        }
    }

    Dialog {
        id: addDialog
        parent: root.contentItem
        property bool exclude: false
        property var windows: []
        title: exclude ? qsTr("Never blur an app") : qsTr("Blur an app")
        primaryText: qsTr("Add")
        primaryEnabled: appId.text.trim() !== ""
        dialogWidth: 520
        function show(excluded) {
            exclude = excluded
            windows = Prefs.openWindows()
            appId.text = ""
            open()
        }
        function addApp(app) {
            if (exclude) {
                Prefs.windowBlurExclude = Prefs.windowBlurExclude.concat([app])
            } else {
                Prefs.windowBlurInclude = Prefs.windowBlurInclude.concat([app])
            }
        }
        onAccepted: addApp(appId.text.trim())
        Column {
            width: parent.width
            spacing: 8
            Text {
                text: addDialog.windows.length === 0 ? qsTr("No open windows") : qsTr("Open windows")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            ListView {
                id: windowList
                width: parent.width
                height: Math.min(contentHeight, 240)
                clip: true
                model: addDialog.windows
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: windowEntry
                    required property var modelData
                    width: windowList.width
                    height: 40
                    radius: Theme.radius
                    color: windowMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Image {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20
                        sourceSize: Qt.size(width, height)
                        source: Theme.icons + encodeURIComponent(Apps.iconForAppId(windowEntry.modelData.appId))
                    }
                    Text {
                        x: 44
                        width: parent.width - 56
                        anchors.verticalCenter: parent.verticalCenter
                        text: windowEntry.modelData.title !== "" ? windowEntry.modelData.title + "   " + windowEntry.modelData.appId : windowEntry.modelData.appId
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        id: windowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            addDialog.addApp(windowEntry.modelData.appId)
                            addDialog.close()
                        }
                    }
                }
            }
            TextBox {
                id: appId
                width: parent.width
                placeholderText: qsTr("Or type an app id (e.g. org.kde.konsole)")
                onAccepted: if (addDialog.primaryEnabled) { addDialog.addApp(text.trim()); addDialog.close() }
            }
        }
    }
}
