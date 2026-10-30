# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Paint (vela-paint): built like Notepad, whose title bar pieces and file
# picker it reuses, with the Explorer's menus, dialog, buttons and scroll bar
# and Settings' slider and toggle.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# The picture and its tools, shared by the app and its tests.
add_library(vela-paint-core OBJECT
    src/canvas.cpp
    src/canvas.h)
target_include_directories(vela-paint-core PUBLIC src)
target_compile_options(vela-paint-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-paint-core PUBLIC Qt6::Gui Qt6::Quick vela-imageformats)

qt_add_executable(vela-paint src/main.cpp)

set(PAINT_QML_FILES
    qml/Main.qml
    qml/ResizeDialog.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FontDialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS PAINT_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-paint
    URI Vela.Paint
    VERSION 1.0
    QML_FILES ${PAINT_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-paint "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-paint PRIVATE -Wall -Wextra)
target_link_libraries(vela-paint PRIVATE
    vela-paint-core
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): what the tools leave on the picture.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-paint-test tests/canvas.cpp)
    target_compile_options(vela-paint-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-paint-test PRIVATE vela-paint-core Qt6::Test)
    add_test(NAME paint-canvas COMMAND vela-paint-test)
    set_tests_properties(paint-canvas PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS vela-paint RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-paint.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-paint_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-paint SOURCE_TARGETS vela-paint vela-paint-core
    TS_FILES i18n/vela-paint_it.ts i18n/vela-paint_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-paint_lupdate
    LRELEASE_TARGET vela-paint_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
