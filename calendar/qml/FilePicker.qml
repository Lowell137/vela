// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs

// The system's Open and Save dialogs (the portal's) for .ics files. Created
// the first time one is needed: Qt Quick Dialogs isn't loaded at startup.
// `done(file)` gets the chosen file's URL, nothing if cancelled.
FileDialog {
    id: picker
    property var done: null

    function pick(save, callback) {
        fileMode = save ? FileDialog.SaveFile : FileDialog.OpenFile
        title = save ? qsTr("Export events") : qsTr("Import events")
        done = callback
        open()
    }
    defaultSuffix: "ics"
    nameFilters: [qsTr("Calendar files (*.ics)"), qsTr("All files (*)")]
    onAccepted: { const c = done; done = null; if (c) c(selectedFile) }
    onRejected: done = null
}
