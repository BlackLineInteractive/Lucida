set(ICON_FILE "${CMAKE_SOURCE_DIR}/media/Lucida.icns")

if(APPLE)
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    add_executable(lucida_sandbox MACOSX_BUNDLE
        src/SandboxApp.cpp
        ${ICON_FILE}
    )
    set_target_properties(lucida_sandbox PROPERTIES
        OUTPUT_NAME "Lucida"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Lucida"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.blackline.lucida"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.5.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.5.0"
        MACOSX_BUNDLE_ICON_FILE "Lucida.icns"
        RESOURCE "${ICON_FILE}"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    add_custom_command(TARGET lucida_sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            Lucida.app/Contents/MacOS/Lucida
            ${CMAKE_BINARY_DIR}/lucida_sandbox
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            Lucida.app/Contents/MacOS/Lucida
            ${CMAKE_BINARY_DIR}/Lucida
        COMMENT "Creating CLI symlinks for Lucida app"
    )
else()
    add_executable(lucida_sandbox src/SandboxApp.cpp)
    set_target_properties(lucida_sandbox PROPERTIES
        OUTPUT_NAME "Lucida"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

target_link_libraries(lucida_sandbox PRIVATE
    lucida::core
    lucida::runtime
    lucida::render
    lucida::resource
    lucida::input
    lucida::framework
    lucida::platform_sdl2
    lucida_imgui
    lucida_stb
    lucida_fastnoise)

if(LUCIDA_RENDER_METAL)
    target_link_libraries(lucida_sandbox PRIVATE lucida::render_metal)
    target_compile_definitions(lucida_sandbox PRIVATE LUCIDA_HAS_METAL=1)
endif()
if(LUCIDA_RENDER_RC)
    target_link_libraries(lucida_sandbox PRIVATE lucida::render_radiance_cascades)
    target_compile_definitions(lucida_sandbox PRIVATE LUCIDA_HAS_RC=1)
endif()
if(LUCIDA_PHYSICS_JOLT)
    target_link_libraries(lucida_sandbox PRIVATE lucida::physics_jolt)
    target_compile_definitions(lucida_sandbox PRIVATE LUCIDA_HAS_JOLT=1)
endif()
