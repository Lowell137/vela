# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Task Manager (vela-taskmanager): built like Notepad, whose title bar pieces
# it reuses, with the Explorer's menus, dialog, buttons and scroll bar and
# Settings' toggle. It reads /proc and /sys itself; services are systemd's.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Network Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# What's read and how the lists follow, shared by the app and its tests.
add_library(vela-taskmanager-core OBJECT
    src/procfs.cpp
    src/procfs.h
    src/rowmodel.cpp
    src/rowmodel.h)
target_include_directories(vela-taskmanager-core PUBLIC src)
target_compile_options(vela-taskmanager-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-taskmanager-core PUBLIC Qt6::Core)

qt_add_executable(vela-taskmanager
    src/main.cpp
    src/graph.cpp
    src/graph.h
    src/system.cpp
    src/system.h)

set(TASKMANAGER_QML_FILES
    qml/Main.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS TASKMANAGER_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-taskmanager
    URI Vela.TaskManager
    VERSION 1.0
    QML_FILES ${TASKMANAGER_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-taskmanager "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-taskmanager PRIVATE -Wall -Wextra)
target_link_libraries(vela-taskmanager PRIVATE
    vela-taskmanager-core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): /proc's text taken apart, and lists that keep their place.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-taskmanager-test tests/procfs.cpp)
    target_compile_options(vela-taskmanager-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-taskmanager-test PRIVATE vela-taskmanager-core Qt6::Test)
    add_test(NAME taskmanager-procfs COMMAND vela-taskmanager-test)
endif()

install(TARGETS vela-taskmanager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-taskmanager.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-taskmanager_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-taskmanager SOURCE_TARGETS vela-taskmanager
    TS_FILES i18n/vela-taskmanager_it.ts i18n/vela-taskmanager_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-taskmanager_lupdate
    LRELEASE_TARGET vela-taskmanager_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
