# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Calculator (vela-calculator): built like Notepad, whose title bar pieces it
# reuses, with the Explorer's menus, tool button and scroll bar.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# The arithmetic, shared by the app and its tests.
add_library(vela-calculator-core OBJECT
    src/engine.cpp
    src/engine.h)
target_include_directories(vela-calculator-core PUBLIC src)
target_compile_options(vela-calculator-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-calculator-core PUBLIC Qt6::Gui)

qt_add_executable(vela-calculator src/main.cpp)

set(CALCULATOR_QML_FILES
    qml/Main.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS CALCULATOR_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-calculator
    URI Vela.Calculator
    VERSION 1.0
    QML_FILES ${CALCULATOR_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-calculator "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-calculator PRIVATE -Wall -Wextra)
target_link_libraries(vela-calculator PRIVATE
    vela-calculator-core
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): what the keys compute.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-calculator-test tests/engine.cpp)
    target_compile_options(vela-calculator-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-calculator-test PRIVATE vela-calculator-core Qt6::Test)
    add_test(NAME calculator-engine COMMAND vela-calculator-test)
    set_tests_properties(calculator-engine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS vela-calculator RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-calculator.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, the Italian
# translation is in i18n/ and ends up in the executable (:/i18n/vela-calculator_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-calculator SOURCE_TARGETS vela-calculator vela-calculator-core
    TS_FILES i18n/vela-calculator_it.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-calculator_lupdate
    LRELEASE_TARGET vela-calculator_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
