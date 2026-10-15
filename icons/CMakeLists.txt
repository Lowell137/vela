# The "vela" icon theme (Fluent-style, light) and "vela-dark", derived here by
# painting currentColor / the Fluent #212121 in the dark text color.
file(GLOB_RECURSE VELA_ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/vela ${CMAKE_CURRENT_SOURCE_DIR}/vela/scalable/*.svg)
set(VELA_DARK_DIR ${CMAKE_CURRENT_BINARY_DIR}/vela-dark)
foreach(icon IN LISTS VELA_ICON_FILES)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vela/${icon} content)
    string(REPLACE "currentColor" "#f3f3f6" content "${content}")
    string(REPLACE "#212121" "#f3f3f6" content "${content}")
    file(WRITE ${VELA_DARK_DIR}/${icon} "${content}")
endforeach()
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vela/index.theme index)
string(REPLACE "Name=Vela" "Name=Vela (dark)" index "${index}")
string(REPLACE "Inherits=Fluent,Fluent-dark,breeze," "Inherits=Fluent-dark,Fluent,breeze-dark,breeze," index "${index}")
file(WRITE ${VELA_DARK_DIR}/index.theme "${index}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vela/index.theme)

install(DIRECTORY vela DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
install(DIRECTORY ${VELA_DARK_DIR} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
