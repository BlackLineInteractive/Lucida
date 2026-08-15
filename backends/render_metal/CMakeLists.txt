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
#
# Staged per file with an explicit DEPENDS rather than as a POST_BUILD step on
# the library: POST_BUILD only fires when the library itself relinks, so editing
# a .metal file alone left the stale copy in place and the change silently did
# not take effect.
file(GLOB LUCIDA_METAL_KERNELS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.metal)
set(LUCIDA_STAGED_KERNELS "")
foreach(kernel ${LUCIDA_METAL_KERNELS})
    get_filename_component(kernel_name ${kernel} NAME)
    set(staged ${CMAKE_BINARY_DIR}/shaders/${kernel_name})
    add_custom_command(
        OUTPUT  ${staged}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${kernel} ${staged}
        DEPENDS ${kernel}
        COMMENT "Staging ${kernel_name}")
    list(APPEND LUCIDA_STAGED_KERNELS ${staged})
endforeach()

add_custom_target(lucida_metal_kernels DEPENDS ${LUCIDA_STAGED_KERNELS})
add_dependencies(lucida_render_metal lucida_metal_kernels)
