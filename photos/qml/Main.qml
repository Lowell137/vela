// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// Photos' window, the viewer of Windows 11's: its own title bar, the commands,
// the image (which fits the window until it's zoomed), the strip of the
// folder's other images and a status bar. Nothing here changes a file except
// Delete, which moves it to the trash.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1000
    height: 700
    minimumWidth: 420
    minimumHeight: 320
    visible: true
    color: fullScreen ? "black" : Theme.background
    title: Album.name !== "" ? Album.name + qsTr(" - Photos") : qsTr("Photos")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    readonly property bool fullScreen: visibility === Window.FullScreen
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }
    function toggleFullScreen() { fullScreen ? showNormal() : showFullScreen() }

    readonly property bool hasImage: Album.index >= 0
    property bool infoOpen: false
    property bool editing: false
    // One image after the other, full screen, until Esc.
    property bool slideshow: false
    onFullScreenChanged: if (!fullScreen) slideshow = false
    Timer {
        interval: 4000
        repeat: true
        running: root.slideshow
        onTriggered: Album.index = (Album.index + 1) % Album.files.length
    }
    function startSlideshow() {
        if (!hasImage) return
        slideshow = true
        showFullScreen()
    }
    function edit() {
        if (hasImage && !Album.animated && !Album.scalable) editing = true
    }

    // --- how the image is shown; forgotten when another one is ---
    property int turn: 0 // degrees: the view's, the file isn't touched
    property real zoom: 0 // 0: fits the window
    readonly property bool turned: turn % 180 !== 0
    readonly property size natural: Album.dimensions // the file's, not what was loaded of it
    readonly property real naturalWidth: turned ? natural.height : natural.width
    readonly property real naturalHeight: turned ? natural.width : natural.height
    readonly property real fit: naturalWidth > 0 ? Math.min(1, view.width / naturalWidth, view.height / naturalHeight) : 1
    readonly property real factor: zoom > 0 ? zoom : fit
    // Zooms keeping the point (x, y) of the view where it is: the pointer, or
    // the middle.
    function zoomTo(value, x, y) {
        if (x === undefined) {
            x = view.width / 2
            y = view.height / 2
        }
        const old = factor
        const px = (view.contentX + x) / old
        const py = (view.contentY + y) / old
        zoom = Math.max(0.05, Math.min(8, value))
        view.contentX = Math.max(0, Math.min(view.contentWidth - view.width, px * factor - x))
        view.contentY = Math.max(0, Math.min(view.contentHeight - view.height, py * factor - y))
    }
    // An SVG is drawn again at the size it's shown, once the zooming rests.
    property size sharp: natural
    onFactorChanged: if (Album.scalable) sharpen.restart()
    Timer {
        id: sharpen
        interval: 150
        onTriggered: root.sharp = Qt.size(Math.ceil(root.natural.width * root.factor), Math.ceil(root.natural.height * root.factor))
    }
    Connections {
        target: Album
        function onIndexChanged() {
            root.turn = 0
            root.zoom = 0
            root.sharp = root.natural
            thumbs.positionViewAtIndex(Album.index, ListView.Contain)
        }
    }

    // --- files ---
    property var picker: null
    function open() {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, {
                parentWindow: root,
                nameFilters: [qsTr("Images (%1)").arg(Album.filter()), qsTr("All files (*)")]
            })
            if (!picker) {
                showError(component.errorString())
                return
            }
        }
        const folder = hasImage ? "file://" + Album.folder : StandardPaths.writableLocation(StandardPaths.PicturesLocation).toString()
        picker.pick(false, folder, "", files => { if (files.length > 0) Album.open(files[0]) })
    }
    function askDelete() {
        if (!hasImage) return
        deleteDialog.text = qsTr("Move %1 to the trash?").arg(Album.name)
        deleteDialog.open()
    }
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    Shortcut { sequence: "Ctrl+O"; enabled: !root.editing; onActivated: root.open() }
    Shortcut { sequence: "Left"; enabled: !root.editing; onActivated: Album.index-- }
    Shortcut { sequence: "Right"; enabled: !root.editing; onActivated: Album.index++ }
    Shortcut { sequence: "Home"; enabled: !root.editing; onActivated: Album.index = 0 }
    Shortcut { sequence: "End"; enabled: !root.editing; onActivated: Album.index = Album.files.length - 1 }
    Shortcut { sequence: "Ctrl+R"; enabled: !root.editing && root.hasImage; onActivated: root.turn = (root.turn + 90) % 360 }
    Shortcut { sequence: "Delete"; enabled: !root.editing && !deleteDialog.visible; onActivated: root.askDelete() }
    Shortcut { sequence: "Ctrl+C"; enabled: !root.editing; onActivated: Album.copy() }
    Shortcut { sequences: ["Ctrl+I", "Alt+Return"]; enabled: !root.editing; onActivated: root.infoOpen = !root.infoOpen }
    Shortcut { sequence: "Ctrl+E"; enabled: !root.editing; onActivated: root.edit() }
    Shortcut { sequence: "Ctrl+P"; enabled: !root.editing && root.hasImage; onActivated: Album.print() }
    Shortcut { sequence: "F5"; enabled: !root.editing; onActivated: root.startSlideshow() }
    Shortcut { sequence: "F11"; enabled: !root.editing; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Escape"; enabled: !root.editing && root.fullScreen; onActivated: root.showNormal() }
    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; enabled: !root.editing; onActivated: root.zoomTo(root.factor * 1.25) }
    Shortcut { sequence: "Ctrl+-"; enabled: !root.editing; onActivated: root.zoomTo(root.factor / 1.25) }
    Shortcut { sequence: "Ctrl+0"; enabled: !root.editing; onActivated: root.zoom = 0 }
    Shortcut { sequence: "Ctrl+1"; enabled: !root.editing; onActivated: root.zoomTo(1) }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.fullScreen ? 0 : 32
        visible: !root.fullScreen

        DragHandler {
            target: null
            onActiveChanged: if (active) root.startSystemMove()
        }
        TapHandler {
            onDoubleTapped: root.toggleMaximized()
        }

        Image {
            x: 14
            y: 8
            width: 16
            height: 16
            source: "image://appicon/image-viewer"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 42
            width: parent.width - x - 3 * 46 - 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideMiddle
        }
        Row {
            anchors { right: parent.right; top: parent.top }
            WindowButton { glyph: "M 0 5.5 H 10"; onClicked: root.showMinimized() }
            WindowButton {
                glyph: root.maximized ? "M 0.5 2.5 H 7.5 V 9.5 H 0.5 Z M 2.5 2.5 V 0.5 H 9.5 V 7.5 H 7.5" : "M 0.5 0.5 H 9.5 V 9.5 H 0.5 Z"
                onClicked: root.toggleMaximized()
            }
            WindowButton { glyph: "M 0 0 L 10 10 M 10 0 L 0 10"; closes: true; onClicked: root.close() }
        }
    }

    // --- the commands ---
    Row {
        id: commands
        anchors { top: strip.bottom; horizontalCenter: parent.horizontalCenter }
        height: root.fullScreen ? 0 : 40
        visible: !root.fullScreen
        spacing: 4
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "document-open"
            text: qsTr("Open")
            tooltip: qsTr("Open (Ctrl+O)")
            onClicked: root.open()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "document-edit"
            text: qsTr("Edit")
            tooltip: qsTr("Edit (Ctrl+E)")
            usable: root.hasImage && !Album.animated && !Album.scalable
            onClicked: root.edit()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "draw-brush"
            tooltip: qsTr("Open in Paint")
            usable: root.hasImage && !Album.animated && !Album.scalable
            onClicked: Album.openInPaint()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "object-rotate-right"
            tooltip: qsTr("Rotate (Ctrl+R)")
            usable: root.hasImage
            onClicked: root.turn = (root.turn + 90) % 360
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "edit-delete"
            tooltip: qsTr("Delete (Del)")
            usable: root.hasImage
            onClicked: root.askDelete()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "edit-copy"
            tooltip: qsTr("Copy (Ctrl+C)")
            usable: root.hasImage
            onClicked: Album.copy()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "document-print"
            tooltip: qsTr("Print (Ctrl+P)")
            usable: root.hasImage
            onClicked: Album.print()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "preferences-desktop-wallpaper"
            tooltip: qsTr("Set as desktop background")
            usable: root.hasImage
            onClicked: Album.setAsWallpaper()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "media-playback-start"
            tooltip: qsTr("Slideshow (F5)")
            usable: root.hasImage
            onClicked: root.startSlideshow()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "documentinfo"
            tooltip: qsTr("File info (Ctrl+I)")
            usable: root.hasImage
            checked: root.infoOpen
            onClicked: root.infoOpen = !root.infoOpen
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            icon: "view-fullscreen"
            tooltip: qsTr("Full screen (F11)")
            onClicked: root.toggleFullScreen()
        }
    }

    // --- the image ---
    Flickable {
        id: view
        anchors { left: parent.left; right: info.visible ? info.left : parent.right; top: commands.bottom; bottom: thumbs.top }
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, root.naturalWidth * root.factor)
        contentHeight: Math.max(height, root.naturalHeight * root.factor)

        Image {
            id: image
            anchors.centerIn: parent
            width: root.natural.width * root.factor
            height: root.natural.height * root.factor
            rotation: root.turn
            source: Album.current
            sourceSize: Album.scalable ? root.sharp : undefined
            visible: !Album.animated
            autoTransform: true // the camera's orientation
            asynchronous: true
            cache: false
            mipmap: true
            smooth: root.factor < 3 // pixels are pixels when zoomed far in
        }
        AnimatedImage { // a GIF plays; the still image above gives it its place
            anchors.fill: image
            rotation: root.turn
            visible: Album.animated
            source: Album.animated ? Album.current : ""
            cache: false
        }
        TapHandler {
            onDoubleTapped: point => root.zoom > 0 ? root.zoom = 0 : root.zoomTo(1, point.position.x - view.contentX, point.position.y - view.contentY)
        }
    }
    // The wheel zooms, like Windows; dragging is left to the view below.
    MouseArea {
        anchors.fill: view
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        onWheel: wheel => root.zoomTo(root.factor * (wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1), wheel.x, wheel.y)
    }
    DropArea {
        anchors.fill: view
        keys: ["text/uri-list"]
        onDropped: drop => {
            Album.open(drop.urls[0])
            drop.accept()
        }
    }

    Text {
        anchors.centerIn: view
        visible: root.hasImage && image.status === Image.Error
        text: qsTr("This file can't be shown.")
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
    }
    Column {
        anchors.centerIn: view
        visible: !root.hasImage
        spacing: 16
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Open an image, or drop one here.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontBody
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Open")
            accent: true
            onClicked: root.open()
        }
    }

    // The previous and the next image, at the sides.
    component SideButton: Rectangle {
        id: sideButton
        property string icon
        signal clicked()
        width: 36
        height: 48
        radius: Theme.radius
        color: Theme.flyout
        border.width: 1
        border.color: Theme.stroke
        opacity: sideMouse.containsMouse ? 1 : 0.8
        Image {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Theme.icons + sideButton.icon
            sourceSize: Qt.size(width, height)
        }
        MouseArea {
            id: sideMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: sideButton.clicked()
        }
    }
    SideButton {
        anchors { left: view.left; leftMargin: 12; verticalCenter: view.verticalCenter }
        visible: Album.index > 0
        icon: "go-previous"
        onClicked: Album.index--
    }
    SideButton {
        anchors { right: view.right; rightMargin: 12; verticalCenter: view.verticalCenter }
        visible: Album.index < Album.files.length - 1
        icon: "go-next"
        onClicked: Album.index++
    }

    // Full screen has no title bar or commands: the way back, in its corner.
    SideButton {
        anchors { right: view.right; top: view.top; margins: 12 }
        height: 36
        visible: root.fullScreen
        icon: "view-restore"
        onClicked: root.showNormal()
    }

    // --- the file's details ---
    Rectangle {
        id: info
        anchors { right: parent.right; top: commands.bottom; bottom: thumbs.top }
        width: 280
        visible: root.infoOpen && root.hasImage && !root.fullScreen
        color: Theme.layer
        Column {
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 14
            Repeater {
                model: [
                    { label: qsTr("Name"), value: Album.name },
                    { label: qsTr("Folder"), value: Album.folder },
                    { label: qsTr("Dimensions"), value: root.natural.width + " × " + root.natural.height },
                    { label: qsTr("Size"), value: Album.size },
                    { label: qsTr("Modified"), value: Album.modified }
                ]
                delegate: Column {
                    id: detail
                    required property var modelData
                    width: parent.width
                    spacing: 2
                    Text {
                        text: detail.modelData.label
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        width: parent.width
                        text: detail.modelData.value
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }

    // --- the folder's images ---
    ListView {
        id: thumbs
        anchors { bottom: status.top; horizontalCenter: parent.horizontalCenter }
        width: Math.min(parent.width - 16, contentWidth)
        height: visible ? 68 : 0
        visible: !root.fullScreen && count > 1
        orientation: ListView.Horizontal
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: 4
        model: Album.files
        delegate: Rectangle {
            id: thumb
            required property url modelData
            required property int index
            y: 4
            width: 60
            height: 60
            radius: Theme.radius
            color: thumbMouse.containsMouse ? Theme.subtleHover : "transparent"
            border.width: Album.index === index ? 2 : 0
            border.color: Theme.accentFill
            Image {
                anchors { fill: parent; margins: 4 }
                source: thumb.modelData
                sourceSize.height: 104 // a small copy, not the whole image
                cache: false // a file saved over shows its new image
                fillMode: Image.PreserveAspectCrop
                clip: true
                asynchronous: true
                autoTransform: true
            }
            MouseArea {
                id: thumbMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Album.index = thumb.index
            }
        }
    }

    // The wheel on the strip goes through the images; clicks reach the strip.
    MouseArea {
        anchors.fill: thumbs
        acceptedButtons: Qt.NoButton
        onWheel: wheel => Album.index += wheel.angleDelta.y < 0 ? 1 : -1
    }

    // --- the status bar ---
    Item {
        id: status
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: root.fullScreen ? 0 : 32
        visible: !root.fullScreen

        Text {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            visible: root.hasImage
            text: (Album.index + 1) + "/" + Album.files.length
                + ("  ·  " + root.natural.width + " × " + root.natural.height)
                + "  ·  " + Album.size
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Row {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            visible: root.hasImage
            spacing: 2
            ToolButton {
                height: 26
                icon: "zoom-fit-best"
                tooltip: qsTr("Fit to window (Ctrl+0)")
                checked: root.zoom === 0
                onClicked: root.zoom = 0
            }
            ToolButton {
                height: 26
                icon: "zoom-out"
                tooltip: qsTr("Zoom out (Ctrl+-)")
                onClicked: root.zoomTo(root.factor / 1.25)
            }
            Text {
                width: 44
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: Math.round(root.factor * 100) + "%"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            ToolButton {
                height: 26
                icon: "zoom-in"
                tooltip: qsTr("Zoom in (Ctrl++)")
                onClicked: root.zoomTo(root.factor * 1.25)
            }
        }
    }

    // --- Edit, over everything but the title bar; loaded when first used ---
    Loader {
        id: editLoader
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        z: 50
        active: root.editing
        source: "EditPage.qml"
        onLoaded: {
            item.source = Album.current
            item.failed.connect(root.showError)
            item.overwriteRequested.connect(() => overwriteDialog.open())
            item.finished.connect(saved => {
                const same = saved.toString() === Album.current.toString()
                root.editing = false
                if (saved.toString() === "") return
                Album.open(saved)
                if (same) { // the same name, another image
                    image.source = ""
                    image.source = Qt.binding(() => Album.current)
                }
            })
        }
    }

    // --- dialogs ---
    Dialog {
        id: errorDialog
        title: qsTr("Photos")
    }
    Dialog {
        id: deleteDialog
        title: qsTr("Delete")
        buttons: [{ text: qsTr("Delete"), accent: true, value: "delete" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => {
            if (value !== "delete") return
            const error = Album.trash()
            if (error !== "") root.showError(error)
        }
    }

    Dialog {
        id: overwriteDialog
        title: qsTr("Save")
        text: qsTr("Replace %1 with the edited image? The original can't be brought back.").arg(Album.name)
        buttons: [{ text: qsTr("Replace"), accent: true, value: "replace" }, { text: qsTr("Cancel"), value: "cancel" }]
        onChosen: value => { if (value === "replace" && editLoader.item) editLoader.item.save(true) }
    }

    ResizeEdges {}
}
