// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtCore
import Vela.Controls

// The Calculator's window, like Windows 11's: its own title bar, the mode
// (Standard or Scientific), the number with what led to it above, the memory
// keys, the keypad. The history and the memory are at the right when the
// window is wide enough, otherwise over the keys when their button is pressed.
Window {
    id: root
    flags: Qt.Window | Qt.FramelessWindowHint // no title bar from Vela
    width: 660
    height: 540
    minimumWidth: 320
    minimumHeight: 470
    visible: true
    color: Theme.background
    title: qsTr("Calculator")

    onActiveChanged: Theme.windowActive = active

    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { maximized ? showNormal() : showMaximized() }

    readonly property bool wide: width >= 620
    property bool historyOpen: false // only when not wide
    property string panelTab: "history" // or "memory"
    property bool second: false // Scientific's 2nd: the other function of some keys

    // The mode is remembered across launches.
    Settings {
        id: settings
        property bool scientific: false
    }
    function setScientific(on) {
        settings.scientific = on
        Calc.scientific = on
        second = false
    }
    Component.onCompleted: Calc.scientific = settings.scientific
    function modeEntries() {
        return [
            { text: qsTr("&Standard"), shortcut: "Alt+1", radio: true, checked: !Calc.scientific, action: () => setScientific(false) },
            { text: qsTr("S&cientific"), shortcut: "Alt+2", radio: true, checked: Calc.scientific, action: () => setScientific(true) }
        ]
    }
    function nextAngleUnit() {
        const units = ["DEG", "RAD", "GRAD"]
        Calc.angleUnit = units[(units.indexOf(Calc.angleUnit) + 1) % 3]
    }
    Shortcut { sequence: "Alt+1"; onActivated: root.setScientific(false) }
    Shortcut { sequence: "Alt+2"; onActivated: root.setScientific(true) }

    // Standard's keys, and Scientific's: {text, press} or {digit}; `big` for
    // the operators, `strong` in the digits' grey, `always` usable after an
    // error.
    readonly property var standardKeys: [
        { text: "%", press: () => Calc.percent() },
        { text: "CE", press: () => Calc.clearEntry(), always: true },
        { text: "C", press: () => Calc.clear(), always: true },
        { text: "⌫", press: () => Calc.backspace(), always: true },
        { text: "1/x", press: () => Calc.function("inv") },
        { text: "x²", press: () => Calc.function("sqr") },
        { text: "√x", press: () => Calc.function("sqrt") },
        { text: "÷", press: () => Calc.operation("÷"), big: true },
        { digit: 7 }, { digit: 8 }, { digit: 9 },
        { text: "×", press: () => Calc.operation("×"), big: true },
        { digit: 4 }, { digit: 5 }, { digit: 6 },
        { text: "−", press: () => Calc.operation("−"), big: true },
        { digit: 1 }, { digit: 2 }, { digit: 3 },
        { text: "+", press: () => Calc.operation("+"), big: true },
        { text: "+/−", press: () => Calc.negate(), strong: true },
        { digit: 0 },
        { text: Qt.locale().decimalPoint, press: () => Calc.point(), strong: true, always: true },
        { text: "=", press: () => Calc.equals(), big: true, accent: true, always: true }
    ]
    readonly property var scientificKeys: [
        { text: "2nd", press: () => root.second = !root.second, checked: second, always: true },
        { text: "π", press: () => Calc.constant("pi"), always: true },
        { text: "e", press: () => Calc.constant("e"), always: true },
        { text: "C", press: () => Calc.clear(), always: true },
        { text: "⌫", press: () => Calc.backspace(), always: true },
        second ? { text: "x³", press: () => Calc.function("cube") } : { text: "x²", press: () => Calc.function("sqr") },
        { text: "1/x", press: () => Calc.function("inv") },
        { text: "|x|", press: () => Calc.function("abs") },
        { text: "n!", press: () => Calc.function("fact") },
        { text: "mod", press: () => Calc.operation("mod") },
        second ? { text: "∛x", press: () => Calc.function("cbrt") } : { text: "√x", press: () => Calc.function("sqrt") },
        { text: Calc.openParentheses > 0 ? "( " + Calc.openParentheses : "(", press: () => Calc.openParenthesis() },
        { text: ")", press: () => Calc.closeParenthesis() },
        { text: "CE", press: () => Calc.clearEntry(), always: true },
        { text: "÷", press: () => Calc.operation("÷"), big: true },
        second ? { text: "ʸ√x", press: () => Calc.operation("yroot") } : { text: "xʸ", press: () => Calc.operation("^") },
        { digit: 7 }, { digit: 8 }, { digit: 9 },
        { text: "×", press: () => Calc.operation("×"), big: true },
        second ? { text: "2ˣ", press: () => Calc.function("exp2") } : { text: "10ˣ", press: () => Calc.function("exp10") },
        { digit: 4 }, { digit: 5 }, { digit: 6 },
        { text: "−", press: () => Calc.operation("−"), big: true },
        { text: "log", press: () => Calc.function("log") },
        { digit: 1 }, { digit: 2 }, { digit: 3 },
        { text: "+", press: () => Calc.operation("+"), big: true },
        second ? { text: "eˣ", press: () => Calc.function("expe") } : { text: "ln", press: () => Calc.function("ln") },
        { text: "+/−", press: () => Calc.negate(), strong: true },
        { digit: 0 },
        { text: Qt.locale().decimalPoint, press: () => Calc.point(), strong: true, always: true },
        { text: "=", press: () => Calc.equals(), big: true, accent: true, always: true }
    ]

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
            source: "image://appicon/accessories-calculator"
            sourceSize: Qt.size(width, height)
        }
        Text {
            x: 42
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            opacity: root.active ? 1 : 0.5
            font.pixelSize: Theme.fontCaption
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

    // --- the keyboard, as in Windows ---
    Item {
        id: content
        anchors { left: parent.left; right: parent.right; top: strip.bottom; bottom: parent.bottom }
        focus: true
        Keys.onPressed: event => {
            event.accepted = true
            if (event.modifiers & Qt.ControlModifier) {
                switch (event.key) {
                case Qt.Key_C: Calc.copy(); break
                case Qt.Key_V: Calc.paste(); break
                case Qt.Key_M: Calc.memory("MS"); break
                case Qt.Key_R: Calc.memory("MR"); break
                case Qt.Key_L: Calc.memory("MC"); break
                case Qt.Key_P: Calc.memory("M+"); break
                case Qt.Key_Q: Calc.memory("M-"); break
                case Qt.Key_H: root.historyOpen = !root.historyOpen; break
                default: event.accepted = false
                }
                return
            }
            switch (event.key) {
            case Qt.Key_Backspace: Calc.backspace(); break
            case Qt.Key_Delete: Calc.clearEntry(); break
            case Qt.Key_Escape: Calc.clear(); break
            case Qt.Key_Return:
            case Qt.Key_Enter: Calc.equals(); break
            case Qt.Key_F9: Calc.negate(); break
            default: event.accepted = Calc.key(event.text)
            }
        }

        // --- the calculator ---
        Item {
            id: pad
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: root.wide ? parent.width - side.width : parent.width

            ToolButton {
                id: modeButton
                x: 6
                y: 6
                icon: "application-menu"
                tooltip: qsTr("Mode")
                onClicked: {
                    const p = mapToItem(null, 0, height)
                    menuLayer.open(root.modeEntries(), p.x, p.y + 2, {})
                }
            }
            Text {
                x: 48
                height: 44
                verticalAlignment: Text.AlignVCenter
                text: Calc.scientific ? qsTr("Scientific") : qsTr("Standard")
                color: Theme.text
                font.pixelSize: Theme.fontSubtitle
                font.weight: Font.DemiBold
            }
            ToolButton {
                anchors { right: parent.right; rightMargin: 6 }
                y: 6
                visible: !root.wide
                icon: "view-history"
                tooltip: qsTr("History and memory (Ctrl+H)")
                checked: root.historyOpen
                onClicked: root.historyOpen = !root.historyOpen
            }

            Text {
                id: expression
                anchors { left: parent.left; right: parent.right; leftMargin: 16; rightMargin: 16 }
                y: 52
                height: 20
                horizontalAlignment: Text.AlignRight
                text: Calc.expression
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                elide: Text.ElideLeft
            }
            Text {
                id: display
                anchors { left: parent.left; right: parent.right; top: expression.bottom; leftMargin: 12; rightMargin: 12 }
                height: 68
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                text: Calc.display
                color: Theme.text
                font.pixelSize: 46
                font.weight: Font.DemiBold
                fontSizeMode: Text.HorizontalFit // long numbers and the error messages shrink
                minimumPixelSize: 14
            }

            Row {
                id: memoryRow
                anchors { left: parent.left; right: parent.right; top: display.bottom; leftMargin: 4; rightMargin: 4 }
                height: 34
                Repeater {
                    model: [
                        { key: "MC", tip: qsTr("Clear memory (Ctrl+L)"), needs: true },
                        { key: "MR", tip: qsTr("Recall memory (Ctrl+R)"), needs: true },
                        { key: "M+", tip: qsTr("Add to memory (Ctrl+P)") },
                        { key: "M-", tip: qsTr("Subtract from memory (Ctrl+Q)") },
                        { key: "MS", tip: qsTr("Store in memory (Ctrl+M)") }
                    ]
                    delegate: ToolButton {
                        required property var modelData
                        width: memoryRow.width / 5
                        text: modelData.key.replace("-", "−")
                        tooltip: modelData.tip
                        usable: !Calc.error && (!modelData.needs || Calc.memoryList.length > 0)
                        onClicked: Calc.memory(modelData.key)
                    }
                }
            }

            // Scientific: the angle's unit and the trigonometry.
            Row {
                id: trigRow
                anchors { left: parent.left; right: parent.right; top: memoryRow.bottom; leftMargin: 4; rightMargin: 4 }
                height: visible ? 34 : 0
                visible: Calc.scientific
                Repeater {
                    model: [
                        { text: Calc.angleUnit, tip: qsTr("Angle unit"), press: () => root.nextAngleUnit(), always: true },
                        { text: root.second ? "sin⁻¹" : "sin", press: () => Calc.function(root.second ? "asin" : "sin") },
                        { text: root.second ? "cos⁻¹" : "cos", press: () => Calc.function(root.second ? "acos" : "cos") },
                        { text: root.second ? "tan⁻¹" : "tan", press: () => Calc.function(root.second ? "atan" : "tan") }
                    ]
                    delegate: ToolButton {
                        required property var modelData
                        width: trigRow.width / 4
                        text: modelData.text
                        tooltip: modelData.tip || ""
                        usable: !Calc.error || !!modelData.always
                        onClicked: modelData.press()
                    }
                }
            }

            Grid {
                id: keys
                anchors { left: parent.left; right: parent.right; top: trigRow.bottom; bottom: parent.bottom; margins: 4 }
                columns: Calc.scientific ? 5 : 4
                spacing: 2
                readonly property int rows: Calc.scientific ? 7 : 6
                readonly property real keyWidth: (width - (columns - 1) * spacing) / columns
                readonly property real keyHeight: (height - (rows - 1) * spacing) / rows
                // Windows' two greys: the digits stand out from the rest.
                readonly property color digitColor: Theme.light ? Theme.control : Theme.controlHover
                readonly property color otherColor: Theme.light ? Theme.controlHover : Theme.control

                Repeater {
                    model: Calc.scientific ? root.scientificKeys : root.standardKeys
                    delegate: Rectangle {
                        id: key
                        required property var modelData
                        readonly property bool isDigit: modelData.digit !== undefined
                        // After an error only what starts over works, like Windows.
                        readonly property bool usable: isDigit || !!modelData.always || !Calc.error
                        width: keys.keyWidth
                        height: keys.keyHeight
                        radius: Theme.radius
                        color: modelData.accent || modelData.checked ? (keyMouse.containsMouse ? Theme.accentFillHover : Theme.accentFill)
                            : isDigit || modelData.strong ? keys.digitColor : keys.otherColor
                        border.width: 1
                        border.color: Theme.controlStroke
                        opacity: !usable ? 0.4 : keyMouse.pressed ? 0.7 : 1
                        Rectangle {
                            anchors.fill: parent
                            radius: parent.radius
                            color: Theme.subtleHover
                            visible: keyMouse.containsMouse && !key.modelData.accent && !key.modelData.checked
                        }
                        Text {
                            anchors.centerIn: parent
                            text: key.isDigit ? key.modelData.digit : key.modelData.text
                            color: key.modelData.accent || key.modelData.checked ? Theme.accentText : Theme.text
                            font.pixelSize: key.modelData.big ? 22 : key.isDigit ? Theme.fontBodyLarge : Theme.fontBody
                            font.weight: key.isDigit ? Font.DemiBold : Font.Normal
                        }
                        MouseArea {
                            id: keyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: key.usable
                            onClicked: key.isDigit ? Calc.digit(key.modelData.digit) : key.modelData.press()
                        }
                    }
                }
            }
        }

        // --- the history ---
        Rectangle {
            id: side
            visible: root.wide || root.historyOpen
            x: root.wide ? parent.width - width : 0
            y: root.wide ? 0 : memoryRow.y
            width: root.wide ? 320 : parent.width
            height: parent.height - y
            color: root.wide ? "transparent" : Theme.flyout
            topLeftRadius: root.wide ? 0 : Theme.radiusOverlay
            topRightRadius: topLeftRadius

            MouseArea { anchors.fill: parent } // the keys below aren't pressed through it

            Row {
                id: panelTabs
                x: 8
                height: 44
                spacing: 4
                Repeater {
                    model: [{ tab: "history", text: qsTr("History") }, { tab: "memory", text: qsTr("Memory") }]
                    delegate: Item {
                        id: panelTabButton
                        required property var modelData
                        readonly property bool chosen: root.panelTab === modelData.tab
                        width: tabText.implicitWidth + 16
                        height: parent.height
                        Text {
                            id: tabText
                            anchors.centerIn: parent
                            text: panelTabButton.modelData.text
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.weight: Font.DemiBold
                            opacity: panelTabButton.chosen ? 1 : 0.6
                        }
                        Rectangle {
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6 }
                            width: 20
                            height: 3
                            radius: 1.5
                            color: Theme.accentFill
                            visible: panelTabButton.chosen
                        }
                        MouseArea { anchors.fill: parent; onClicked: root.panelTab = panelTabButton.modelData.tab }
                    }
                }
            }
            Text {
                anchors { left: parent.left; top: panelTabs.bottom; leftMargin: 16; topMargin: 8 }
                visible: list.count === 0
                text: root.panelTab === "history" ? qsTr("There's no history yet.") : qsTr("There's nothing saved in memory.")
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
            }
            ListView {
                id: list
                anchors { left: parent.left; right: parent.right; top: panelTabs.bottom; bottom: clearButton.top; bottomMargin: 4 }
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.panelTab === "history" ? Calc.history : Calc.memoryList
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool isMemory: root.panelTab === "memory"
                    x: 4
                    width: list.width - 18
                    height: 64
                    radius: Theme.radius
                    color: rowMouse.pressed ? Theme.subtlePressed : rowMouse.containsMouse || rowHover.hovered ? Theme.subtleHover : "transparent"
                    Text {
                        anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                        y: 8
                        visible: !row.isMemory
                        horizontalAlignment: Text.AlignRight
                        text: row.isMemory ? "" : row.modelData.expression
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideLeft
                    }
                    Text {
                        anchors { left: parent.left; right: parent.right; leftMargin: 12; rightMargin: 12 }
                        y: row.isMemory ? 6 : 30
                        horizontalAlignment: Text.AlignRight
                        text: row.isMemory ? row.modelData : row.modelData.result
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                        font.weight: Font.DemiBold
                        elide: Text.ElideLeft
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            if (row.isMemory) Calc.memory("MR", row.index)
                            else Calc.recall(row.index)
                            root.historyOpen = false
                        }
                    }
                    HoverHandler { id: rowHover }
                    // A stored value's own keys, while the pointer is on it.
                    Row {
                        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 4; bottomMargin: 2 }
                        visible: row.isMemory && rowHover.hovered
                        Repeater {
                            model: ["MC", "M+", "M-"]
                            delegate: ToolButton {
                                required property string modelData
                                height: 26
                                text: modelData.replace("-", "−")
                                onClicked: Calc.memory(modelData, row.index)
                            }
                        }
                    }
                }
            }
            ScrollBar {
                flickable: list
                anchors { right: list.right; top: list.top; bottom: list.bottom }
            }
            ToolButton {
                id: clearButton
                anchors { right: parent.right; bottom: parent.bottom; margins: 8 }
                visible: list.count > 0
                icon: "edit-delete"
                tooltip: root.panelTab === "history" ? qsTr("Clear all history") : qsTr("Clear all memory")
                onClicked: root.panelTab === "history" ? Calc.clearHistory() : Calc.memory("MC")
            }
        }
    }

    MenuLayer {
        id: menuLayer
        anchors.fill: parent
        z: 100
        onVisibleChanged: if (!visible) content.forceActiveFocus()
    }

    ResizeEdges {}
}
