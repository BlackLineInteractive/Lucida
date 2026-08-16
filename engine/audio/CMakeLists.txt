# lucida::audio - Audio subsystem interface & Null backend

add_library(lucida_audio STATIC
    src/AudioSystem.cpp
)
add_library(lucida::audio ALIAS lucida_audio)

target_include_directories(lucida_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_audio PUBLIC lucida::core lucida::runtime lucida::render)
