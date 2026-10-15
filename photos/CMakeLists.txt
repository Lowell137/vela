# SPDX-FileCopyrightText: 2026 Vela contributors
# SPDX-License-Identifier: GPL-3.0-or-later

# Photos (vela-photos): an image viewer, built like Notepad, whose title bar
# pieces and file picker it reuses, with the Explorer's dialog and buttons.
find_package(Qt6 6.7 REQUIRED COMPONENTS Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.7)

# The folder's list of images, shared by the app and its tests.
add_library(vela-photos-core OBJECT
    src/album.cpp
    src/album.h
    src/edits.cpp
    src/edits.h
    src/editview.cpp
    src/editview.h
    src/formats.cpp
    src/formats.h)
target_include_directories(vela-photos-core PUBLIC src)
target_compile_options(vela-photos-core PRIVATE -Wall -Wextra)
target_link_libraries(vela-photos-core PUBLIC Qt6::Gui Qt6::Quick vela-portalprint)

qt_add_executable(vela-photos src/main.cpp)

set(PHOTOS_QML_FILES
    qml/Main.qml
    qml/EditPage.qml
    ${PROJECT_SOURCE_DIR}/settings/qml/Slider.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/WindowButton.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/ResizeEdges.qml
    ${PROJECT_SOURCE_DIR}/notepad/qml/FilePicker.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/Dialog.qml
    ${PROJECT_SOURCE_DIR}/explorer/qml/ToolButton.qml)

foreach(file IN LISTS PHOTOS_QML_FILES)
    get_filename_component(name ${file} NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_qml_module(vela-photos
    URI Vela.Photos
    VERSION 1.0
    QML_FILES ${PHOTOS_QML_FILES})

# The command icons: Fluent's, before the theme's (common/fluent).
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/common/fluent/*.svg)
qt_add_resources(vela-photos "fluent" PREFIX /fluent BASE ${PROJECT_SOURCE_DIR}/common/fluent FILES ${FLUENT_ICONS})

target_compile_options(vela-photos PRIVATE -Wall -Wextra)
target_link_libraries(vela-photos PRIVATE
    vela-photos-core
    Qt6::Qml
    Qt6::Quick
    vela-controls # Theme, Style (common/)
    vela-controlsplugin
    vela-app)

# The tests (ctest): which files of a folder are shown, in what order.
if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.7 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-photos-test tests/album.cpp)
    target_compile_options(vela-photos-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-photos-test PRIVATE vela-photos-core Qt6::Test)
    add_test(NAME photos-album COMMAND vela-photos-test)
    qt_add_executable(vela-photos-formats-test tests/formats.cpp)
    target_compile_options(vela-photos-formats-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-photos-formats-test PRIVATE vela-photos-core Qt6::Test)
    add_test(NAME photos-formats COMMAND vela-photos-formats-test)
    qt_add_executable(vela-photos-edits-test tests/edits.cpp)
    target_compile_options(vela-photos-edits-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-photos-edits-test PRIVATE vela-photos-core Qt6::Test)
    add_test(NAME photos-edits COMMAND vela-photos-edits-test)
    set_tests_properties(photos-album photos-edits photos-formats PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

install(TARGETS vela-photos RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES vela-photos.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# Translations: strings in the code are in English, Italian and Turkish live
# in i18n/ and end up in the executable (:/i18n/vela-photos_it.qm, see
# shell/src/language.h). After changing texts:
#   cmake --build build --target update_translations
find_package(Qt6 6.7 REQUIRED COMPONENTS LinguistTools)
qt_add_translations(TARGETS vela-photos SOURCE_TARGETS vela-photos vela-photos-core
    TS_FILES i18n/vela-photos_it.ts i18n/vela-photos_tr.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_TARGET vela-photos_lupdate
    LRELEASE_TARGET vela-photos_lrelease
    LUPDATE_OPTIONS -source-language en -no-obsolete -locations none)
