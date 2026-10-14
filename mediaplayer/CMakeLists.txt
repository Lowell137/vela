# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Media Player (vela-mediaplayer): built like Notepad, whose title bar pieces
# and file picker it reuses, with the Explorer's menus, dialog and buttons and
# Settings' slider. mpv (libmpv) plays.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui OpenGL Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

qt_standard_project_setup(REQUIRES 6.7)

# The folder's list of files, shared by the app and its tests.
add_library(vela-mediaplayer-core OBJECT
    src/playlist.cpp
    src/playlist.h)
target_include_directories(vela-mediaplayer-core PUBLIC src)
target_compile_options(vela-mediaplayer-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-mediaplayer-core PUBLIC Qt6::Core)

qt_add_executable(vela-mediaplayer
    src/main.cpp
    src/mpvplayer.cpp
    src/mpvplayer.h)

set(MEDIAPLAYER_QML_FILES
    qml/Main.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Slider.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS MEDIAPLAYER_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-mediaplayer
    URI Vela.MediaPlayer
    VERSION 1.0
    QML_FILES ${MEDIAPLAYER_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-mediaplayer "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-mediaplayer PRIVATE -Wall -Wextra)
target_link_libraries(vela-mediaplayer PRIVATE
    vela-mediaplayer-core
    PkgConfig::MPV
    Qt6::OpenGL
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): which files of a folder are played, in what order.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-mediaplayer-test tests/playlist.cpp)
    target_compile_options(vela-mediaplayer-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-mediaplayer-test PRIVATE vela-mediaplayer-core Qt6::Test)
    add_test(NAME mediaplayer-playlist COMMAND vela-mediaplayer-test)
endif()

install(TARGETS vela-mediaplayer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-mediaplayer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-mediaplayer_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-mediaplayer SOURCE_TARGETS vela-mediaplayer
    TS_FILES i18n/vela-mediaplayer_it.ts i18n/vela-mediaplayer_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-mediaplayer_lupdate
    LRELEASE_TARGET vela-mediaplayer_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
