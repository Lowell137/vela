// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls
import Vela.Paint.Backend

// Paint's window, the one of Windows 11's without layers: its own title bar,
// the menus, the ribbon of tools, brushes, shapes and colors (which folds when
// the window narrows), the picture in the middle and a status bar. The picture and what the tools do to it are PaintCanvas (src/canvas.h).
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 1340
    height: 800
    minimumWidth: 560
    minimumHeight: 420
    visible: true
    color: Theme.background
    readonly property string name: canvas.source.toString() !== "" ? decodeURIComponent(canvas.source.toString().split("/").pop()) : qsTr("Untitled")
    title: name + qsTr(" - Paint")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    // --- the view ---
    property real zoom: 1
    function zoomTo(value) { zoom = Math.max(0.125, Math.min(8, value)) }
    property var pointer: null // over the picture, in its pixels
    // The magnifier: closer where it's clicked, the right button further.
    function magnify(x, y, out) {
        zoomTo(out ? zoom / 2 : zoom * 2)
        view.contentX = Math.max(0, Math.min(Math.max(0, view.contentWidth - view.width), sheet.x + x * zoom - view.width / 2))
        view.contentY = Math.max(0, Math.min(Math.max(0, view.contentHeight - view.height), sheet.y + y * zoom - view.height / 2))
    }
    // The wheel: closer or further around the pointer, so what's under it
    // stays there. (vx, vy) is the pointer in the view.
    function wheelZoom(vx, vy, notches) {
        const before = zoom
        const px = (view.contentX + vx - sheet.x) / before
        const py = (view.contentY + vy - sheet.y) / before
        zoomTo(before * Math.pow(1.1, notches))
        if (zoom === before) return
        view.contentX = Math.max(0, Math.min(Math.max(0, view.contentWidth - view.width), sheet.x + px * zoom - vx))
        view.contentY = Math.max(0, Math.min(Math.max(0, view.contentHeight - view.height), sheet.y + py * zoom - vy))
    }
    Settings {
        id: settings
        property bool gridlines: false
        property bool rulers: false
    }

    // --- the ribbon folds: its groups become buttons when the window narrows ---
    property var openGroup: null // the folded group whose commands are shown
    // How the ribbon fits. A group that can be squeezed (the shapes, the
    // palette) gives up room first, the first by `order` first; when that
    // isn't enough the groups fold, also by `order`, and what is left over
    // goes back, to the last by `order`. `squeeze` maps `order` to pixels.
    readonly property var plan: {
        const items = []
        for (let i = 0; i < groups.children.length; i++) {
            const group = groups.children[i]
            if (group.fullWidth === undefined) continue
            items.push(group)
        }
        items.sort((a, b) => a.order - b.order)
        // the narrowest it can all be with `k` folded
        const narrowest = k => items.reduce((sum, g, i) => sum + (i < k ? g.foldedWidth : g.fullWidth - g.flex), 0)
        let folded = 0
        while (folded < items.length && narrowest(folded) > ribbon.width) folded++
        let spare = ribbon.width - narrowest(folded)
        const squeeze = {}
        for (let i = items.length - 1; i >= folded; i--) {
            const back = Math.max(0, Math.min(spare, items[i].flex))
            spare -= back
            squeeze[items[i].order] = items[i].flex - back
        }
        return { folded: folded, squeeze: squeeze }
    }
    // The canvas' new size while one of its handles is dragged.
    property var dragSize: null

    // --- colors: the palette changes the well that was clicked last ---
    property bool secondWell: false
    function setColor(color, second) {
        if (second) canvas.color2 = color
        else canvas.color = color
    }
    readonly property var swatches: [
        "#000000", "#7f7f7f", "#880015", "#ed1c24", "#ff7f27", "#fff200", "#22b14c", "#00a2e8", "#3f48cc", "#a349a4",
        "#ffffff", "#c3c3c3", "#b97a57", "#ffaec9", "#ffc90e", "#efe4b0", "#b5e61d", "#99d9ea", "#7092be", "#c8bfe7"
    ]
    property var colorPicker: null
    function editColor() {
        if (!colorPicker) {
            colorPicker = Qt.createQmlObject("import QtQuick.Dialogs; ColorDialog {}", root)
            colorPicker.accepted.connect(() => root.setColor(colorPicker.selectedColor, root.secondWell))
        }
        colorPicker.selectedColor = secondWell ? canvas.color2 : canvas.color
        colorPicker.open()
    }

    // --- text: typed over the picture, drawn on it when done ---
    property string textFamily: Qt.application.font.family
    property int textSize: 24
    property bool textBold: false
    property bool textItalic: false
    property bool textUnderline: false
    function chooseFont() {
        fontLoader.active = true
        fontLoader.item.families = canvas.fontFamilies()
        fontLoader.item.ask(textFamily)
    }
    function startText(x, y) {
        finishText()
        typing.x = Math.round(x)
        typing.y = Math.round(y)
        typing.text = ""
        typing.visible = true
        typing.forceActiveFocus()
    }
    function finishText() {
        if (!typing.visible) return
        typing.visible = false
        canvas.drawText(typing.x, typing.y, typing.text, textFamily, textSize, textBold, textItalic, textUnderline)
    }

    // --- files ---
    property var picker: null
    function pick(save, done) {
        if (!picker) {
            const component = Qt.createComponent("FilePicker.qml")
            picker = component.createObject(root, { parentWindow: root })
            if (!picker) {
                showError(component.errorString())
                return
            }
        }
        picker.nameFilters = save ? [qsTr("PNG (*.png)"), qsTr("JPEG (*.jpg *.jpeg)"), qsTr("Pictures (%1)").arg(canvas.filter())]
            : [qsTr("Pictures (%1)").arg(canvas.filter()), qsTr("All files (*)")]
        const folder = canvas.source.toString() !== "" ? canvas.source.toString().replace(/\/[^\/]*$/, "")
            : StandardPaths.writableLocation(StandardPaths.PicturesLocation).toString()
        picker.pick(save, folder, save ? (canvas.source.toString() !== "" ? name : qsTr("Untitled") + ".png") : "",
            files => { if (files.length > 0) done(files[0]) })
    }
    function showError(message) {
        errorDialog.text = message
        errorDialog.open()
    }
    function write(url, then) {
        const error = canvas.save(url)
        if (error !== "") showError(qsTr("%1 can't be saved: %2").arg(decodeURIComponent(url.toString().split("/").pop())).arg(error))
        else if (then) then()
    }
    function saveAs(then) {
        finishText()
        pick(true, file => {
            const text = file.toString()
            // A name without an extension is a PNG.
            write(/\.[A-Za-z0-9]+$/.test(text) ? file : text + ".png", then)
        })
    }
    function save(then) {
        finishText()
        if (canvas.source.toString() === "") saveAs(then)
        else write(canvas.source, then)
    }
    // Before the picture is replaced or the window closed: Save, Don't save
    // or Cancel, if it has changes.
    property var pending: null
    function guard(action) {
        finishText()
        if (!canvas.modified) {
            action()
            return
        }
        pending = action
        saveDialog.text = qsTr("Do you want to save changes to %1?").arg(name)
        saveDialog.open()
    }
    function newFile() { guard(() => canvas.newImage(1152, 648)) }
    function load(url) {
        const error = canvas.load(url)
        if (error !== "") showError(qsTr("%1 can't be opened: %2").arg(decodeURIComponent(url.toString().split("/").pop())).arg(error))
    }
    function openFile() { guard(() => pick(false, file => load(file))) }
    property bool closingNow: false
    onClosing: close => {
        finishText()
        if (canvas.modified && !closingNow) {
            close.accepted = false
            guard(() => {
                closingNow = true
                root.close()
            })
        }
    }
    Component.onCompleted: if (StartFile.toString() !== "") load(StartFile)

    // --- menus ---
    function fileEntries() {
        return [
            { text: qsTr("&New"), shortcut: "Ctrl+N", action: () => newFile() },
            { text: qsTr("&Open..."), shortcut: "Ctrl+O", action: () => openFile() },
            { text: qsTr("&Save"), shortcut: "Ctrl+S", action: () => save() },
            { text: qsTr("Save &as..."), shortcut: "Ctrl+Shift+S", action: () => saveAs() },
            { separator: true },
            { text: qsTr("E&xit"), action: () => root.close() }
        ]
    }
    function editEntries() {
        const selected = canvas.selection.width > 0
        return [
            { text: qsTr("&Undo"), shortcut: "Ctrl+Z", enabled: canvas.canUndo, action: () => canvas.undo() },
            { text: qsTr("&Redo"), shortcut: "Ctrl+Y", enabled: canvas.canRedo, action: () => canvas.redo() },
            { separator: true },
            { text: qsTr("Cu&t"), shortcut: "Ctrl+X", enabled: selected, action: () => canvas.cut() },
            { text: qsTr("&Copy"), shortcut: "Ctrl+C", action: () => canvas.copy() },
            { text: qsTr("&Paste"), shortcut: "Ctrl+V", action: () => canvas.paste() },
            { text: qsTr("&Delete"), shortcut: "Del", enabled: selected, action: () => canvas.deleteSelection() },
            { separator: true },
            { text: qsTr("Select &all"), shortcut: "Ctrl+A", action: () => canvas.selectAll() },
            { text: qsTr("&Invert selection"), shortcut: "Ctrl+Shift+I", action: () => canvas.invertSelection() },
            { text: qsTr("Tra&nsparent selection"), checked: canvas.transparentSelection, action: () => canvas.transparentSelection = !canvas.transparentSelection }
        ]
    }
    function viewEntries() {
        return [
            { text: qsTr("Zoom &in"), shortcut: "Ctrl++", action: () => zoomTo(zoom * 2) },
            { text: qsTr("Zoom &out"), shortcut: "Ctrl+-", action: () => zoomTo(zoom / 2) },
            { text: qsTr("&100%"), shortcut: "Ctrl+0", action: () => zoomTo(1) },
            { separator: true },
            { text: qsTr("&Rulers"), shortcut: "Ctrl+Shift+R", checked: settings.rulers, action: () => settings.rulers = !settings.rulers },
            { text: qsTr("&Gridlines"), shortcut: "Ctrl+G", checked: settings.gridlines, action: () => settings.gridlines = !settings.gridlines }
        ]
    }
    function brushEntries() {
        return [
            ["round", qsTr("&Brush")], ["calligraphy", qsTr("&Calligraphy pen")], ["airbrush", qsTr("&Airbrush")],
            ["marker", qsTr("&Marker")], ["crayon", qsTr("C&rayon")], ["watercolor", qsTr("&Watercolor")]
        ].map(kind => ({
            text: kind[1],
            radio: true,
            checked: canvas.brush === kind[0],
            action: () => {
                canvas.brush = kind[0]
                canvas.tool = "brush"
            }
        }))
    }
    function openBelow(entries, button) {
        const p = button.mapToItem(null, 0, button.height)
        menuLayer.open(entries, p.x, p.y + 2, {})
    }

    // While text is typed the keys are its own.
    Shortcut { sequence: "Ctrl+N"; onActivated: root.newFile() }
    Shortcut { sequence: "Ctrl+O"; onActivated: root.openFile() }
    Shortcut { sequence: "Ctrl+S"; onActivated: root.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: root.saveAs() }
    Shortcut { sequence: "Ctrl+Z"; enabled: !typing.visible; onActivated: canvas.undo() }
    Shortcut { sequences: ["Ctrl+Y", "Ctrl+Shift+Z"]; enabled: !typing.visible; onActivated: canvas.redo() }
    Shortcut { sequence: "Ctrl+X"; enabled: !typing.visible; onActivated: canvas.cut() }
    Shortcut { sequence: "Ctrl+C"; enabled: !typing.visible; onActivated: canvas.copy() }
    Shortcut { sequence: "Ctrl+V"; enabled: !typing.visible; onActivated: canvas.paste() }
    Shortcut { sequence: "Ctrl+A"; enabled: !typing.visible; onActivated: canvas.selectAll() }
    Shortcut { sequence: "Delete"; enabled: !typing.visible; onActivated: canvas.deleteSelection() }
    Shortcut { sequence: "Escape"; enabled: !typing.visible; onActivated: root.openGroup ? root.openGroup = null : canvas.deselect() }
    Shortcut { sequences: ["Return", "Enter"]; enabled: !typing.visible; onActivated: canvas.finishShape() }
    Shortcut { sequence: "Ctrl+Shift+I"; onActivated: canvas.invertSelection() }
    Shortcut { sequence: "Ctrl+Shift+R"; onActivated: settings.rulers = !settings.rulers }
    Shortcut { sequence: "Ctrl+G"; onActivated: settings.gridlines = !settings.gridlines }
    Shortcut { sequence: "Ctrl+Shift+X"; onActivated: canvas.crop() }
    Shortcut { sequence: "Ctrl+W"; onActivated: resizeLoader.show() }
    Shortcut { sequence: "Ctrl+R"; onActivated: canvas.rotate() }
    Shortcut { sequences: ["Ctrl++", "Ctrl+="]; onActivated: root.zoomTo(root.zoom * 2) }
    Shortcut { sequence: "Ctrl+-"; onActivated: root.zoomTo(root.zoom / 2) }
    Shortcut { sequence: "Ctrl+0"; onActivated: root.zoomTo(1) }

    // --- the title bar ---
    Item {
        id: strip
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 32

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
            source: "image://appicon/applications-graphics"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 42
            width: parent.width - x - 3 * 46 - 8
            anchors.verticalCenter: parent.verticalCenter
            text: (canvas.modified ? "● " : "") + root.title
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

    // --- the menus, with Save, Undo and Redo next to them ---
    Row {
        id: menuBar
        anchors { left: parent.left; leftMargin: 6; top: strip.bottom }
        height: 36
        spacing: 4
        ToolButton {
            id: fileButton
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: qsTr("File")
            onClicked: root.openBelow(root.fileEntries(), fileButton)
        }
        ToolButton {
            id: editButton
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: qsTr("Edit")
            onClicked: root.openBelow(root.editEntries(), editButton)
        }
        ToolButton {
            id: viewButton
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: qsTr("View")
            onClicked: root.openBelow(root.viewEntries(), viewButton)
        }
        Item { width: 12; height: 1 }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            icon: "document-save"
            tooltip: qsTr("Save (Ctrl+S)")
            onClicked: root.save()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            icon: "edit-undo"
            tooltip: qsTr("Undo (Ctrl+Z)")
            usable: canvas.canUndo
            onClicked: canvas.undo()
        }
        ToolButton {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            icon: "edit-redo"
            tooltip: qsTr("Redo (Ctrl+Y)")
            usable: canvas.canRedo
            onClicked: canvas.redo()
        }
    }

    // --- the ribbon ---
    // A group of commands with its name under it, and a line after it. When
    // the window is too narrow for all of them, the groups fold in `order`
    // into a button that shows their commands in a panel under it.
    component Group: Item {
        id: group
        property string title
        property string icon
        property int order: 0
        default property alias content: inside.data
        // How much narrower it can be before it folds, and its full width
        // when its content's width follows the room (a Gallery).
        property real flex: 0
        property real natural: -1
        readonly property real fullWidth: (natural >= 0 ? natural : inside.width) + 25
        readonly property real foldedWidth: 68
        readonly property bool isFolded: order <= root.plan.folded
        readonly property real squeezed: isFolded ? 0 : (root.plan.squeeze[order] || 0)
        readonly property bool isOpen: root.openGroup === group
        onIsFoldedChanged: if (isOpen) root.openGroup = null
        width: isFolded ? foldedWidth : fullWidth - squeezed
        height: 92

        Row {
            id: inside
            parent: group.isFolded ? panelBody : group
            x: 12
            y: group.isFolded ? 8 : 0
            height: 64
            spacing: 2
        }
        ToolButton {
            visible: group.isFolded
            x: (parent.width - 1 - width) / 2
            y: 16
            icon: group.icon
            menu: true
            checked: group.isOpen
            onClicked: root.openGroup = group.isOpen ? null : group
        }
        Text {
            x: (parent.width - 1 - width) / 2
            anchors { bottom: parent.bottom; bottomMargin: 6 }
            text: group.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }
        Rectangle {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: 1
            height: 64
            color: Theme.stroke
        }
        Rectangle {
            id: panel
            parent: root.contentItem
            visible: group.isFolded && group.isOpen
            z: 90
            width: inside.width + 24
            height: 80
            radius: Theme.radiusOverlay
            color: Theme.flyout
            border.width: 1
            border.color: Theme.stroke
            onVisibleChanged: {
                if (!visible) return
                const p = group.mapToItem(null, 0, group.height)
                x = Math.max(4, Math.min(root.width - width - 4, p.x))
                y = p.y - 4
            }
            MouseArea { anchors.fill: parent } // a click between its buttons isn't one outside
            Item {
                id: panelBody
                anchors.fill: parent
            }
        }
    }
    // Commands in two rows shown a part at a time when their group is
    // squeezed: the wheel, a drag or the arrows at the ends move along them.
    component Gallery: Item {
        id: gallery
        property real naturalWidth: 0
        property real squeezed: 0 // from its group
        default property alias content: strip.data
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        width: naturalWidth - squeezed
        height: 58

        Flickable {
            id: scroller
            anchors.fill: parent
            clip: true
            contentWidth: gallery.naturalWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            onWidthChanged: returnToBounds()
            Item {
                id: strip
                width: gallery.naturalWidth
                height: gallery.height
            }
        }
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => {
                const delta = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x
                scroller.contentX = Math.max(0, Math.min(scroller.contentWidth - scroller.width, scroller.contentX - delta))
            }
        }
        Rectangle {
            visible: scroller.contentX > 0
            width: 20
            height: parent.height
            color: Theme.flyout // opaque: what's under the arrow isn't shown through it
            ToolButton {
                anchors.centerIn: parent
                width: 18
                height: 28
                icon: "go-previous"
                onClicked: scroller.contentX = Math.max(0, scroller.contentX - 90)
            }
        }
        Rectangle {
            visible: scroller.contentX < scroller.contentWidth - scroller.width - 1
            x: parent.width - width
            width: 20
            height: parent.height
            color: Theme.flyout
            ToolButton {
                anchors.centerIn: parent
                width: 18
                height: 28
                icon: "go-next"
                onClicked: scroller.contentX = Math.min(scroller.contentWidth - scroller.width, scroller.contentX + 90)
            }
        }
    }
    // A tool: lit while it's the one in hand.
    component Tool: ToolButton {
        property string tool
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        checked: canvas.tool === tool
        onClicked: canvas.tool = tool
    }
    // Small commands, one above the other.
    component Pair: Column {
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        spacing: 2
    }

    Rectangle {
        id: ribbon
        anchors { left: parent.left; right: parent.right; top: menuBar.bottom }
        height: 92
        color: Theme.layer
        clip: true

        Row {
            id: groups
            Group {
                title: qsTr("Selection")
                icon: "select-rectangular"
                order: 5
                Tool { tool: "select"; icon: "select-rectangular"; tooltip: qsTr("Select") }
                Tool { tool: "lasso"; icon: "select-lasso"; tooltip: qsTr("Free-form selection") }
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "transform-crop"
                    tooltip: qsTr("Crop to the selection (Ctrl+Shift+X)")
                    usable: canvas.selection.width > 0
                    onClicked: canvas.crop()
                }
                ToolButton {
                    id: selectionMore
                    anchors.verticalCenter: parent.verticalCenter
                    menu: true
                    tooltip: qsTr("Selection options")
                    onClicked: root.openBelow([
                        { text: qsTr("Select &all"), shortcut: "Ctrl+A", action: () => canvas.selectAll() },
                        { text: qsTr("&Invert selection"), shortcut: "Ctrl+Shift+I", action: () => canvas.invertSelection() },
                        { text: qsTr("&Delete"), shortcut: "Del", enabled: canvas.selection.width > 0, action: () => canvas.deleteSelection() },
                        { separator: true },
                        { text: qsTr("Tra&nsparent selection"), checked: canvas.transparentSelection, action: () => canvas.transparentSelection = !canvas.transparentSelection }
                    ], selectionMore)
                }
            }
            Group {
                title: qsTr("Image")
                icon: "transform-scale"
                order: 3
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "transform-scale"
                    tooltip: qsTr("Resize (Ctrl+W)")
                    onClicked: resizeLoader.show()
                }
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "object-rotate-right"
                    tooltip: qsTr("Rotate (Ctrl+R)")
                    onClicked: canvas.rotate()
                }
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "object-flip-horizontal"
                    tooltip: qsTr("Flip horizontally")
                    onClicked: canvas.flip(true)
                }
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "object-flip-vertical"
                    tooltip: qsTr("Flip vertically")
                    onClicked: canvas.flip(false)
                }
            }
            Group {
                title: qsTr("Tools")
                icon: "document-edit"
                order: 6
                Tool { tool: "pencil"; icon: "document-edit"; tooltip: qsTr("Pencil") }
                Tool { tool: "fill"; icon: "color-fill"; tooltip: qsTr("Fill") }
                Tool { tool: "text"; icon: "draw-text"; tooltip: qsTr("Text") }
                Tool { tool: "eraser"; icon: "draw-eraser"; tooltip: qsTr("Eraser") }
                Tool { tool: "picker"; icon: "color-picker"; tooltip: qsTr("Color picker") }
                Tool { tool: "zoom"; icon: "edit-find"; tooltip: qsTr("Magnifier") }
            }
            Group {
                title: qsTr("Brushes")
                icon: "draw-brush"
                order: 7
                Tool { tool: "brush"; icon: "draw-brush"; tooltip: qsTr("Brush") }
                ToolButton {
                    id: brushMore
                    anchors.verticalCenter: parent.verticalCenter
                    menu: true
                    tooltip: qsTr("Brushes")
                    onClicked: root.openBelow(root.brushEntries(), brushMore)
                }
            }
            Group {
                id: shapesGroup
                title: qsTr("Shapes")
                icon: "shape-polygon"
                order: 1
                flex: 210 // down to four columns
                natural: 366 // 11 columns of 28 and 2, the outline and fill buttons, the gaps
                Gallery {
                  naturalWidth: 328
                  squeezed: shapesGroup.squeezed
                  Grid {
                    columns: 11
                    spacing: 2
                    Repeater {
                        model: [
                            ["line", qsTr("Line")], ["curve", qsTr("Curve")], ["polygon", qsTr("Polygon")],
                            ["rectangle", qsTr("Rectangle")], ["rounded", qsTr("Rounded rectangle")], ["ellipse", qsTr("Ellipse")],
                            ["triangle", qsTr("Triangle")], ["right-triangle", qsTr("Right triangle")], ["diamond", qsTr("Diamond")],
                            ["pentagon", qsTr("Pentagon")], ["hexagon", qsTr("Hexagon")],
                            ["arrow", qsTr("Right arrow")], ["arrow-left", qsTr("Left arrow")], ["arrow-up", qsTr("Up arrow")],
                            ["arrow-down", qsTr("Down arrow")], ["star4", qsTr("Four-point star")], ["star", qsTr("Five-point star")],
                            ["star6", qsTr("Six-point star")], ["callout", qsTr("Callout")], ["lightning", qsTr("Lightning")],
                            ["heart", qsTr("Heart")]
                        ]
                        delegate: ToolButton {
                            required property var modelData
                            width: 28
                            height: 28
                            icon: "shape-" + modelData[0]
                            tooltip: modelData[1]
                            checked: canvas.tool === modelData[0]
                            onClicked: canvas.tool = modelData[0]
                        }
                    }
                  }
                }
                Item { width: 6; height: 1 }
                Pair {
                    ToolButton {
                        width: 28
                        height: 28
                        icon: "shape-outline"
                        tooltip: qsTr("Outline, in color 1")
                        checked: canvas.outlined
                        onClicked: canvas.outlined = !canvas.outlined
                    }
                    ToolButton {
                        width: 28
                        height: 28
                        icon: "format-fill-color"
                        tooltip: qsTr("Fill, in color 2")
                        checked: canvas.filled
                        onClicked: canvas.filled = !canvas.filled
                    }
                }
            }
            Group {
                title: qsTr("Size")
                icon: "shape-line"
                order: 4
                Slider {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    from: 1
                    to: 40
                    stepSize: 1
                    value: canvas.size
                    onMoved: value => canvas.size = value
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40
                    text: canvas.size + " px"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                }
            }
            Group {
                id: colorsGroup
                title: qsTr("Colors")
                icon: "color-management"
                order: 2
                flex: 182 // down to three columns of swatches
                natural: 394 // the two wells, 10 columns of 22 and 4, the edit button, the gaps
                Repeater {
                    model: 2
                    delegate: Rectangle {
                        required property int index
                        readonly property bool active: root.secondWell === (index === 1)
                        anchors.verticalCenter: parent.verticalCenter
                        width: 40
                        height: 52
                        radius: Theme.radius
                        color: active ? Theme.subtleHover : "transparent"
                        Rectangle {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 6 }
                            width: 26
                            height: 26
                            radius: 13
                            color: index === 1 ? canvas.color2 : canvas.color
                            border.width: 1
                            border.color: Theme.controlStrokeStrong
                        }
                        Text {
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 3 }
                            text: index + 1
                            color: Theme.textSecondary
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.secondWell = index === 1
                        }
                    }
                }
                Item { width: 8; height: 1 }
                Gallery {
                  naturalWidth: 256
                  squeezed: colorsGroup.squeezed
                  Grid {
                    y: 5
                    columns: 10
                    spacing: 4
                    Repeater {
                        model: root.swatches
                        delegate: Rectangle {
                            required property string modelData
                            width: 22
                            height: 22
                            radius: 11
                            color: modelData
                            border.width: 1
                            border.color: Theme.controlStrokeStrong
                            MouseArea { // the right button: the other well
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: mouse => root.setColor(parent.modelData, root.secondWell !== (mouse.button === Qt.RightButton))
                            }
                        }
                    }
                  }
                }
                Item { width: 8; height: 1 }
                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    icon: "color-management"
                    tooltip: qsTr("Edit colors")
                    onClicked: root.editColor()
                }
            }
        }
    }
    // A click anywhere else closes a folded group's panel.
    MouseArea {
        anchors.fill: parent
        z: 89
        visible: root.openGroup !== null
        acceptedButtons: Qt.AllButtons
        onPressed: root.openGroup = null
    }

    // --- the text's font, while the text tool is in hand ---
    Rectangle {
        id: textBar
        anchors { left: parent.left; right: parent.right; top: ribbon.bottom }
        visible: canvas.tool === "text"
        height: visible ? 40 : 0
        color: Theme.layer
        Row {
            x: 12
            height: parent.height
            spacing: 4
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "preferences-desktop-font"
                text: root.textFamily
                tooltip: qsTr("Font")
                onClicked: root.chooseFont()
            }
            Item { width: 8; height: 1 }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "list-remove"
                tooltip: qsTr("Smaller")
                usable: root.textSize > 8
                onClicked: root.textSize = Math.max(8, root.textSize - (root.textSize > 32 ? 4 : 2))
            }
            Text {
                width: 44
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: root.textSize + " px"
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "list-add"
                tooltip: qsTr("Larger")
                usable: root.textSize < 288
                onClicked: root.textSize = Math.min(288, root.textSize + (root.textSize >= 32 ? 4 : 2))
            }
            Item { width: 8; height: 1 }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "format-text-bold"
                tooltip: qsTr("Bold")
                checked: root.textBold
                onClicked: root.textBold = !root.textBold
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "format-text-italic"
                tooltip: qsTr("Italic")
                checked: root.textItalic
                onClicked: root.textItalic = !root.textItalic
            }
            ToolButton {
                anchors.verticalCenter: parent.verticalCenter
                icon: "format-text-underline"
                tooltip: qsTr("Underline")
                checked: root.textUnderline
                onClicked: root.textUnderline = !root.textUnderline
            }
        }
    }

    // --- the picture ---
    // A square to drag: the canvas' edges and the selection's corners.
    component Handle: Rectangle {
        id: handle
        signal dragged(real x, real y) // the pointer, in the picture's pixels
        signal dropped()
        width: 9
        height: 9
        color: "white"
        border.width: 1
        border.color: "#606060"
        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            preventStealing: true
            cursorShape: Qt.SizeAllCursor
            onPositionChanged: mouse => {
                const p = mapToItem(sheet, mouse.x, mouse.y)
                handle.dragged(p.x / root.zoom, p.y / root.zoom)
            }
            onReleased: handle.dropped()
        }
    }
    // The ruler's numbers: every `step` pixels of the picture, far enough apart to read.
    readonly property int rulerStep: [10, 20, 50, 100, 200, 500, 1000, 2000].find(step => step * zoom >= 60) || 5000
    readonly property int rulerSize: settings.rulers ? 22 : 0

    Rectangle {
        id: desk
        anchors { left: parent.left; right: parent.right; top: textBar.bottom; bottom: status.top }
        color: Theme.background
        clip: true

        // The wheel zooms around the pointer; Shift moves along the picture
        // and Alt up and down. Buttons go through to what is below.
        MouseArea {
            z: 10
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: wheel => {
                const p = mapToItem(view, wheel.x, wheel.y)
                const along = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                if ((wheel.modifiers & Qt.ShiftModifier) || wheel.angleDelta.y === 0) {
                    view.contentX = Math.max(0, Math.min(Math.max(0, view.contentWidth - view.width), view.contentX - along))
                } else if (wheel.modifiers & Qt.AltModifier) {
                    view.contentY = Math.max(0, Math.min(Math.max(0, view.contentHeight - view.height), view.contentY - along))
                } else {
                    root.wheelZoom(Math.max(0, Math.min(view.width, p.x)), Math.max(0, Math.min(view.height, p.y)), wheel.angleDelta.y / 120)
                }
            }
        }

        Flickable {
            id: view
            anchors { fill: parent; leftMargin: root.rulerSize; topMargin: root.rulerSize }
            clip: true
            interactive: false // dragging draws
            contentWidth: Math.max(width, sheet.width + 48)
            contentHeight: Math.max(height, sheet.height + 48)

            Item {
                id: sheet
                // In the middle while there's room around it.
                x: Math.max(24, Math.round((view.width - width) / 2))
                y: Math.max(24, Math.round((view.height - height) / 2))
                width: canvas.width * root.zoom
                height: canvas.height * root.zoom

                PaintCanvas {
                    id: canvas
                    scale: root.zoom
                    transformOrigin: Item.TopLeft
                    smooth: root.zoom < 1 // enlarged pixels stay square
                    onPicked: (color, secondary) => root.setColor(color, secondary)
                    onToolChanged: root.finishText()

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        hoverEnabled: true
                        cursorShape: canvas.tool === "text" ? Qt.IBeamCursor : Qt.CrossCursor
                        onPressed: mouse => {
                            if (canvas.tool === "text") root.startText(mouse.x, mouse.y)
                            else if (canvas.tool === "zoom") root.magnify(mouse.x, mouse.y, mouse.button === Qt.RightButton)
                            else canvas.press(mouse.x, mouse.y, mouse.button === Qt.RightButton)
                        }
                        onPositionChanged: mouse => {
                            root.pointer = Qt.point(Math.floor(mouse.x), Math.floor(mouse.y))
                            if (pressed) canvas.move(mouse.x, mouse.y, mouse.modifiers & Qt.ShiftModifier)
                        }
                        onReleased: canvas.release()
                        onDoubleClicked: canvas.finishShape()
                        onExited: root.pointer = null
                    }

                    TextEdit {
                        id: typing
                        visible: false
                        color: canvas.color
                        font.family: root.textFamily
                        font.pixelSize: root.textSize
                        font.bold: root.textBold
                        font.italic: root.textItalic
                        font.underline: root.textUnderline
                        selectionColor: Theme.accent
                        renderType: Text.QtRendering // scaled with the picture
                        Keys.onEscapePressed: visible = false
                        Keys.onPressed: event => {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                                root.finishText()
                                event.accepted = true
                            }
                        }
                        Rectangle {
                            x: -3
                            y: -3
                            width: Math.max(parent.width, 40) + 6
                            height: parent.height + 6
                            color: "transparent"
                            border.width: Math.max(1, 1 / root.zoom)
                            border.color: Theme.accentFill
                        }
                    }
                }

                // Lines between the pixels when they're large, every ten otherwise.
                Image {
                    readonly property real tile: (root.zoom >= 4 ? 1 : 10) * root.zoom
                    anchors.fill: parent
                    visible: settings.gridlines && tile >= 4
                    fillMode: Image.Tile
                    horizontalAlignment: Image.AlignLeft
                    verticalAlignment: Image.AlignTop
                    sourceSize: Qt.size(tile, tile)
                    source: "data:image/svg+xml," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">'
                        + '<path d="M0 .5H10M.5 0V10" stroke="#808080" stroke-opacity=".5" stroke-width="1" vector-effect="non-scaling-stroke"/></svg>')
                }

                // The selection's corners stretch it.
                Repeater {
                    model: canvas.selection.width > 0 && (canvas.tool === "select" || canvas.tool === "lasso") ? 4 : 0
                    delegate: Handle {
                        required property int index
                        readonly property bool atRight: index % 2 === 1
                        readonly property bool atBottom: index >= 2
                        readonly property rect box: canvas.selection
                        x: (atRight ? box.x + box.width : box.x) * root.zoom - 4
                        y: (atBottom ? box.y + box.height : box.y) * root.zoom - 4
                        onDragged: (px, py) => {
                            // The opposite corner stays where it is.
                            const fx = atRight ? box.x : box.x + box.width
                            const fy = atBottom ? box.y : box.y + box.height
                            const w = Math.max(1, Math.round(atRight ? px - fx : fx - px))
                            const h = Math.max(1, Math.round(atBottom ? py - fy : fy - py))
                            canvas.stretchSelection(Qt.rect(atRight ? fx : fx - w, atBottom ? fy : fy - h, w, h))
                        }
                    }
                }

                // The canvas' own size: its right edge, its bottom and their corner.
                Rectangle {
                    visible: root.dragSize !== null
                    width: root.dragSize ? root.dragSize.width * root.zoom : 0
                    height: root.dragSize ? root.dragSize.height * root.zoom : 0
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.accentFill
                }
                Repeater {
                    model: canvas.selection.width > 0 ? 0 : 3
                    delegate: Handle {
                        required property int index
                        readonly property bool across: index !== 1
                        readonly property bool down: index !== 0
                        x: across ? sheet.width + 1 : sheet.width / 2 - 4
                        y: down ? sheet.height + 1 : sheet.height / 2 - 4
                        onDragged: (px, py) => root.dragSize = Qt.size(
                            across ? Math.max(1, Math.min(16000, Math.round(px))) : canvas.imageSize.width,
                            down ? Math.max(1, Math.min(16000, Math.round(py))) : canvas.imageSize.height)
                        onDropped: {
                            if (root.dragSize) canvas.resize(root.dragSize.width, root.dragSize.height, false)
                            root.dragSize = null
                        }
                    }
                }
            }

        }
        ScrollBar {
            flickable: view
            anchors { right: parent.right; top: view.top; bottom: parent.bottom }
        }

        // --- rulers, in the picture's pixels ---
        Rectangle {
            visible: settings.rulers
            x: root.rulerSize
            width: parent.width - x
            height: root.rulerSize
            color: Theme.layer
            clip: true
            Item {
                x: sheet.x - view.contentX
                Repeater {
                    model: settings.rulers ? Math.floor(canvas.imageSize.width / root.rulerStep) + 1 : 0
                    delegate: Item {
                        required property int index
                        x: index * root.rulerStep * root.zoom
                        Rectangle { width: 1; height: root.rulerSize; color: Theme.controlStrokeStrong }
                        Text {
                            x: 4
                            y: 2
                            text: index * root.rulerStep
                            color: Theme.textSecondary
                            font.pixelSize: 10
                        }
                    }
                }
                Rectangle {
                    visible: root.pointer !== null
                    x: root.pointer ? root.pointer.x * root.zoom : 0
                    width: 1
                    height: root.rulerSize
                    color: Theme.accentFill
                }
            }
        }
        Rectangle {
            visible: settings.rulers
            y: root.rulerSize
            width: root.rulerSize
            height: parent.height - y
            color: Theme.layer
            clip: true
            Item {
                y: sheet.y - view.contentY
                Repeater {
                    model: settings.rulers ? Math.floor(canvas.imageSize.height / root.rulerStep) + 1 : 0
                    delegate: Item {
                        required property int index
                        y: index * root.rulerStep * root.zoom
                        Rectangle { width: root.rulerSize; height: 1; color: Theme.controlStrokeStrong }
                        Text {
                            x: 2
                            y: 3
                            width: root.rulerSize - 3
                            text: index * root.rulerStep
                            color: Theme.textSecondary
                            font.pixelSize: 8
                            elide: Text.ElideRight
                        }
                    }
                }
                Rectangle {
                    visible: root.pointer !== null
                    y: root.pointer ? root.pointer.y * root.zoom : 0
                    width: root.rulerSize
                    height: 1
                    color: Theme.accentFill
                }
            }
        }
        Rectangle { // the corner between them
            visible: settings.rulers
            width: root.rulerSize
            height: root.rulerSize
            color: Theme.layer
        }
    }

    // --- the status bar ---
    Rectangle {
        id: status
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 30
        color: Theme.layer

        Row {
            x: 12
            height: parent.height
            spacing: 24
            Text {
                width: 110
                anchors.verticalCenter: parent.verticalCenter
                text: root.pointer ? root.pointer.x + ", " + root.pointer.y + " px" : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            Text {
                width: 130
                anchors.verticalCenter: parent.verticalCenter
                text: canvas.selection.width > 0 ? qsTr("Selection: %1 × %2 px").arg(canvas.selection.width).arg(canvas.selection.height) : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.dragSize ? root.dragSize.width + " × " + root.dragSize.height : canvas.imageSize.width + " × " + canvas.imageSize.height) + " px"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
        }
        Row {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 2
            ToolButton {
                height: 26
                icon: "zoom-out"
                tooltip: qsTr("Zoom out (Ctrl+-)")
                onClicked: root.zoomTo(root.zoom / 2)
            }
            Text {
                width: 48
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                text: Math.round(root.zoom * 100) + "%"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            ToolButton {
                height: 26
                icon: "zoom-in"
                tooltip: qsTr("Zoom in (Ctrl++)")
                onClicked: root.zoomTo(root.zoom * 2)
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
    }

    // --- dialogs ---
    Loader {
        id: resizeLoader
        anchors.fill: parent
        z: 150
        active: false
        source: "ResizeDialog.qml"
        function show() {
            root.finishText()
            active = true
            item.show(canvas.imageSize)
        }
        onLoaded: item.resized.connect((width, height, scale) => canvas.resize(width, height, scale))
    }
    Loader {
        id: fontLoader
        anchors.fill: parent
        z: 150
        active: false
        source: "FontDialog.qml"
    }
    Connections {
        target: fontLoader.item
        function onAccepted(family) { root.textFamily = family }
    }
    Dialog {
        id: errorDialog
        title: qsTr("Paint")
    }
    Dialog {
        id: saveDialog
        title: qsTr("Paint")
        buttons: [
            { text: qsTr("Save"), accent: true, value: "save" },
            { text: qsTr("Don't save"), value: "discard" },
            { text: qsTr("Cancel"), value: "cancel" }
        ]
        onChosen: value => {
            const action = root.pending
            root.pending = null
            if (value === "save") root.save(action)
            else if (value === "discard" && action) action()
        }
    }

    ResizeEdges {}
}
