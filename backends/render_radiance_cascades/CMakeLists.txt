# Radiance Cascades 3D Render Backend (Metal)

add_library(lucida_render_radiance_cascades STATIC
    src/RadianceCascadesBackend.mm
    ${LUCIDA_IMGUI_DIR}/backends/imgui_impl_metal.mm
)
add_library(lucida::render_radiance_cascades ALIAS lucida_render_radiance_cascades)

target_include_directories(lucida_render_radiance_cascades PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_render_radiance_cascades
    PUBLIC  lucida::core lucida::render
    PRIVATE lucida_imgui)

set_source_files_properties(src/RadianceCascadesBackend.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

find_library(METAL_LIBRARY Metal REQUIRED)
find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
target_link_libraries(lucida_render_radiance_cascades PRIVATE
    ${METAL_LIBRARY} ${QUARTZCORE_LIBRARY} ${FOUNDATION_LIBRARY})

file(GLOB LUCIDA_RC_METAL_KERNELS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.metal)
set(LUCIDA_RC_STAGED_KERNELS "")
foreach(kernel ${LUCIDA_RC_METAL_KERNELS})
    get_filename_component(kernel_name ${kernel} NAME)
    set(staged ${CMAKE_BINARY_DIR}/shaders/${kernel_name})
    add_custom_command(
        OUTPUT  ${staged}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${kernel} ${staged}
        DEPENDS ${kernel}
        COMMENT "Staging ${kernel_name}")
    list(APPEND LUCIDA_RC_STAGED_KERNELS ${staged})
endforeach()

add_custom_target(lucida_rc_metal_kernels DEPENDS ${LUCIDA_RC_STAGED_KERNELS})
add_dependencies(lucida_render_radiance_cascades lucida_rc_metal_kernels)
