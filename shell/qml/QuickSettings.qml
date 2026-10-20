// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Vela.Audio
import Vela.Controls

// Quick settings (Win+A, or a click on the taskbar's system icons), like
// Windows 11: tiles to turn on and off (Wi-Fi, Bluetooth, airplane mode, power
// saving, night light...), brightness and volume, battery and Settings at the
// bottom. An acrylic panel at the bottom right. Some tiles have a page of
// their own: the Wi-Fi arrow lists the networks to choose one, Accessibility
// turns on the magnifier, color filters and sticky keys; the arrow next to
// the volume opens the sound page (outputs and the mixer, Win+Ctrl+V).
Window {
    id: root
    visible: false
    width: Theme.quickSettingsWidth
    height: panel.height + 24 + (mediaApart ? mediaCard.height + 8 : 0)
    color: "transparent"

    // "" the quick settings; "wifi", "sound" and "accessibility" their pages.
    property string page: ""

    function open() {
        Menus.notificationCenterOpen = false
        page = ""
        if (!Menus.placeOnTargetScreen(root)) {
            Qt.callLater(root.open)
            return
        }
        visible = true
        Menus.quickSettingsOpen = true
        panel.opacity = 0
        slide.y = Config.taskbarVertical ? 0 : (Config.taskbarTop || Config.dock ? -12 : 12)
        slide.x = Config.taskbarLeft ? -12 : (Config.taskbarRight ? 12 : 0)
        appear.restart()
        panel.forceActiveFocus()
        updateBlur()
    }
    function close() {
        visible = false
        Menus.quickSettingsOpen = false
    }
    function toggle() {
        if (visible) {
            close()
        } else {
            open()
        }
    }
    // Win+Ctrl+V, "Open volume mixer": straight to the sound page.
    function toggleSound() {
        if (visible && page === "sound") {
            close()
            return
        }
        if (!visible) {
            open()
        }
        showPage("sound")
    }
    function showPage(name) {
        page = name
        pageIn.restart()
        if (name === "wifi") {
            wifiPage.expanded = ""
            wifiPage.password = ""
            Network.scan() // cached networks appear immediately, rescan in background
        }
        panel.forceActiveFocus()
    }
    // At the bottom right, above the taskbar: from panel to output
    // coordinates.
    function screenPoint(item, x, y) {
        const p = item.mapToItem(null, x, y)
        return Menus.cornerPoint(p, root.width, root.height, Screen.width, Screen.height, true, 0)
    }
    // The outputs (or inputs) for one app (the mixer), in a menu under its
    // button.
    function chooseAppDevice(app, button, input) {
        const choose = name => () => input ? Audio.setAppInput(app.key, name) : Audio.setAppOutput(app.key, name)
        const current = input ? app.input : app.output
        const entries = [{ text: input ? qsTr("Default input") : qsTr("Default output"), radio: true,
                           checked: current === "", action: choose("") },
                         { separator: true }]
        for (const device of input ? Audio.inputs : Audio.outputs) {
            entries.push({ text: device.description.replace(/&/g, "&&"), icon: device.icon, radio: true,
                           checked: current === device.name, action: choose(device.name) })
        }
        const p = screenPoint(button, 0, button.height + 4)
        Menus.open(entries, p.x, p.y, { minWidth: 240, screen: root.screen ? root.screen.name : "" })
    }
    // The system volume: the default output's.
    function setSystemVolume(value) {
        if (Audio.muted) Audio.setMuted("output", Audio.defaultOutput, false)
        Audio.setVolume("output", Audio.defaultOutput, value)
    }
    // Caffeine for a while: the arrow on its tile lists durations under it.
    // Keep the selected preset separate from the decreasing remaining time.
    function caffeineMatches(minutes) {
        return Access.caffeine && Access.caffeineDurationSecs === minutes * 60
    }
    function openCaffeineMenu(tile) {
        const presets = [30, 60, 120, 300]
        const names = [qsTr("30 minutes"), qsTr("1 hour"), qsTr("2 hours"), qsTr("5 hours")]
        const entries = []
        if (Access.caffeine && Access.caffeineSecs > 0)
            entries.push({ header: qsTr("%1 minutes remaining").arg(Math.ceil(Access.caffeineSecs / 60)) })
        entries.push({ text: qsTr("Indefinitely"), radio: true,
                           checked: Access.caffeine && Access.caffeineSecs < 0,
                           action: () => Access.caffeine = true },
                         { separator: true })
        for (let i = 0; i < presets.length; i++) {
            const mins = presets[i]
            entries.push({ text: names[i], radio: true, checked: caffeineMatches(mins),
                           action: () => Access.setCaffeineMinutes(mins) })
        }
        entries.push({ separator: true },
                     { text: qsTr("Custom..."), action: () => root.showPage("caffeine") },
                     { separator: true },
                     { text: qsTr("Turn off"), enabled: Access.caffeine,
                       action: () => Access.caffeine = false })
        const p = screenPoint(tile, 0, tile.height + 4)
        Menus.open(entries, p.x, p.y, { minWidth: 220, screen: root.screen ? root.screen.name : "" })
    }
    function openSettings(name) {
        close()
        System.trigger(name)
    }

    Connections {
        target: Shell
        function onQuickSettingsRequested() { root.toggle() }
        function onSoundOutputRequested() { root.toggleSound() }
        function onNotificationCenterRequested() { root.close() }
    }

    // Back from a page: Alt+Left, the keyboard's Back key and the mouse's
    // side button (common/navigationbuttons.cpp), like the arrow at the top.
    Shortcut {
        sequences: [StandardKey.Back]
        enabled: root.page !== ""
        onActivated: root.showPage("")
    }

    onActiveChanged: {
        if (!active && visible && !Menus.isOpen) {
            close()
        }
    }

    function updateBlur() {
        const rects = [Qt.rect(panel.x, panel.y, panel.width, panel.height)]
        if (mediaApart) {
            rects.push(Qt.rect(mediaCard.x, mediaCard.y, mediaCard.width, mediaCard.height))
        }
        Effects.setBlur(root, rects)
    }

    ParallelAnimation {
        id: appear
        NumberAnimation { target: panel; property: "opacity"; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "y"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: slide; property: "x"; to: 0; duration: Theme.slow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }
    // A page comes in from the right (back: like the others, it's short).
    ParallelAnimation {
        id: pageIn
        NumberAnimation { target: pages; property: "opacity"; from: 0; to: 1; duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
        NumberAnimation { target: pageSlide; property: "x"; from: 24; to: 0; duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate }
    }

    // A tile: on (accent color) or off, with the label below. `split`: an
    // arrow on the right opening its page (Wi-Fi); `menu`: the whole tile
    // opens the page (Accessibility).
    component Tile: Item {
        id: tile
        property string icon
        property string label
        property bool checked
        property bool usable: true
        property bool split: false
        property bool menu: false
        // quicksettings-split-gap: the two parts as separate buttons.
        readonly property int gap: split ? Theme.quickSettingsSplitGap : 0
        readonly property real mainWidth: split ? button.width - 36 - gap : button.width
        signal toggled()
        signal details()
        signal contextMenu()
        // 104 x 82 by default: the panel's inner width shared by the columns.
        width: Math.floor((Theme.quickSettingsWidth - 56 - 8 * (Theme.quickSettingsColumns - 1)) / Theme.quickSettingsColumns)
        height: Theme.quickSettingsTileHeight
        opacity: usable ? 1 : 0.45

        Rectangle {
            id: button
            width: parent.width
            height: Theme.quickSettingsTileHeight - 34
            radius: Theme.quickSettingsTileRadius
            color: tile.gap > 0 ? "transparent" : tile.checked ? Theme.accent : Theme.control
            border.width: tile.checked || tile.gap > 0 ? 0 : 1
            border.color: Theme.stroke
            clip: true

            Repeater {
                model: tile.gap > 0 ? [[0, tile.mainWidth], [button.width - 36, 36]] : []
                delegate: Rectangle {
                    required property var modelData
                    x: modelData[0]
                    width: modelData[1]
                    height: button.height
                    radius: button.radius
                    color: tile.checked ? Theme.accent : Theme.control
                    border.width: tile.checked ? 0 : 1
                    border.color: Theme.stroke
                }
            }

            // The two halves (or the whole tile), each with its hover.
            Rectangle {
                x: 0
                width: tile.mainWidth
                height: parent.height
                radius: parent.radius
                color: tile.checked ? "white" : Theme.overlay
                opacity: mainMouse.pressed ? 0.06 : mainMouse.containsMouse && tile.usable ? 0.08 : 0
            }
            Rectangle {
                visible: tile.split
                x: parent.width - 36
                width: 36
                height: parent.height
                radius: parent.radius
                color: tile.checked ? "white" : Theme.overlay
                opacity: detailsMouse.pressed ? 0.06 : detailsMouse.containsMouse && tile.usable ? 0.08 : 0
            }
            Rectangle {
                visible: tile.split && tile.gap === 0
                x: parent.width - 36
                y: 10
                width: 1
                height: parent.height - 20
                color: tile.checked ? Qt.rgba(1, 1, 1, 0.35) : Theme.stroke
            }
            Image {
                x: tile.mainWidth / 2 - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                source: (tile.checked ? Theme.iconsOnAccent : Theme.icons) + encodeURIComponent(tile.icon)
                sourceSize: Qt.size(width, height)
            }
            // The ">" arrow.
            Shape {
                visible: tile.split || tile.menu
                x: tile.split ? parent.width - 21 : parent.width - 18
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: 10
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: tile.checked ? "white" : Theme.text
                    strokeWidth: 1.2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 1; startY: 1
                    PathLine { x: 5; y: 5 }
                    PathLine { x: 1; y: 9 }
                }
            }
            MouseArea {
                id: mainMouse
                width: tile.mainWidth
                height: parent.height
                hoverEnabled: true
                enabled: tile.usable
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton) {
                        tile.contextMenu()
                    } else if (tile.menu) {
                        tile.details()
                    } else {
                        tile.toggled()
                    }
                }
            }
            MouseArea {
                id: detailsMouse
                visible: tile.split
                x: parent.width - 36
                width: 36
                height: parent.height
                hoverEnabled: true
                enabled: tile.usable
                onClicked: tile.details()
            }
        }
        Text {
            anchors { top: button.bottom; topMargin: 6; horizontalCenter: parent.horizontalCenter }
            width: parent.width
            text: tile.label
            color: Theme.text
            font.pixelSize: Theme.fontCaption
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    // A slider (brightness, volume, an app's volume) with its icon on the
    // left: a click on it mutes. `details`: an arrow on the right opening a
    // page (the volume's: the sound page). While it's dragged the number shows
    // above the knob, like Windows.
    component Slider: Item {
        id: slider
        property string icon
        property real value
        property bool muted: false
        // An app's icon (the mixer): muted, it fades and gets a crossed
        // speaker; the volume's icon is already the crossed one.
        property bool appIcon: false
        property bool details: false
        // What it plays now, 0-1 (docs/audio.md §7.1); below 0: no bar.
        property real level: -1
        signal moved(real value)
        signal wheeled(int direction)
        signal iconClicked()
        signal detailsClicked()
        readonly property real shown: Math.max(0, Math.min(1, value))
        width: parent.width
        height: 36

        Item {
            id: iconButton
            width: 36
            height: 36
            Rectangle {
                anchors.fill: parent
                radius: Theme.radius
                color: Theme.subtleHover
                opacity: iconMouse.containsMouse ? 1 : 0
            }
            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                opacity: slider.muted && slider.appIcon ? 0.45 : 1
                source: Theme.icons + encodeURIComponent(slider.icon)
                sourceSize: Qt.size(width, height)
            }
            Image {
                visible: slider.muted && slider.appIcon
                anchors { right: parent.right; bottom: parent.bottom; margins: 5 }
                width: 12
                height: 12
                source: Theme.icons + "audio-volume-muted"
                sourceSize: Qt.size(width, height)
            }
            MouseArea {
                id: iconMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: slider.iconClicked()
            }
        }
        Item {
            id: track
            anchors { left: iconButton.right; leftMargin: 12; right: detailsButton.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
            height: 20
            // quicksettings-thick-slider: a thick track and a wide knob.
            readonly property bool thick: Theme.quickSettingsThickSlider
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: track.thick ? 10 : 4
                radius: height / 2
                color: Theme.stroke
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                // Thick: up to the knob's middle, the knob kept inside the track.
                width: track.thick ? knob.x + knob.width / 2 : parent.width * slider.shown
                height: track.thick ? 10 : 4
                radius: height / 2
                color: slider.muted ? Theme.textSecondary : Theme.accent
            }
            // The level: a thin bar under the track.
            Rectangle {
                visible: slider.level >= 0
                y: parent.height / 2 + 5
                width: parent.width * Math.max(0, Math.min(1, slider.level))
                height: 2
                radius: 1
                color: Theme.accent
                opacity: 0.6
            }
            Rectangle {
                id: knob
                x: track.thick ? (parent.width - width) * slider.shown : parent.width * slider.shown - width / 2
                anchors.verticalCenter: parent.verticalCenter
                // Thick (WindowGlass README): Thumb 40 x 25, CornerRadius 12,
                // the inner dot collapsed.
                width: track.thick ? 40 : 18
                height: track.thick ? 25 : 18
                radius: track.thick ? 12 : height / 2
                color: Theme.popup
                border.width: 1
                // Ours, not in the README: held, the edge lights up.
                border.color: track.thick && trackMouse.pressed ? Theme.controlStrokeStrong : Theme.stroke
                Rectangle {
                    visible: !track.thick
                    anchors.centerIn: parent
                    // Fluent-Qt Slider.cpp: 9, 14 under the pointer, 250 ms OutCubic.
                    width: trackMouse.containsMouse || trackMouse.pressed ? 14 : 9
                    Behavior on width { NumberAnimation { duration: Theme.normal; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
                    height: width
                    radius: width / 2
                    color: slider.muted ? Theme.textSecondary : Theme.accent
                }
            }
            Rectangle {
                visible: trackMouse.pressed
                anchors { bottom: knob.top; bottomMargin: 6; horizontalCenter: knob.horizontalCenter }
                width: valueText.implicitWidth + 16
                height: 26
                radius: track.thick ? 10 : Theme.radius // WindowGlass: ToolTip CornerRadius 10
                color: Theme.popup
                border.width: 1
                border.color: Theme.stroke
                Text {
                    id: valueText
                    anchors.centerIn: parent
                    text: Math.round(slider.shown * 100)
                    color: Theme.text
                    font.pixelSize: Theme.fontCaption
                }
            }
            MouseArea {
                id: trackMouse
                anchors { fill: parent; margins: -8 }
                hoverEnabled: true
                function update(x) {
                    // Thick: the knob's middle travels the track minus the knob.
                    const v = track.thick ? (x - 8 - knob.width / 2) / (track.width - knob.width) : (x - 8) / track.width
                    slider.moved(Math.max(0, Math.min(1, v)))
                }
                onPressed: mouse => update(mouse.x)
                onPositionChanged: mouse => { if (pressed) update(mouse.x) }
                // A step every notch (120), touchpads add up their small ones.
                property real wheelRest: 0
                onWheel: wheel => {
                    wheelRest += wheel.angleDelta.y
                    while (Math.abs(wheelRest) >= 120) {
                        slider.wheeled(wheelRest > 0 ? 1 : -1)
                        wheelRest -= wheelRest > 0 ? 120 : -120
                    }
                }
            }
        }
        Item {
            id: detailsButton
            anchors.right: parent.right
            width: slider.details ? 36 : 0
            height: 36
            visible: slider.details
            Rectangle {
                anchors.fill: parent
                radius: Theme.radius
                color: Theme.subtleHover
                opacity: detailsMouse.containsMouse ? 1 : 0
            }
            Shape {
                anchors.centerIn: parent
                width: 6
                height: 10
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Theme.text
                    strokeWidth: 1.2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: 1; startY: 1
                    PathLine { x: 5; y: 5 }
                    PathLine { x: 1; y: 9 }
                }
            }
            MouseArea {
                id: detailsMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: slider.detailsClicked()
            }
        }
    }

    // A toggle switch like Windows 11's.
    component Switch: Item {
        id: toggle
        property bool checked
        signal toggled()
        width: 40
        height: 20
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: toggle.checked ? Theme.accent : "transparent"
            border.width: toggle.checked ? 0 : 1
            border.color: Theme.textSecondary
            Rectangle {
                x: toggle.checked ? parent.width - width - 4 : 4
                anchors.verticalCenter: parent.verticalCenter
                width: switchMouse.containsMouse ? 14 : 12
                height: width
                radius: width / 2
                color: toggle.checked ? "white" : Theme.textSecondary
                Behavior on x { NumberAnimation { duration: Theme.fast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.decelerate } }
            }
        }
        MouseArea {
            id: switchMouse
            anchors { fill: parent; margins: -4 }
            hoverEnabled: true
            onClicked: toggle.toggled()
        }
    }

    // A page header: back, the title, and on the right what's needed.
    component PageHeader: Item {
        id: header
        property string title
        default property alias trailing: trailingSlot.data
        width: parent.width
        height: 36
        Item {
            id: back
            width: 32
            height: 32
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                anchors.fill: parent
                radius: Theme.radius
                color: Theme.subtleHover
                opacity: backMouse.containsMouse ? 1 : 0
            }
            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Theme.icons + "go-previous"
                sourceSize: Qt.size(width, height)
            }
            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.showPage("")
            }
        }
        Text {
            anchors { left: back.right; leftMargin: 8; verticalCenter: parent.verticalCenter }
            text: header.title
            color: Theme.text
            font.pixelSize: Theme.fontBody + 2
            font.weight: Font.DemiBold
        }
        Item {
            id: trailingSlot
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            width: childrenRect.width
            height: childrenRect.height
        }
    }

    // Where an app's sound goes or comes from, in the mixer: a small button
    // with an arrow, which opens the list.
    component DeviceChooser: Rectangle {
        id: chooser
        property alias text: chooserLabel.text
        signal clicked()
        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
        width: visible ? Math.min(chooserLabel.implicitWidth, soundColumn.width * 0.45) + 28 : 0
        height: 22
        radius: Theme.radius
        color: chooserMouse.containsMouse ? Theme.subtleHover : "transparent"
        Text {
            id: chooserLabel
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 28
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        // The "v" arrow.
        Shape {
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            width: 9
            height: 5
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: Theme.textSecondary
                strokeWidth: 1.2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                startX: 1; startY: 1
                PathLine { x: 4.5; y: 4.5 }
                PathLine { x: 8; y: 1 }
            }
        }
        MouseArea {
            id: chooserMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: chooser.clicked()
        }
    }

    // A link at the bottom of a page ("More settings...").
    component Link: Text {
        id: link
        property string target
        color: linkMouse.containsMouse ? Theme.accentFill : Theme.accent
        font.pixelSize: Theme.fontBody
        font.underline: linkMouse.containsMouse
        MouseArea {
            id: linkMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.openSettings(link.target)
        }
    }

    // A media card's button: "previous", "play", "pause" or "next".
    component MediaButton: Item {
        id: mediaButton
        property string icon
        property string label
        property bool usable: true
        signal clicked()
        width: 44
        height: 36
        opacity: usable ? 1 : 0.4
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: mediaButton.clicked()
        Rectangle {
            anchors.fill: parent
            radius: Theme.radius
            color: Theme.subtleHover
            opacity: mediaMouse.containsMouse && mediaButton.usable ? 1 : 0
        }
        // Solid glyphs in the text color, like Windows' (the icon theme's
        // media icons are thin and grey).
        Shape {
            anchors.centerIn: parent
            width: 20
            height: 20
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: Theme.text
                strokeColor: "transparent"
                PathSvg {
                    path: ({
                        previous: "M3.5 4h2v12h-2z M16.5 4L7 10l9.5 6z",
                        play: "M6 3.5L17 10L6 16.5z",
                        pause: "M5 3.5h3.5v13H5z M11.5 3.5H15v13h-3.5z",
                        next: "M14.5 4h2v12h-2z M3.5 4L13 10l-9.5 6z"
                    })[mediaButton.icon]
                }
            }
        }
        MouseArea {
            id: mediaMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: mediaButton.usable
            onClicked: mediaButton.clicked()
        }
    }

    component MediaPanel: Rectangle {
        id: media
        readonly property var player: Media.current
        visible: player.service !== undefined
        width: parent.width
        height: 128
        radius: Theme.radius
        color: Theme.control
        border.width: 1
        border.color: Theme.stroke

        Image {
            id: mediaApp
            x: 12
            y: 12
            width: 16
            height: 16
            source: Theme.icons + encodeURIComponent(Apps.iconForAppId(media.player.desktopEntry || "audio-x-generic"))
            sourceSize: Qt.size(width, height)
        }
        Text {
            anchors { left: mediaApp.right; leftMargin: 8; right: mediaSwitch.visible ? mediaSwitch.left : cover.left; rightMargin: 12; verticalCenter: mediaApp.verticalCenter }
            text: media.player.identity || ""
            color: Theme.text
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        // Several players: the previous / next one, and which.
        Row {
            id: mediaSwitch
            visible: media.player.count > 1
            anchors { right: cover.left; rightMargin: 12; verticalCenter: mediaApp.verticalCenter }
            spacing: 4
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: media.player.index + "/" + media.player.count
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }
            Repeater {
                model: [{ t: "\u2039", d: -1, n: qsTr("Previous player") }, { t: "\u203a", d: 1, n: qsTr("Next player") }]
                Rectangle {
                    width: 20
                    height: 20
                    radius: Theme.radius
                    color: switchMouse.containsMouse ? Theme.subtleHover : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.n
                    Accessible.onPressAction: Media.cycle(modelData.d)
                    Text {
                        anchors.centerIn: parent
                        text: modelData.t
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                    }
                    MouseArea {
                        id: switchMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: Media.cycle(modelData.d)
                    }
                }
            }
        }
        Text {
            id: mediaTitle
            anchors { left: mediaApp.left; right: cover.left; rightMargin: 12; top: mediaApp.bottom; topMargin: 8 }
            text: media.player.title || media.player.identity || ""
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            anchors { left: mediaTitle.left; right: mediaTitle.right; top: mediaTitle.bottom; topMargin: 2 }
            text: media.player.artist || ""
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
        // The cover (file:// or http(s)://, loaded in the
        // background); until it's there, or without one, a note.
        Rectangle {
            id: cover
            anchors { right: parent.right; rightMargin: 12; top: parent.top; topMargin: 12 }
            width: 64
            height: 64
            radius: Theme.radius
            color: Theme.subtleHover
            clip: true
            Image {
                anchors.centerIn: parent
                width: 32
                height: 32
                visible: art.status !== Image.Ready
                source: Theme.icons + "audio-x-generic"
                sourceSize: Qt.size(width, height)
            }
            Image {
                id: art
                anchors.fill: parent
                visible: status === Image.Ready
                source: media.player.artUrl || ""
                sourceSize: Qt.size(128, 128)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: false // players reuse one file for every track
            }
        }
        Row {
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8 }
            spacing: 12
            MediaButton {
                icon: "previous"
                label: qsTr("Previous")
                usable: media.player.canGoPrevious === true
                onClicked: Media.command("previous")
            }
            MediaButton {
                icon: media.player.playing ? "pause" : "play"
                label: media.player.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: Media.command("play-pause")
            }
            MediaButton {
                icon: "next"
                label: qsTr("Next")
                usable: media.player.canGoNext === true
                onClicked: Media.command("next")
            }
        }
    }


    // quicksettings-media-card: what plays now on a card of its own, below the
    // panel (WindowGlass), or above it with quicksettings-media-top.
    readonly property bool mediaApart: Theme.quickSettingsMediaCard && Media.current.service !== undefined && page === ""
    Rectangle {
        id: mediaCard
        visible: root.mediaApart
        x: panel.x
        y: Theme.quickSettingsMediaTop ? 12 : panel.y + panel.height + 8
        width: panel.width
        height: mediaInCard.height + 24
        radius: Theme.quickSettingsRadius
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        opacity: panel.opacity
        transform: Translate { x: slide.x; y: slide.y }
        onVisibleChanged: root.updateBlur()
        onYChanged: root.updateBlur()
        MediaPanel {
            id: mediaInCard
            x: 12
            y: 12
            width: parent.width - 24
            color: "transparent"
            border.width: 0
        }
    }

    Rectangle {
        id: panel
        x: 12
        y: root.mediaApart && Theme.quickSettingsMediaTop ? 12 + mediaCard.height + 8 : 12
        onYChanged: root.updateBlur()
        width: root.width - 24
        height: root.page === "" ? content.height + 24 + footer.height
            : (root.page === "wifi" ? wifiPage.height : root.page === "sound" ? soundPage.height
                : root.page === "caffeine" ? caffeinePage.height : accessPage.height) + 32
        radius: Theme.quickSettingsRadius
        color: Theme.surface
        border.width: 1
        border.color: Theme.stroke
        transform: Translate { id: slide }
        focus: true
        clip: true
        Keys.onEscapePressed: {
            if (root.page !== "") {
                root.showPage("")
            } else {
                root.close()
            }
        }
        onHeightChanged: root.updateBlur()

        Item {
            id: pages
            anchors.fill: parent
            transform: Translate { id: pageSlide }

            // ---------------------------------------------------------- main --
            Column {
                id: content
                visible: root.page === ""
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 16

                // What plays now (MPRIS, see mediakeys.h), like Windows 11:
                // the app, the track and the artist, the cover on the right,
                // the controls below. Only while there is a player.
                MediaPanel {
                    visible: player.service !== undefined && !root.mediaApart
                    width: parent.width
                }

                Grid {
                    columns: Theme.quickSettingsColumns
                    columnSpacing: 8
                    rowSpacing: 4

                    Tile {
                        visible: Status.wifiAvailable
                        icon: Status.wifiEnabled ? "network-wireless" : "network-wireless-disconnected"
                        label: Status.networkWireless && Status.networkName !== "" ? Status.networkName : qsTr("Wi-Fi")
                        checked: Status.wifiEnabled
                        split: Network.available
                        onToggled: Status.setWifiEnabled(!Status.wifiEnabled)
                        onDetails: root.showPage("wifi")
                        onContextMenu: root.openSettings("network")
                    }
                    // Without Wi-Fi (a wired desktop): the network as it is,
                    // for information.
                    Tile {
                        visible: !Status.wifiAvailable
                        icon: Status.networkIconName
                        label: Status.networkConnected ? (Status.networkName !== "" ? Status.networkName : qsTr("Network")) : qsTr("Not connected")
                        checked: Status.networkConnected
                        onToggled: root.openSettings("network")
                        onContextMenu: root.openSettings("network")
                    }
                    Tile {
                        visible: Status.bluetoothAvailable
                        icon: Status.bluetoothEnabled ? "network-bluetooth" : "network-bluetooth-inactive"
                        label: qsTr("Bluetooth")
                        checked: Status.bluetoothEnabled
                        split: true
                        onToggled: Status.setBluetoothEnabled(!Status.bluetoothEnabled)
                        onDetails: root.openSettings("bluetooth-settings")
                        onContextMenu: root.openSettings("bluetooth-settings")
                    }
                    Tile {
                        visible: Status.wifiAvailable || Status.bluetoothAvailable
                        icon: "network-flightmode-on"
                        label: qsTr("Airplane mode")
                        checked: Status.airplane
                        onToggled: Status.setAirplaneMode(!Status.airplane)
                    }
                    Tile {
                        visible: Status.powerSaverAvailable
                        icon: "battery-profile-powersave"
                        label: qsTr("Energy saver")
                        checked: Status.powerSaver
                        onToggled: Status.setPowerSaver(!Status.powerSaver)
                        onContextMenu: root.openSettings("power")
                    }
                    Tile {
                        icon: "redshift-status-on"
                        label: qsTr("Night light")
                        checked: Access.nightLight
                        onToggled: Access.nightLight = !Access.nightLight
                        onContextMenu: root.openSettings("night-light-settings")
                    }
                    // Keep awake: no lock nor screen off until turned off (only
                    // for this session). Breeze's only cup is KTeaTime's. The
                    // left half toggles indefinitely, the arrow lists
                    // durations (openCaffeineMenu).
                    Tile {
                        id: caffeineTile
                        icon: "kteatime-symbolic"
                        label: Access.caffeine && Access.caffeineSecs > 0
                            ? qsTr("Caffeine %1m").arg(Math.ceil(Access.caffeineSecs / 60))
                            : qsTr("Caffeine")
                        checked: Access.caffeine
                        split: true
                        onToggled: Access.caffeine = !Access.caffeine
                        onDetails: root.openCaffeineMenu(caffeineTile)
                        onContextMenu: root.openSettings("power")
                    }
                    Tile {
                        icon: "preferences-desktop-accessibility"
                        label: qsTr("Accessibility")
                        checked: Access.magnifier || Access.colorFilter || Access.stickyKeys
                        menu: true
                        onDetails: root.showPage("accessibility")
                        onContextMenu: root.openSettings("accessibility-settings")
                    }
                }

                Slider {
                    visible: Status.brightnessAvailable
                    icon: "brightness-high"
                    value: Status.brightness
                    onMoved: value => Status.setBrightness(value)
                    onWheeled: direction => Status.setBrightness(Math.max(0, Math.min(1, Status.brightness + direction * 0.02)))
                }
                // The arrow opens the sound page: outputs and the mixer.
                Slider {
                    visible: Audio.available && Audio.defaultOutput !== ""
                    icon: Audio.iconName
                    value: Audio.volume
                    muted: Audio.muted
                    details: Audio.available
                    onMoved: value => root.setSystemVolume(value)
                    onWheeled: direction => Audio.stepVolume("output", Audio.defaultOutput, direction)
                    onIconClicked: Audio.setMuted("output", Audio.defaultOutput, !Audio.muted)
                    onDetailsClicked: root.showPage("sound")
                }
            }

            // --------------------------------------------- Wi-Fi networks --
            Column {
                id: wifiPage
                visible: root.page === "wifi"
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 8
                property string expanded: "" // the open network
                property string connecting: "" // the network being connected to
                property string password: ""

                PageHeader {
                    title: qsTr("Wi-Fi")
                    Switch {
                        checked: Status.wifiEnabled
                        onToggled: Status.setWifiEnabled(!Status.wifiEnabled)
                    }
                }
                Text {
                    visible: !Status.wifiEnabled || Network.wifiNetworks.length === 0
                    width: parent.width
                    topPadding: 8
                    bottomPadding: 8
                    text: !Status.wifiEnabled ? qsTr("Wi-Fi is turned off.")
                        : Network.scanning ? qsTr("Looking for networks...") : qsTr("No networks found.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }
                Flickable {
                    visible: Status.wifiEnabled && Network.wifiNetworks.length > 0
                    width: parent.width
                    height: Math.min(contentHeight, 340)
                    contentHeight: networks.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: networks
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: Network.wifiNetworks
                            delegate: Rectangle {
                                id: net
                                required property var modelData
                                readonly property bool open: wifiPage.expanded === modelData.ssid
                                readonly property bool needsPassword: modelData.secure && !modelData.active
                                    && (!modelData.known || Network.passwordRequiredSsid === modelData.ssid)
                                readonly property bool busy: wifiPage.connecting === modelData.ssid && Network.connectResult === ""
                                width: networks.width
                                height: open ? row.height + actions.height + 12 : row.height
                                radius: Theme.radius
                                color: open ? Theme.control : netMouse.containsMouse ? Theme.subtleHover : "transparent"
                                clip: true

                                Item {
                                    id: row
                                    width: parent.width
                                    height: 52
                                    Image {
                                        x: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 20
                                        height: 20
                                        source: Theme.icons + (net.modelData.signal > 75 ? "network-wireless-signal-excellent"
                                            : net.modelData.signal > 50 ? "network-wireless-signal-good"
                                            : net.modelData.signal > 25 ? "network-wireless-signal-ok" : "network-wireless-signal-weak")
                                        sourceSize: Qt.size(width, height)
                                    }
                                    Column {
                                        x: 44
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - 56
                                        Text {
                                            width: parent.width
                                            text: net.modelData.ssid
                                            color: Theme.text
                                            font.pixelSize: Theme.fontBody
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: net.busy ? qsTr("Connecting...")
                                                : net.modelData.active ? (net.modelData.secure ? qsTr("Connected, secured") : qsTr("Connected"))
                                                : net.modelData.secure ? qsTr("Secured") : qsTr("Open")
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontCaption
                                        }
                                    }
                                    MouseArea {
                                        id: netMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            wifiPage.expanded = net.open ? "" : net.modelData.ssid
                                            wifiPage.password = ""
                                        }
                                    }
                                }

                                Column {
                                    id: actions
                                    visible: net.open
                                    x: 44
                                    y: row.height
                                    width: parent.width - 56
                                    spacing: 8

                                    // Ask for a key only on first join or if a
                                    // saved connection fails authentication.
                                    Rectangle {
                                        visible: net.needsPassword
                                        width: parent.width
                                        height: 32
                                        radius: Theme.radius
                                        color: Theme.control
                                        border.width: 1
                                        border.color: passwordField.activeFocus ? Theme.accent : Theme.stroke
                                        MenuTextField {
                                            id: passwordField
                                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                                            verticalAlignment: TextInput.AlignVCenter
                                            echoMode: TextInput.Password
                                            text: wifiPage.password
                                            onTextEdited: wifiPage.password = text
                                            onAccepted: connectButton.activate()
                                            mapToScreen: (x, y) => root.screenPoint(passwordField, x, y)
                                        }
                                        Text {
                                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                                            visible: passwordField.text === ""
                                            text: qsTr("Enter the network security key")
                                            color: Theme.textSecondary
                                            font.pixelSize: Theme.fontCaption
                                        }
                                    }
                                    Text {
                                        visible: wifiPage.connecting === net.modelData.ssid && Network.connectResult !== "" && Network.connectResult !== "ok"
                                        width: parent.width
                                        text: Network.connectResult
                                        color: Theme.light ? "#c42b1c" : "#ff99a4"
                                        font.pixelSize: Theme.fontCaption
                                        wrapMode: Text.WordWrap
                                    }
                                    Rectangle {
                                        id: connectButton
                                        anchors.right: parent.right
                                        width: 120
                                        height: 32
                                        radius: Theme.radius
                                        readonly property bool primary: !net.modelData.active
                                        readonly property bool usable: !net.busy && (!net.needsPassword || wifiPage.password.length >= 8)
                                        opacity: usable ? 1 : 0.5
                                        color: primary ? (buttonMouse.containsMouse ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
                                            : buttonMouse.containsMouse ? Theme.subtleHover : Theme.control
                                        border.width: primary ? 0 : 1
                                        border.color: Theme.stroke
                                        function activate() {
                                            if (!usable) return
                                            if (net.modelData.active) {
                                                Network.disconnectWifi(net.modelData.ssid)
                                            } else {
                                                wifiPage.connecting = net.modelData.ssid
                                                Network.connectWifi(net.modelData.ssid, net.needsPassword ? wifiPage.password : "")
                                            }
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: net.modelData.active ? qsTr("Disconnect") : net.needsPassword ? qsTr("Next") : qsTr("Connect")
                                            color: connectButton.primary ? "white" : Theme.text
                                            font.pixelSize: Theme.fontBody
                                        }
                                        MouseArea {
                                            id: buttonMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: connectButton.activate()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                Item { width: 1; height: 4 }
                Link {
                    text: qsTr("More Wi-Fi settings")
                    target: "network"
                }
            }

            // ------------------------------------------------------- sound --
            // Like Windows 11's: where the sound goes, then the volume of the
            // output and of each app playing.
            Column {
                id: soundPage
                visible: root.page === "sound"
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 8

                PageHeader { title: qsTr("Sound output") }
                Flickable {
                    width: parent.width
                    height: Math.min(contentHeight, 440)
                    contentHeight: soundColumn.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: soundColumn
                        width: parent.width
                        spacing: 2

                        Repeater {
                            model: Audio.outputs
                            delegate: Rectangle {
                                id: output
                                required property var modelData
                                width: soundColumn.width
                                height: 44
                                radius: Theme.radius
                                color: outputMouse.containsMouse ? Theme.subtleHover : output.modelData.isDefault ? Theme.control : "transparent"
                                // The chosen one: the accent line on the left,
                                // like a selected item on Windows.
                                Rectangle {
                                    visible: output.modelData.isDefault
                                    x: 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 3
                                    height: 16
                                    radius: 1.5
                                    color: Theme.accent
                                }
                                Image {
                                    x: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 18
                                    height: 18
                                    source: Theme.icons + encodeURIComponent(output.modelData.icon)
                                    sourceSize: Qt.size(width, height)
                                }
                                Text {
                                    x: 44
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 56 - (battery.visible ? battery.width + 8 : 0)
                                    text: output.modelData.description
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    elide: Text.ElideRight
                                }
                                // A Bluetooth headset's charge, when it
                                // reports it.
                                Row {
                                    id: battery
                                    visible: output.modelData.battery >= 0
                                    anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                                    spacing: 4
                                    Image {
                                        width: 16
                                        height: 16
                                        source: Theme.icons + (output.modelData.battery >= 90 ? "battery-full"
                                            : output.modelData.battery >= 40 ? "battery-good"
                                            : output.modelData.battery >= 15 ? "battery-low" : "battery-caution")
                                        sourceSize: Qt.size(width, height)
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: output.modelData.battery + "%"
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontCaption
                                    }
                                }
                                MouseArea {
                                    id: outputMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: Audio.setDefault("output", output.modelData.name)
                                }
                            }
                        }

                        Item { width: 1; height: 12 }
                        Text {
                            text: qsTr("Volume mixer")
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            bottomPadding: 4
                        }
                        // The output's volume first, then the apps'.
                        Text {
                            text: (Audio.outputs.find(o => o.isDefault) || { description: qsTr("Speakers") }).description
                            width: parent.width
                            leftPadding: 48
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideRight
                        }
                        Slider {
                            icon: Audio.iconName
                            value: Audio.volume
                            muted: Audio.muted
                            onMoved: value => root.setSystemVolume(value)
                            onWheeled: direction => Audio.stepVolume("output", Audio.defaultOutput, direction)
                            onIconClicked: Audio.setMuted("output", Audio.defaultOutput, !Audio.muted)
                        }
                        Repeater {
                            model: Audio.apps
                            delegate: Column {
                                id: app
                                required property var modelData
                                width: soundColumn.width
                                topPadding: 6
                                // The name, and on the right where its sound
                                // goes: a click chooses another output.
                                Item {
                                    width: parent.width
                                    height: 20
                                    Text {
                                        x: 48
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: parent.width - 48 - appOutput.width - 12
                                        text: app.modelData.name
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontCaption
                                        elide: Text.ElideRight
                                    }
                                    DeviceChooser {
                                        id: appOutput
                                        visible: app.modelData.playing && (Audio.outputs.length > 1 || app.modelData.output !== "")
                                        text: app.modelData.output === "" ? qsTr("Default output") : app.modelData.outputDescription
                                        onClicked: root.chooseAppDevice(app.modelData, appOutput, false)
                                    }
                                }
                                LevelMeter {
                                    id: appLevel
                                    audio: Audio
                                    kind: "app"
                                    name: app.modelData.key
                                    active: root.visible && soundPage.visible && app.modelData.playing
                                }
                                Slider {
                                    visible: app.modelData.playing
                                    icon: app.modelData.icon
                                    appIcon: true
                                    value: app.modelData.volume
                                    level: appLevel.level
                                    muted: app.modelData.muted
                                    onMoved: value => Audio.setAppVolume(app.modelData.key, value)
                                    onWheeled: direction => Audio.stepAppVolume(app.modelData.key, direction)
                                    onIconClicked: Audio.setAppMuted(app.modelData.key, !app.modelData.muted)
                                }
                                // An app that records: where its sound comes
                                // from.
                                Item {
                                    visible: app.modelData.recording && (Audio.inputs.length > 1 || app.modelData.input !== "")
                                    width: parent.width
                                    height: visible ? 24 : 0
                                    Image {
                                        x: 15
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 16
                                        height: 16
                                        source: Theme.icons + (app.modelData.playing ? "audio-input-microphone" : encodeURIComponent(app.modelData.icon))
                                        sourceSize: Qt.size(width, height)
                                    }
                                    DeviceChooser {
                                        id: appInput
                                        anchors.right: undefined
                                        x: 40
                                        text: app.modelData.input === "" ? qsTr("Default input") : app.modelData.inputDescription
                                        onClicked: root.chooseAppDevice(app.modelData, appInput, true)
                                    }
                                }
                            }
                        }
                        Text {
                            visible: Audio.apps.length === 0
                            width: parent.width
                            topPadding: 8
                            leftPadding: 48
                            text: qsTr("No apps are playing sound.")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Item { width: 1; height: 4 }
                Link {
                    text: qsTr("More volume settings")
                    target: "volume-mixer" // Settings' Volume mixer, like Windows
                }
            }

            // ------------------------------------------------ accessibility --
            Column {
                id: accessPage
                visible: root.page === "accessibility"
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 4

                PageHeader { title: qsTr("Accessibility") }
                Item { width: 1; height: 4 }
                Repeater {
                    model: [
                        { icon: "zoom-in", text: qsTr("Magnifier"), key: "magnifier" },
                        { icon: "preferences-desktop-color", text: qsTr("Color filters"), key: "colorFilter" },
                        { icon: "input-keyboard", text: qsTr("Sticky keys"), key: "stickyKeys" }
                    ]
                    delegate: Rectangle {
                        id: option
                        required property var modelData
                        width: accessPage.width
                        height: 48
                        radius: Theme.radius
                        color: optionMouse.containsMouse ? Theme.subtleHover : "transparent"
                        Image {
                            x: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 18
                            height: 18
                            source: Theme.icons + option.modelData.icon
                            sourceSize: Qt.size(width, height)
                        }
                        Text {
                            x: 44
                            anchors.verticalCenter: parent.verticalCenter
                            text: option.modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                        }
                        MouseArea {
                            id: optionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: Access[option.modelData.key] = !Access[option.modelData.key]
                        }
                        Switch {
                            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                            checked: Access[option.modelData.key]
                            onToggled: Access[option.modelData.key] = !Access[option.modelData.key]
                        }
                    }
                }
                Item { width: 1; height: 8 }
                Link {
                    text: qsTr("More accessibility settings")
                    target: "accessibility-settings"
                }
            }

            // ---------------------------------------------------- caffeine --
            // Custom duration (the menu's "Custom..."): minutes to stay awake.
            Column {
                id: caffeinePage
                visible: root.page === "caffeine"
                x: 16
                y: 16
                width: parent.width - 32
                spacing: 8
                property string minutes: "30"

                PageHeader { title: qsTr("Keep awake") }
                Text {
                    width: parent.width
                    text: qsTr("Stay awake for these minutes, then let the screen lock and turn off again.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.WordWrap
                }
                Row {
                    width: parent.width
                    spacing: 8
                    Rectangle {
                        width: parent.width - startButton.width - 8
                        height: 32
                        radius: Theme.radius
                        color: Theme.control
                        border.width: 1
                        border.color: minutesField.activeFocus ? Theme.accent : Theme.stroke
                        MenuTextField {
                            id: minutesField
                            anchors { fill: parent; leftMargin: 10; rightMargin: 10 }
                            verticalAlignment: TextInput.AlignVCenter
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 24 * 60 }
                            text: caffeinePage.minutes
                            onTextEdited: caffeinePage.minutes = text
                            onAccepted: startButton.activate()
                            mapToScreen: (x, y) => root.screenPoint(minutesField, x, y)
                        }
                        Text {
                            anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                            visible: minutesField.text === ""
                            text: qsTr("Minutes")
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                    Rectangle {
                        id: startButton
                        width: 120
                        height: 32
                        radius: Theme.radius
                        readonly property bool usable: minutesField.acceptableInput
                        opacity: usable ? 1 : 0.5
                        color: buttonMouse.containsMouse && usable ? Qt.lighter(Theme.accent, 1.1) : Theme.accent
                        function activate() {
                            if (!usable) return
                            Access.setCaffeineMinutes(parseInt(caffeinePage.minutes, 10))
                            root.showPage("")
                        }
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("Start")
                            color: "white"
                            font.pixelSize: Theme.fontBody
                        }
                        MouseArea {
                            id: buttonMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: startButton.activate()
                        }
                    }
                }
            }
        }

        // At the bottom: battery on the left, Settings on the right.
        Rectangle {
            id: footer
            visible: root.page === ""
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
            height: 48
            color: Theme.footer
            bottomLeftRadius: Theme.radiusOverlay - 1
            bottomRightRadius: Theme.radiusOverlay - 1

            Row {
                visible: Status.batteryPresent
                anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
                spacing: 8
                Image {
                    width: 18
                    height: 18
                    source: Theme.icons + encodeURIComponent(Status.batteryCharging ? "battery-good-charging" : "battery-good")
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Status.batteryPercent + "%"
                    color: Theme.text
                    font.pixelSize: Theme.fontCaption
                }
            }
            Item {
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 36
                height: 36
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius
                    color: Theme.subtleHover
                    opacity: settingsMouse.containsMouse ? 1 : 0
                }
                Image {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    source: Theme.icons + "configure"
                    sourceSize: Qt.size(width, height)
                }
                MouseArea {
                    id: settingsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.openSettings("settings")
                }
            }
        }
    }
}
