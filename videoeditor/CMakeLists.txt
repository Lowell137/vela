# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Video Editor (vela-videoeditor): built like the Media Player, whose mpv
# item it shares (mediaplayer/src/mpvplayer.h), with Notepad's title bar
# pieces and file picker, the Explorer's menus, dialog and buttons and
# Settings' slider and toggle. ffmpeg and ffprobe do the rest.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui OpenGL Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup(REQUIRES 6.7)

# The timeline, the project file, the preview's EDL and the export's ffmpeg
# command, shared by the app and its tests.
add_library(vela-videoeditor-core OBJECT
    src/exportcommand.cpp
    src/exportcommand.h
    src/timeline.cpp
    src/timeline.h)
target_include_directories(vela-videoeditor-core PUBLIC src)
target_compile_options(vela-videoeditor-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-videoeditor-core PUBLIC Qt6::Core)

qt_add_executable(vela-videoeditor
    src/main.cpp
    src/exporter.cpp
    src/exporter.h
    src/mediapool.cpp
    src/mediapool.h
    ${PROJECT_SOURCE_DIR}/mediaplayer/src/mpvplayer.cpp
    ${PROJECT_SOURCE_DIR}/mediaplayer/src/mpvplayer.h)
target_include_directories(vela-videoeditor PRIVATE ${PROJECT_SOURCE_DIR}/mediaplayer/src)

set(VIDEOEDITOR_QML_FILES
    qml/Main.qml
    qml/Chip.qml
    qml/ExportDialog.qml
    qml/Inspector.qml
    qml/SettingsPage.qml
    qml/TimelineView.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS VIDEOEDITOR_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-videoeditor
    URI Vela.VideoEditor
    VERSION 1.0
    QML_FILES ${VIDEOEDITOR_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-videoeditor "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-videoeditor PRIVATE -Wall -Wextra)
target_link_libraries(vela-videoeditor PRIVATE
    vela-videoeditor-core
    PkgConfig::MPV
    Qt6::OpenGL
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): the timeline's edits, undo, EDL and project file; the
# export's ffmpeg command, and one real export when ffmpeg is there.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-videoeditor-test-timeline tests/timeline.cpp)
    target_compile_options(vela-videoeditor-test-timeline PRIVATE -Wall -Wextra)
    target_link_libraries(vela-videoeditor-test-timeline PRIVATE vela-videoeditor-core Qt6::Test)
    add_test(NAME videoeditor-timeline COMMAND vela-videoeditor-test-timeline)
    qt_add_executable(vela-videoeditor-test-export tests/export.cpp)
    target_compile_options(vela-videoeditor-test-export PRIVATE -Wall -Wextra)
    target_link_libraries(vela-videoeditor-test-export PRIVATE vela-videoeditor-core Qt6::Test)
    add_test(NAME videoeditor-export COMMAND vela-videoeditor-test-export)
    set_tests_properties(videoeditor-export PROPERTIES TIMEOUT 120)
endif()

install(TARGETS vela-videoeditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-videoeditor.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-videoeditor_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-videoeditor SOURCE_TARGETS vela-videoeditor vela-videoeditor-core
    TS_FILES i18n/vela-videoeditor_it.ts i18n/vela-videoeditor_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-videoeditor_lupdate
    LRELEASE_TARGET vela-videoeditor_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
