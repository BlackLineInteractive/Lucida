# Metal ray tracing backend. Compiled only on Apple platforms.

add_library(lucida_render_metal STATIC
    src/MetalBackend.mm
    ${LUCIDA_IMGUI_DIR}/backends/imgui_impl_metal.mm
)
add_library(lucida::render_metal ALIAS lucida_render_metal)

target_include_directories(lucida_render_metal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_render_metal
    PUBLIC  lucida::core lucida::render
    PRIVATE lucida_imgui)

# ARC for our backend only; imgui_impl_metal.mm expects manual retain/release.
set_source_files_properties(src/MetalBackend.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

find_library(METAL_LIBRARY Metal REQUIRED)
find_library(METALFX_LIBRARY MetalFX)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
target_link_libraries(lucida_render_metal PRIVATE
    ${METAL_LIBRARY} ${QUARTZCORE_LIBRARY} ${FOUNDATION_LIBRARY})
if(METALFX_LIBRARY)
    target_link_libraries(lucida_render_metal PRIVATE ${METALFX_LIBRARY})
endif()

# Kernels are compiled from source at startup, so they must sit next to the
# binary rather than inside it.
add_custom_command(TARGET lucida_render_metal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:lucida_render_metal>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            ${CMAKE_BINARY_DIR}/shaders
    COMMENT "Staging Metal kernels")
