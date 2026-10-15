# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Weather (vela-weather): built like Task Manager, whose title bar pieces it
# reuses, with the Explorer's buttons and scroll bar and Settings' toggle. The
# forecasts come from Open-Meteo (no key); the place is searched, never
# guessed from the IP.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Network Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# What the answers mean, the saved places and the cache, shared by the app
# and its tests (the network is a Fetcher, replaced in the tests).
add_library(vela-weather-core OBJECT
    src/weather.cpp
    src/weather.h
    src/weathermodel.cpp
    src/weathermodel.h)
target_include_directories(vela-weather-core PUBLIC src)
target_compile_options(vela-weather-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-weather-core PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(vela-weather src/main.cpp)

set(WEATHER_QML_FILES
    qml/Main.qml
    qml/Card.qml
    qml/ForecastPage.qml
    qml/PlacesPage.qml
    qml/SettingsPage.qml
    qml/HourlyChart.qml
    qml/SunArc.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Toggle.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml)

foreach(file IN LISTS WEATHER_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-weather
    URI Vela.Weather
    VERSION 1.0
    QML_FILES ${WEATHER_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-weather "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-weather PRIVATE -Wall -Wextra)
target_link_libraries(vela-weather PRIVATE
    vela-weather-core
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): parsers on saved real answers, tables, units, places,
# cache and the model with a fake fetcher. No network.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-weather-test tests/weathertest.cpp)
    target_compile_options(vela-weather-test PRIVATE -Wall -Wextra)
    target_compile_definitions(vela-weather-test PRIVATE
        WEATHER_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    target_link_libraries(vela-weather-test PRIVATE vela-weather-core Qt6::Test)
    add_test(NAME weather COMMAND vela-weather-test)
endif()

install(TARGETS vela-weather RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-weather.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (see shell/src/language.h).
# After changing texts: cmake --build build --target vela-weather_lupdate
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-weather SOURCE_TARGETS vela-weather vela-weather-core
    TS_FILES i18n/vela-weather_it.ts i18n/vela-weather_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-weather_lupdate
    LRELEASE_TARGET vela-weather_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
