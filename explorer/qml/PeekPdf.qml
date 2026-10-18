// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Pdf

// First page of a PDF for the quick preview; only built when QtPdf is there.
Item {
    property string path: ""
    property real detail: 1 // the zoom it is rendered for
    PdfDocument { id: doc; source: path !== "" ? "file://" + encodeURI(path).replace(/#/g, "%23").replace(/\?/g, "%3F") : "" }
    PdfPageImage {
        anchors.fill: parent
        document: doc
        currentFrame: 0
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(width * parent.detail, height * parent.detail)
        asynchronous: true
    }
}
