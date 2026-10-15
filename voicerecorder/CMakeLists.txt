# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Sound Recorder (vela-voicerecorder): built like the Task Manager, whose
# title bar pieces and dialog it reuses. pw-record (PipeWire) records, mpv
# (libmpv, no picture) plays, ffmpeg trims and converts.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV IMPORTED_TARGET mpv)
if(NOT MPV_FOUND)
    message(STATUS "libmpv not found: vela-voicerecorder is not built")
    return()
endif()

qt_standard_project_setup(REQUIRES 6.7)

# WAV peaks and file names, shared by the app and its tests.
add_library(vela-voicerecorder-core OBJECT
    src/wav.cpp
    src/wav.h)
target_include_directories(vela-voicerecorder-core PUBLIC src)
target_compile_options(vela-voicerecorder-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-voicerecorder-core PUBLIC Qt6::Core)

qt_add_executable(vela-voicerecorder
    src/main.cpp
    src/audioplayer.cpp
    src/audioplayer.h
    src/library.cpp
    src/library.h
    src/recorder.cpp
    src/recorder.h)

set(VOICERECORDER_QML_FILES
    qml/Main.qml
    qml/Waveform.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Slider.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/MenuLayer.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ScrollBar.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/MenuPanel.qml
    ${PROJECT_SOURCE_DIR}/shell/qml/PanelShadow.qml)

foreach(file IN LISTS VOICERECORDER_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-voicerecorder
    URI Vela.VoiceRecorder
    VERSION 1.0
    QML_FILES ${VOICERECORDER_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-voicerecorder "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-voicerecorder PRIVATE -Wall -Wextra)
target_link_libraries(vela-voicerecorder PRIVATE
    vela-voicerecorder-core
    PkgConfig::MPV
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): WAV headers and peaks, names, the waveform.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-voicerecorder-test tests/wav.cpp)
    target_compile_options(vela-voicerecorder-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-voicerecorder-test PRIVATE vela-voicerecorder-core Qt6::Test)
    add_test(NAME voicerecorder-wav COMMAND vela-voicerecorder-test)
endif()

install(TARGETS vela-voicerecorder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-voicerecorder.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: see taskmanager/CMakeLists.txt.
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-voicerecorder SOURCE_TARGETS vela-voicerecorder
    TS_FILES i18n/vela-voicerecorder_it.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-voicerecorder_lupdate
    LRELEASE_TARGET vela-voicerecorder_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
