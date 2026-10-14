// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs

// The system's Open and Save dialogs (the portal's). Created the first time
// one is needed: Qt Quick Dialogs isn't loaded at startup. `done(files)` gets
// the chosen files, none if cancelled.
FileDialog {
    id: picker
    property var done: null

    function pick(save, folder, name, callback) {
        fileMode = save ? FileDialog.SaveFile : FileDialog.OpenFiles
        title = save ? qsTr("Save as") : qsTr("Open")
        if (folder.toString() !== "") currentFolder = folder
        if (save && name !== "") selectedFile = currentFolder + "/" + encodeURIComponent(name)
        done = callback
        open()
    }
    function finish(files) {
        const callback = done
        done = null
        if (callback) callback(files)
    }

    nameFilters: [qsTr("Text documents (*.txt)"), qsTr("All files (*)")]
    onAccepted: finish(fileMode === FileDialog.SaveFile ? [selectedFile] : selectedFiles)
    onRejected: finish([])
}
