# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Calendar (vela-calendar): built like the Task Manager, whose title bar
# pieces it reuses, with the Explorer's menus, dialog, buttons and scroll bar
# and Settings' toggle. Events live in a JSON file of its own; .ics is the
# way in and out. Nothing goes over the network.
find_package(Qt6 6.7 REQUIRED COMPONENTS DBus Gui Network Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# Events, repeats, .ics and the store: shared by the app and its tests.
add_library(vela-calendar-core OBJECT
    src/event.cpp
    src/event.h
    src/ics.cpp
    src/ics.h
    src/store.cpp
    src/store.h)
target_include_directories(vela-calendar-core PUBLIC src)
target_compile_options(vela-calendar-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-calendar-core PUBLIC Qt6::Core Qt6::DBus)

qt_add_executable(vela-calendar src/main.cpp)

set(CALENDAR_QML_FILES
    qml/Main.qml
    qml/Dates.js
    qml/MiniMonth.qml
    qml/MonthView.qml
    qml/TimeGrid.qml
    qml/EventEditor.qml
    qml/SettingsPage.qml
    qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS CALENDAR_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-calendar
    URI Vela.Calendar
    VERSION 1.0
    QML_FILES ${CALENDAR_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-calendar "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-calendar PRIVATE -Wall -Wextra)
target_link_libraries(vela-calendar PRIVATE
    vela-calendar-core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): repeats expanded, .ics read and written, the store's file.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-calendar-test tests/calendar.cpp)
    target_compile_options(vela-calendar-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-calendar-test PRIVATE vela-calendar-core Qt6::Test)
    add_test(NAME calendar-core COMMAND vela-calendar-test)
    # A zone with a daylight saving change, whatever the machine has.
    set_tests_properties(calendar-core PROPERTIES ENVIRONMENT "TZ=Europe/Rome;QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS vela-calendar RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-calendar.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-calendar_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target vela-calendar_lupdate
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-calendar SOURCE_TARGETS vela-calendar vela-calendar-core
    TS_FILES i18n/vela-calendar_it.ts i18n/vela-calendar_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-calendar_lupdate
    LRELEASE_TARGET vela-calendar_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
