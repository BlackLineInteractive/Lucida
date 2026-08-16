# lucida::render - front end and backend interface. Header-only for now:
# every concrete graphics API lives under backends/.

add_library(lucida_render INTERFACE)
add_library(lucida::render ALIAS lucida_render)

target_include_directories(lucida_render INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_render INTERFACE lucida::core)
