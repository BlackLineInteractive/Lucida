# lucida::framework — tools and UI/UX. The only engine-side target that links
# ImGui; gameplay and backends stay free of it.

add_library(lucida_framework STATIC
    src/CameraController.cpp
    src/DebugUI.cpp
    ${LUCIDA_IFD_DIR}/ImGuiFileDialog.cpp
)
add_library(lucida::framework ALIAS lucida_framework)

target_include_directories(lucida_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lucida_framework
    PUBLIC  lucida::core lucida::input lucida::render
    PRIVATE lucida_imgui)
