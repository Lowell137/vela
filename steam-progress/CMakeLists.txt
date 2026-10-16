# SPDX-FileCopyrightText: 2026 Brando Giuffrida
# SPDX-License-Identifier: GPL-3.0-or-later

# Steam's download progress on its taskbar button. Off by default and
# standalone: it needs only Qt Core and DBus, and reads Steam's own files.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(vela-steam-progress steam-progress.cpp progress.h)
target_compile_options(vela-steam-progress PRIVATE -Wall -Wextra)
target_link_libraries(vela-steam-progress PRIVATE Qt6::Core Qt6::DBus)
install(TARGETS vela-steam-progress RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(VELA_BUILD_TOOLS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(vela-steam-progress-test test.cpp progress.h)
    target_include_directories(vela-steam-progress-test PRIVATE .)
    target_compile_options(vela-steam-progress-test PRIVATE -Wall -Wextra)
    target_link_libraries(vela-steam-progress-test PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME steam-progress COMMAND vela-steam-progress-test)
endif()
