# lucida::core - depends on nothing but the standard library and glm.
# Anything that needs SDL, Metal, Jolt or ImGui does not belong in this target.

add_library(lucida_core STATIC
    src/Log.cpp
    src/LinearAllocator.cpp
    src/PoolAllocator.cpp
    src/Profiler.cpp
    src/Registry.cpp
)
add_library(lucida::core ALIAS lucida_core)

target_include_directories(lucida_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_core PUBLIC lucida_glm lucida_entt)
target_compile_features(lucida_core PUBLIC cxx_std_20)
