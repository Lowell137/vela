// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import QtCore
import Vela.Controls

// Personalization > Background: recent images, "Browse photos", a folder of
// the user's own pictures and the wallpapers installed on the system.
Page {
    id: page

    function urlFor(path) {
        return path.startsWith(":") ? "qrc" + path : "file://" + path
    }

    PersonalizationPage.DesktopPreview {}

    component Thumb: Rectangle {
        id: thumb
        required property string path
        width: 120
        height: 80
        radius: Theme.radius
        color: "#101010"
        border.width: Prefs.wallpaper === path ? 2 : 0
        border.color: Theme.accentFill
        Image {
            anchors { fill: parent; margins: Prefs.wallpaper === thumb.path ? 3 : 0 }
            source: page.urlFor(thumb.path)
            sourceSize: Qt.size(240, 160)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: status === Image.Ready ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.fast } }
        }
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "white"
            opacity: thumbMouse.containsMouse ? 0.08 : 0
        }
        MouseArea {
            id: thumbMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: Prefs.setWallpaper(thumb.path)
        }
    }

    CardGroup {
        Card {
            icon: "preferences-desktop-wallpaper"
            title: qsTr("Personalize your background")
            description: qsTr("One picture fills every screen")
            trailing: Choice { model: [qsTr("Picture")]; currentIndex: 0 }
            contentItem: Column {
                width: parent.width
                spacing: 8
                Text {
                    text: qsTr("Recent images")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    leftPadding: 36
                }
                Flow {
                    x: 36
                    width: parent.width - 36
                    spacing: 8
                    Repeater {
                        model: [Prefs.defaultWallpaper].concat(Prefs.recentWallpapers)
                        delegate: Thumb { required property string modelData; path: modelData }
                    }
                }
            }
        }
        Card {
            icon: "folder-pictures"
            title: qsTr("Choose a photo")
            trailing: Button {
                text: qsTr("Browse photos")
                onClicked: {
                    // close() first: if the portal never answered (its
                    // backend died), open() alone would do nothing forever.
                    fileDialog.close()
                    fileDialog.open()
                }
            }
        }
        Card {
            icon: "folder"
            title: qsTr("Pictures from a folder")
            description: Prefs.wallpaperFolder || qsTr("Show the pictures of a folder here")
            trailing: Row {
                spacing: 8
                Choice {
                    readonly property var sorts: ["name", "size", "newest", "oldest"]
                    visible: Prefs.wallpaperFolder !== ""
                    model: [qsTr("Name"), qsTr("Size"), qsTr("Newest"), qsTr("Oldest")]
                    currentIndex: Math.max(0, sorts.indexOf(Prefs.wallpaperFolderSort))
                    onChosen: index => Prefs.wallpaperFolderSort = sorts[index]
                }
                Button {
                    text: qsTr("Remove")
                    visible: Prefs.wallpaperFolder !== ""
                    onClicked: Prefs.setWallpaperFolder("")
                }
                Button {
                    text: qsTr("Choose folder")
                    onClicked: {
                        folderDialog.close()
                        folderDialog.open()
                    }
                }
            }
        }
    }

    CardGroup {
        title: Prefs.wallpaperFolder.split("/").pop()
        visible: folder.count > 0
        Card {
            minimumHeight: 0
            contentItem: Flow {
                width: parent.width
                spacing: 8
                Repeater {
                    id: folder
                    model: Prefs.folderWallpapers
                    delegate: Thumb { required property string modelData; path: modelData }
                }
            }
        }
    }

    CardGroup {
        title: qsTr("System backgrounds")
        visible: system.count > 0
        Card {
            minimumHeight: 0
            contentItem: Flow {
                width: parent.width
                spacing: 8
                Repeater {
                    id: system
                    model: Prefs.systemWallpapers
                    delegate: Thumb { required property string modelData; path: modelData }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose a photo")
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp *.gif *.svg *.avif *.jxl)")]
        onAccepted: Prefs.setWallpaper(selectedFile.toString())
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder")
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: Prefs.setWallpaperFolder(selectedFolder.toString())
    }
}
