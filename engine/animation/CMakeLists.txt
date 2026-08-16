# lucida::animation - Skeletal animation system & pose evaluation

add_library(lucida_animation STATIC
    src/AnimationSystem.cpp
)
add_library(lucida::animation ALIAS lucida_animation)

target_include_directories(lucida_animation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_animation PUBLIC lucida::core lucida::runtime)
