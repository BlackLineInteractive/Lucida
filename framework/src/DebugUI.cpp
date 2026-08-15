#include "lucida/framework/DebugUI.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/Theme.h"

#include "ImGuiFileDialog.h"
#include "imgui.h"

namespace lucida {

void DebugUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyTheme();
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

    if (BeginSection("Frame", true)) {
        ImGui::Text("%.1f fps   %.2f ms cpu", m_fps_ema, time.real_delta * 1000.0f);
        ImGui::Text("gpu %.2f ms", stats.gpu_frame_ms);
        ImGui::Text("%d rays   %d tris", stats.ray_count, stats.tri_count);
        ImGui::Text("ticks this frame: %u", time.tick_count);

        usize slot_count = 0;
        const ProfileSlot* slots = ProfileSlots(slot_count);
        for (usize i = 0; i < slot_count; ++i) {
            if (slots[i].name) ImGui::Text("  %-14s %.3f ms", slots[i].name, slots[i].millis_avg);
        }
        EndSection();
    }

    if (BeginSection("Quality", true)) {
        ImGui::SliderFloat("render scale", &settings.render_scale, 0.25f, 1.0f, "%.2f");
        ImGui::SliderInt("max depth", &settings.max_depth, 1, 12);
        ImGui::SliderInt("samples", &settings.samples, 1, 8);
        ImGui::Checkbox("fog", &settings.fog);
        ImGui::SameLine();
        ImGui::Checkbox("vsync", &settings.vsync);
        ImGui::SliderInt("debug view", &settings.debug_mode, 0, 6);
        EndSection();
    }

    if (BeginSection("Camera", true)) {
        const Vec3& p = camera.Camera().position;
        ImGui::Text("%.2f  %.2f  %.2f", p.x, p.y, p.z);
        bool walking = camera.Mode() == CameraMode::Walk;
        if (ImGui::Checkbox("walk mode (gravity)", &walking)) {
            camera.SetMode(walking ? CameraMode::Walk : CameraMode::Fly);
        }
        ImGui::SliderFloat("walk speed", &camera.Tuning().walk_speed, 1.0f, 20.0f);
        ImGui::SliderFloat("mouse", &camera.Tuning().look_sensitivity, 0.0005f, 0.01f, "%.4f");
        EndSection();
    }

    if (BeginSection("Scene")) {
        for (u8 i = 0; i < u8(scenes::BuiltIn::Count); ++i) {
            const auto which = scenes::BuiltIn(i);
            if (ImGui::RadioButton(scenes::Name(which), ui.scene == which)) {
                ui.scene = which;
                ui.request_scene_reload = true;
            }
        }

        if (AnimatedButton("Load model...")) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("LoadModel", "Choose a model",
                                                    ".glb,.gltf,.obj,.fbx", config);
        }
        if (AnimatedButton("Fullscreen")) ui.request_fullscreen = true;
        ImGui::SameLine();
        if (AnimatedButton("Quit")) ui.request_quit = true;
        EndSection();
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
