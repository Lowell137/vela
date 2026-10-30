# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Event Viewer (vela-eventviewer): built like the Task Manager, whose title bar
# pieces and widgets it reuses. It reads systemd's journal through journalctl.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# What journalctl prints taken apart and the list that holds it, shared by the
# app and its tests.
add_library(vela-eventviewer-core OBJECT
    src/eventmodel.cpp
    src/eventmodel.h
    src/journal.cpp
    src/journal.h
    src/reader.cpp
    src/reader.h)
target_include_directories(vela-eventviewer-core PUBLIC src)
target_compile_options(vela-eventviewer-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-eventviewer-core PUBLIC Qt6::Gui)

qt_add_executable(vela-eventviewer src/main.cpp)

set(EVENTVIEWER_QML_FILES
    qml/Main.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS EVENTVIEWER_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-eventviewer
    URI Vela.EventViewer
    VERSION 1.0
    QML_FILES ${EVENTVIEWER_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-eventviewer "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-eventviewer PRIVATE -Wall -Wextra)
target_link_libraries(vela-eventviewer PRIVATE
    vela-eventviewer-core
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): journalctl's lines, the arguments and the capped list.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-eventviewer-test tests/eventviewer.cpp)
    target_compile_options(vela-eventviewer-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-eventviewer-test PRIVATE vela-eventviewer-core Qt6::Test)
    add_test(NAME eventviewer-journal COMMAND vela-eventviewer-test)
endif()

install(TARGETS vela-eventviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-eventviewer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-eventviewer_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target vela-eventviewer_lupdate
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-eventviewer SOURCE_TARGETS vela-eventviewer
    TS_FILES i18n/vela-eventviewer_it.ts i18n/vela-eventviewer_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-eventviewer_lupdate
    LRELEASE_TARGET vela-eventviewer_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
