// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import Vela.Files.Backend

// File Explorer's search options narrow what the folder shows, and clearing
// them brings everything back.
Item {
    FolderModel { id: folder; path: TestFolder }

    TestCase {
        name: "SearchFilters"
        function test_filters_narrow_and_clear() {
            tryCompare(folder, "count", 300, 5000)
            folder.filterKind = "folder"
            compare(folder.count, 0)
            folder.filterKind = ""
            compare(folder.count, 300)
            folder.filterSize = "gigantic"
            compare(folder.count, 0)
            folder.filterSize = ""
            folder.filterDate = "today"
            compare(folder.count, 300) // made just now
            folder.filterDate = "lastyear"
            compare(folder.count, 0)
            folder.filterDate = ""
            compare(folder.count, 300)
        }
    }
}
