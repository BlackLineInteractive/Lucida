#include "lucida/framework/DebugUI.h"

#include "lucida/core/diag/Profiler.h"

#include "ImGuiFileDialog.h"
#include "imgui.h"

namespace lucida {

void DebugUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding  = 4.0f;
    style.WindowPadding  = ImVec2(10, 10);
}

void DebugUI::Shutdown() { ImGui::DestroyContext(); }

void DebugUI::Build(UiState& ui, RenderSettings& settings, const RenderStats& stats,
                    CameraController& camera, const FrameTime& time) {
    ImGui::NewFrame();

    if (!ui.show_menu) {
        ImGui::Render();
        return;
    }

    const f32 fps = time.real_delta > 0.0f ? 1.0f / time.real_delta : 0.0f;
    m_fps_ema = m_fps_ema * 0.92f + fps * 0.08f;

    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lucida");

    if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%.1f fps   %.2f ms cpu", m_fps_ema, time.real_delta * 1000.0f);
        ImGui::Text("gpu %.2f ms", stats.gpu_frame_ms);
        ImGui::Text("%d rays   %d tris", stats.ray_count, stats.tri_count);
        ImGui::Text("ticks this frame: %u", time.tick_count);

        usize slot_count = 0;
        const ProfileSlot* slots = ProfileSlots(slot_count);
        for (usize i = 0; i < slot_count; ++i) {
            if (slots[i].name) ImGui::Text("  %-14s %.3f ms", slots[i].name, slots[i].millis_avg);
        }
    }

    if (ImGui::CollapsingHeader("Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("render scale", &settings.render_scale, 0.25f, 1.0f, "%.2f");
        ImGui::SliderInt("max depth", &settings.max_depth, 1, 12);
        ImGui::SliderInt("samples", &settings.samples, 1, 8);
        ImGui::Checkbox("fog", &settings.fog);
        ImGui::SameLine();
        ImGui::Checkbox("vsync", &settings.vsync);
        ImGui::SliderInt("debug view", &settings.debug_mode, 0, 6);
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        const Vec3& p = camera.Camera().position;
        ImGui::Text("%.2f  %.2f  %.2f", p.x, p.y, p.z);
        bool walking = camera.Mode() == CameraMode::Walk;
        if (ImGui::Checkbox("walk mode (gravity)", &walking)) {
            camera.SetMode(walking ? CameraMode::Walk : CameraMode::Fly);
        }
        ImGui::SliderFloat("walk speed", &camera.Tuning().walk_speed, 1.0f, 20.0f);
        ImGui::SliderFloat("mouse", &camera.Tuning().look_sensitivity, 0.0005f, 0.01f, "%.4f");
    }

    if (ImGui::CollapsingHeader("Scene")) {
        if (ImGui::RadioButton("demo 0.2", ui.demo_scene == 0)) ui.demo_scene = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("demo 0.3", ui.demo_scene == 1)) ui.demo_scene = 1;

        if (ImGui::Button("Load model...")) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("LoadModel", "Choose a model",
                                                    ".glb,.gltf,.obj,.fbx", config);
        }
        if (ImGui::Button("Fullscreen")) ui.request_fullscreen = true;
        ImGui::SameLine();
        if (ImGui::Button("Quit")) ui.request_quit = true;
    }

    ImGui::End();

    if (ImGuiFileDialog::Instance()->Display("LoadModel", ImGuiWindowFlags_NoCollapse,
                                             ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            ui.pending_model_path = ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Render();
}

} // namespace lucida
