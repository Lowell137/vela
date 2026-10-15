# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Sticky Notes (vela-stickynotes): built like the Task Manager, with Notepad's
# title bar pieces and the Explorer's menus and dialog. The notes are one JSON file.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Network Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# The notes and their file, shared by the app and its tests.
add_library(vela-stickynotes-core OBJECT
    src/notestore.cpp
    src/notestore.h)
target_include_directories(vela-stickynotes-core PUBLIC src)
target_compile_options(vela-stickynotes-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-stickynotes-core PUBLIC Qt6::Core)

qt_add_executable(vela-stickynotes
    src/main.cpp
    src/instance.cpp
    src/instance.h)

set(STICKYNOTES_QML_FILES
    qml/Main.qml
    qml/NoteWindow.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS STICKYNOTES_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-stickynotes
    URI Vela.StickyNotes
    VERSION 1.0
    QML_FILES ${STICKYNOTES_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-stickynotes "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-stickynotes PRIVATE -Wall -Wextra)
target_link_libraries(vela-stickynotes PRIVATE
    vela-stickynotes-core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): the notes file, ids, debounce and the list filter.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-stickynotes-test tests/notestore.cpp)
    target_compile_options(vela-stickynotes-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-stickynotes-test PRIVATE vela-stickynotes-core Qt6::Test)
    add_test(NAME stickynotes-store COMMAND vela-stickynotes-test)
endif()

install(TARGETS vela-stickynotes RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-stickynotes.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, the Italian
# translation is in i18n/ and ends up in the executable (:/i18n/vela-stickynotes_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target vela-stickynotes_lupdate
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-stickynotes SOURCE_TARGETS vela-stickynotes
    TS_FILES i18n/vela-stickynotes_it.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-stickynotes_lupdate
    LRELEASE_TARGET vela-stickynotes_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
