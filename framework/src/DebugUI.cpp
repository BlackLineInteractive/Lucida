// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/DebugUI.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/Theme.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include "ImGuiFileDialog.h"
#include "imgui.h"

#include <glm/gtc/quaternion.hpp>

#include <cstdarg>
#include <cstdio>

namespace lucida {
namespace {

// A vector row that reads as one control instead of three.
bool Vec3Row(const char* label, Vec3& value, f32 speed = 0.01f) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(90.0f);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::DragFloat3("##v", &value.x, speed);
    ImGui::PopID();
    return changed;
}

void LabelledText(const char* label, const char* fmt, ...) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(90.0f);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

} // namespace

void DebugUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyTheme();
}

void DebugUI::Shutdown() { ImGui::DestroyContext(); }

void DebugUI::Build(World& world, UiState& ui, RenderSettings& settings,
                    const RenderStats& stats, CameraController& camera,
                    const FrameTime& time) {
    ImGui::NewFrame();

    if (!ui.show_menu) {
        ImGui::Render();
        return;
    }

    const f32 fps = time.real_delta > 0.0f ? 1.0f / time.real_delta : 0.0f;
    m_fps_ema = m_fps_ema * 0.92f + fps * 0.08f;

    // The central dock node stays transparent so the traced image shows through
    // it. Until the renderer can hand ImGui a texture, that image is the
    // viewport; a dockable viewport panel arrives with offscreen targets.
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    DrawMenuBar(ui);
    if (ui.show_hierarchy) DrawHierarchy(world, ui);
    if (ui.show_inspector) DrawInspector(world, ui);
    if (ui.show_renderer)  DrawRenderer(ui, settings, camera);
    if (ui.show_stats)     DrawStats(stats, time);

    if (ImGuiFileDialog::Instance()->Display("LoadModel", ImGuiWindowFlags_NoCollapse,
                                             ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            ui.pending_model_path = ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Render();
}

void DebugUI::DrawMenuBar(UiState& ui) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Load model...")) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("LoadModel", "Choose a model",
                                                    ".glb,.gltf,.obj,.fbx", config);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) ui.request_quit = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Hierarchy", nullptr, &ui.show_hierarchy);
        ImGui::MenuItem("Inspector", nullptr, &ui.show_inspector);
        ImGui::MenuItem("Renderer", nullptr, &ui.show_renderer);
        ImGui::MenuItem("Statistics", nullptr, &ui.show_stats);
        ImGui::Separator();
        if (ImGui::MenuItem("Fullscreen", "F11")) ui.request_fullscreen = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scene")) {
        for (u8 i = 0; i < u8(scenes::BuiltIn::Count); ++i) {
            const auto which = scenes::BuiltIn(i);
            if (ImGui::MenuItem(scenes::Name(which), nullptr, ui.scene == which)) {
                ui.scene = which;
                ui.request_scene_reload = true;
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void DebugUI::DrawHierarchy(World& world, UiState& ui) {
    if (!ImGui::Begin("Hierarchy", &ui.show_hierarchy)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();
    ImGui::TextDisabled("%zu entities", entities.Count());
    ImGui::Separator();

    // Flat list for now. Nesting arrives with reparenting: a tree that cannot be
    // rearranged is a list wearing a costume.
    for (auto [entity, name] : entities.View<Name>().each()) {
        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

        if (Visibility* visibility = entities.Get<Visibility>(entity)) {
            ImGui::Checkbox("##visible", &visibility->visible);
            ImGui::SameLine();
        }
        if (ImGui::Selectable(name.value.c_str(), ui.selection == entity)) {
            ui.selection = entity;
        }

        ImGui::PopID();
    }

    if (entities.Count() == 0) {
        ImGui::TextDisabled("Nothing here yet.");
        ImGui::TextDisabled("Load a model, or open a project.");
    }

    ImGui::End();
}

void DebugUI::DrawInspector(World& world, UiState& ui) {
    if (!ImGui::Begin("Inspector", &ui.show_inspector)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();
    if (!entities.Valid(ui.selection)) {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::End();
        return;
    }

    if (Name* name = entities.Get<Name>(ui.selection)) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s", name->value.c_str());
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) name->value = buffer;
    }

    if (LocalTransform* local = entities.Get<LocalTransform>(ui.selection)) {
        if (BeginSection("Transform", true)) {
            Vec3Row("Position", local->position);

            // Euler angles exist for editing only: derived on read, rebuilt on
            // write, never stored. A second representation of the same rotation
            // is how gimbal bugs get in.
            Vec3 euler = glm::degrees(glm::eulerAngles(local->rotation));
            if (Vec3Row("Rotation", euler, 0.5f)) {
                local->rotation = Quat(glm::radians(euler));
            }

            ImGui::TextUnformatted("Scale");
            ImGui::SameLine(90.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##scale", &local->scale, 0.01f, 0.001f, 1000.0f);
            EndSection();
        }
    }

    if (const WorldTransform* world_transform = entities.Get<WorldTransform>(ui.selection)) {
        if (BeginSection("World")) {
            const Vec3 position(world_transform->matrix[3]);
            LabelledText("Position", "%.3f  %.3f  %.3f", position.x, position.y, position.z);
            ImGui::TextDisabled("Derived from the parent chain each frame.");
            EndSection();
        }
    }

    if (const MeshInstance* mesh = entities.Get<MeshInstance>(ui.selection)) {
        if (BeginSection("Mesh instance", true)) {
            LabelledText("Mesh", "%u", mesh->mesh.index);
            LabelledText("Instance", "%u", mesh->instance.index);
            EndSection();
        }
    }

    if (Vehicle* vehicle = entities.Get<Vehicle>(ui.selection)) {
        if (BeginSection("Vehicle", true)) {
            ImGui::SliderFloat("Throttle", &vehicle->input.throttle, 0.0f, 1.0f);
            ImGui::SliderFloat("Brake", &vehicle->input.brake, 0.0f, 1.0f);
            ImGui::SliderFloat("Steer", &vehicle->input.steer, -1.0f, 1.0f);
            ImGui::Checkbox("Handbrake", &vehicle->input.handbrake);
            EndSection();
        }
    }

    ImGui::End();
}

void DebugUI::DrawRenderer(UiState& ui, RenderSettings& settings, CameraController& camera) {
    if (!ImGui::Begin("Renderer", &ui.show_renderer)) {
        ImGui::End();
        return;
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
        LabelledText("Position", "%.2f  %.2f  %.2f", p.x, p.y, p.z);

        bool walking = camera.Mode() == CameraMode::Walk;
        if (ImGui::Checkbox("walk mode (gravity)", &walking)) {
            camera.SetMode(walking ? CameraMode::Walk : CameraMode::Fly);
        }
        ImGui::SliderFloat("walk speed", &camera.Tuning().walk_speed, 1.0f, 20.0f);
        ImGui::SliderFloat("mouse", &camera.Tuning().look_sensitivity, 0.0005f, 0.01f, "%.4f");
        EndSection();
    }

    ImGui::End();
}

void DebugUI::DrawStats(const RenderStats& stats, const FrameTime& time) {
    if (!ImGui::Begin("Statistics", nullptr)) {
        ImGui::End();
        return;
    }

    ImGui::Text("%.1f fps", m_fps_ema);
    ImGui::Separator();
    LabelledText("CPU", "%.2f ms", time.real_delta * 1000.0f);
    LabelledText("GPU", "%.2f ms", stats.gpu_frame_ms);
    LabelledText("Rays", "%d", stats.ray_count);
    LabelledText("Triangles", "%d", stats.tri_count);
    LabelledText("Ticks", "%u", time.tick_count);

    usize slot_count = 0;
    const ProfileSlot* slots = ProfileSlots(slot_count);
    if (slot_count > 0) {
        ImGui::Separator();
        for (usize i = 0; i < slot_count; ++i) {
            if (slots[i].name) LabelledText(slots[i].name, "%.3f ms", slots[i].millis_avg);
        }
    }

    ImGui::End();
}

} // namespace lucida
