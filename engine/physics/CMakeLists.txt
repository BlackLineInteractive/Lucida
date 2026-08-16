# lucida::physics - vendor-neutral interface only. Jolt and Bullet live under
# backends/ and are never linked into this target.

add_library(lucida_physics INTERFACE)
add_library(lucida::physics ALIAS lucida_physics)

target_include_directories(lucida_physics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_physics INTERFACE lucida::core)
