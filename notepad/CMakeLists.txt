# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Notepad (vela-notepad): built like the File Explorer, Qt Quick without Qt
# Quick Controls at startup. It reuses the Explorer's window pieces (menus,
# dialogs, tool buttons, scroll bar) and the shell's menu panel.
find_package(Qt6 6.7 REQUIRED COMPONENTS DBus Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# Printing through the portal, for Notepad and Photos.
add_library(vela-portalprint STATIC
    src/portalprint.cpp
    src/portalprint.h)
target_include_directories(vela-portalprint PUBLIC src)
target_compile_options(vela-portalprint PRIVATE -Wall -Wextra)
target_link_libraries(vela-portalprint PUBLIC Qt6::DBus Qt6::Gui)

# Notepad's C++ code, shared by the app and its tests.
add_library(vela-notepad-core OBJECT
    src/app.h
    src/document.cpp
    src/document.h)
target_include_directories(vela-notepad-core PUBLIC src)
target_compile_options(vela-notepad-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-notepad-core PUBLIC
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    vela-portalprint)

qt_add_executable(vela-notepad src/main.cpp)

set(NOTEPAD_QML_FILES
    qml/Main.qml
    qml/Editor.qml
    qml/FindBar.qml
    qml/GoToDialog.qml
    qml/FontDialog.qml
    qml/FilePicker.qml
    qml/StatusBar.qml
    qml/SettingsPage.qml
    qml/WindowButton.qml
    qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Toggle.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS NOTEPAD_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-notepad
    URI Vela.Notepad
    VERSION 1.0
    QML_FILES ${NOTEPAD_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-notepad "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-notepad PRIVATE -Wall -Wextra)
target_link_libraries(vela-notepad PRIVATE
    vela-notepad-core
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): a file read and written back keeps its bytes.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-notepad-test tests/document.cpp)
    target_compile_options(vela-notepad-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-notepad-test PRIVATE vela-notepad-core Qt6::Test)
    add_test(NAME notepad-document COMMAND vela-notepad-test)
    set_tests_properties(notepad-document PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS vela-notepad RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-notepad.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-notepad_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-notepad SOURCE_TARGETS vela-notepad vela-notepad-core vela-portalprint
    TS_FILES i18n/vela-notepad_it.ts i18n/vela-notepad_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-notepad_lupdate
    LRELEASE_TARGET vela-notepad_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
