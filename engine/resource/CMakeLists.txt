# lucida::resource — asset import. Assimp and stb stop here; nothing above this
# module knows what a file format is.

add_library(lucida_resource STATIC
    src/ModelLoader.cpp
    src/StbImplementations.cpp
)
add_library(lucida::resource ALIAS lucida_resource)

target_include_directories(lucida_resource PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_resource
    PUBLIC  lucida::core lucida::render
    PRIVATE lucida_bvh lucida_stb ${LUCIDA_ASSIMP_TARGET})
