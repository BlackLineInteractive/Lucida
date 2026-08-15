# lucida::input — HID abstraction (GEA ch.8). Knows nothing about SDL; the
# platform backend fills the state, gameplay reads actions.

add_library(lucida_input INTERFACE)
add_library(lucida::input ALIAS lucida_input)

target_include_directories(lucida_input INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_input INTERFACE lucida::core)
