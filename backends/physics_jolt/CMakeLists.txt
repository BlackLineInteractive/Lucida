# The only target that links Jolt.

add_library(lucida_physics_jolt STATIC
    src/JoltBackend.cpp
    src/JoltVehicleWorld.cpp
)
add_library(lucida::physics_jolt ALIAS lucida_physics_jolt)

target_include_directories(lucida_physics_jolt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_physics_jolt
    PUBLIC  lucida::core lucida::physics
    PRIVATE Jolt)
