# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Terminal (vela-terminal): built like Notepad, whose title bar pieces it
# reuses, with the Explorer's menus and buttons and Settings' slider. libvterm
# understands what the programs write; the drawing and the window are ours.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(VTERM REQUIRED IMPORTED_TARGET vterm)

qt_standard_project_setup(REQUIRES 6.7)

# What a program shows and the lines it leaves behind, shared with the tests.
add_library(vela-terminal-core OBJECT
    src/graphics.cpp
    src/graphics.h
    src/terminal.cpp
    src/terminal.h
    src/vt.cpp
    src/vt.h)
target_include_directories(vela-terminal-core PUBLIC src)
target_compile_options(vela-terminal-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-terminal-core PUBLIC Qt6::Gui Qt6::Quick PkgConfig::VTERM)

qt_add_executable(vela-terminal
    src/main.cpp
    src/host.h)

set(TERMINAL_QML_FILES
    qml/Main.qml
    qml/Pane.qml
    qml/Split.qml
    qml/SettingsPage.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Slider.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS TERMINAL_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-terminal
    URI Vela.Terminal
    VERSION 1.0
    QML_FILES ${TERMINAL_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-terminal "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-terminal PRIVATE -Wall -Wextra)
target_link_libraries(vela-terminal PRIVATE
    vela-terminal-core
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): what's written is shown, kept, copied; keys reach the program.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-terminal-test tests/vt.cpp)
    target_compile_options(vela-terminal-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-terminal-test PRIVATE vela-terminal-core Qt6::Test)
    add_test(NAME terminal-vt COMMAND vela-terminal-test)
    # The keys, through the item itself, with a real pty (offscreen).
    qt_add_executable(vela-terminal-keys tests/keys.cpp)
    target_compile_options(vela-terminal-keys PRIVATE -Wall -Wextra)
    target_link_libraries(vela-terminal-keys PRIVATE vela-terminal-core Qt6::Test)
    add_test(NAME terminal-keys COMMAND vela-terminal-keys)
    # The kitty graphics protocol: commands are split out of the text, pictures placed.
    qt_add_executable(vela-terminal-graphics tests/graphics.cpp)
    target_compile_options(vela-terminal-graphics PRIVATE -Wall -Wextra)
    target_link_libraries(vela-terminal-graphics PRIVATE vela-terminal-core Qt6::Test)
    add_test(NAME terminal-graphics COMMAND vela-terminal-graphics)
endif()

install(TARGETS vela-terminal RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-terminal.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-terminal_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-terminal SOURCE_TARGETS vela-terminal
    TS_FILES i18n/vela-terminal_it.ts i18n/vela-terminal_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-terminal_lupdate
    LRELEASE_TARGET vela-terminal_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
