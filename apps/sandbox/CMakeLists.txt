# The app picks the backends. Everything below it sees interfaces only.

add_executable(lucida_sandbox src/SandboxApp.cpp)

target_link_libraries(lucida_sandbox PRIVATE
    lucida::core
    lucida::runtime
    lucida::render
    lucida::resource
    lucida::input
    lucida::framework
    lucida::platform_sdl2
    lucida_stb)

if(LUCIDA_RENDER_METAL)
    target_link_libraries(lucida_sandbox PRIVATE lucida::render_metal)
    target_compile_definitions(lucida_sandbox PRIVATE LUCIDA_HAS_METAL=1)
endif()
if(LUCIDA_PHYSICS_JOLT)
    target_link_libraries(lucida_sandbox PRIVATE lucida::physics_jolt)
    target_compile_definitions(lucida_sandbox PRIVATE LUCIDA_HAS_JOLT=1)
endif()

set_target_properties(lucida_sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
