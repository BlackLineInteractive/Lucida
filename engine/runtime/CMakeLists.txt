# lucida::runtime - game loop and world. Links core only, on purpose:
# the loop must not know which renderer or physics backend exists.

add_library(lucida_runtime STATIC
    src/World.cpp
    src/Engine.cpp
)
add_library(lucida::runtime ALIAS lucida_runtime)

target_include_directories(lucida_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_runtime PUBLIC lucida::core)
