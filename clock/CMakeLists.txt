# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Clock (vela-clock): built like the Task Manager, whose pieces it reuses.
# Rings through org.freedesktop.Notifications and paplay.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui DBus Network Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# The time arithmetic and stores, shared by the app and its tests.
add_library(vela-clock-core OBJECT
    src/clockmath.cpp
    src/clockmath.h)
target_include_directories(vela-clock-core PUBLIC src)
target_compile_options(vela-clock-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-clock-core PUBLIC Qt6::Core)

qt_add_executable(vela-clock
    src/main.cpp
    src/clock.cpp
    src/clock.h)

set(CLOCK_QML_FILES
    qml/Main.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Toggle.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Slider.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS CLOCK_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-clock
    URI Vela.Clock
    VERSION 1.0
    QML_FILES ${CLOCK_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-clock "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-clock PRIVATE -Wall -Wextra)
target_link_libraries(vela-clock PRIVATE
    vela-clock-core
    Qt6::DBus
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): when alarms ring, how timers count, the stopwatch's laps, the world's clocks.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-clock-test tests/clockmath.cpp)
    target_compile_options(vela-clock-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-clock-test PRIVATE vela-clock-core Qt6::Test)
    add_test(NAME clock-math COMMAND vela-clock-test)
endif()

install(TARGETS vela-clock RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-clock.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-clock_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-clock SOURCE_TARGETS vela-clock
    TS_FILES i18n/vela-clock_it.ts i18n/vela-clock_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-clock_lupdate
    LRELEASE_TARGET vela-clock_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
