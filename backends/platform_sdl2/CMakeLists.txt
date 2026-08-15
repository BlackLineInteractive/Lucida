# The only target that links SDL2.

add_library(lucida_platform_sdl2 STATIC
    src/PlatformSDL2.cpp
    ${LUCIDA_IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
)
add_library(lucida::platform_sdl2 ALIAS lucida_platform_sdl2)

target_include_directories(lucida_platform_sdl2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_platform_sdl2
    PUBLIC  lucida::core lucida::input lucida::render
    PRIVATE lucida_imgui ${LUCIDA_SDL2_TARGET})
