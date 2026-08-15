// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/DebugUI.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/Picking.h"
#include "lucida/framework/Theme.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include "lucida/framework/SceneAssets.h"
#include "ImGuiFileDialog.h"
#include "ImGuizmo.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

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

// Turns a live-edited control into a single undoable command. ImGui reports both
// edges of an interaction, which is exactly what the one-entry-per-drag rule
// needs: grab the old value on activation, push on release.
void DebugUI::TrackEdit(Registry& registry, Entity entity, const LocalTransform& current,
                        const char* name) {
    if (ImGui::IsItemActivated()) {
        m_drag_start = current;
        m_dragging = true;
    }
    if (m_dragging && ImGui::IsItemDeactivatedAfterEdit()) {
        m_commands.Push(std::make_unique<TransformCommand>(registry, entity, m_drag_start,
                                                           current, name));
        m_dragging = false;
    }
}

void DebugUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyTheme();
}

void DebugUI::Shutdown() { ImGui::DestroyContext(); }

void DebugUI::Build(World& world, SceneAssets& assets, UiState& ui, RenderSettings& settings,
                    const RenderStats& stats, CameraController& camera,
                    const FrameTime& time, void* viewport_texture, f32 viewport_aspect) {
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    if (!ui.show_menu) {
        ImGui::Render();
        return;
    }

    const f32 fps = time.real_delta > 0.0f ? 1.0f / time.real_delta : 0.0f;
    m_fps_ema = m_fps_ema * 0.92f + fps * 0.08f;

    // The layout every engine editor uses, because it works: viewport in the
    // middle, the scene tree on the left, properties on the right, output along
    // the bottom. Built once; after that ImGui restores it from imgui.ini, so a
    // rearranged workspace survives a restart.
    const ImGuiID dockspace_id = ImGui::GetID("LucidaDockSpace");
    if (m_reset_layout || ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        BuildDefaultLayout(dockspace_id);
        m_reset_layout = false;
    }

    // The central node stays transparent, so closing the viewport panel shows
    // the traced image behind the docks rather than a blank hole.
    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    DrawMenuBar(ui);
    // Undo before anything reads the world this frame, so the panels below show
    // the restored state rather than the state that was just undone.
    const ImGuiIO& io = ImGui::GetIO();
    const bool modifier = io.KeySuper || io.KeyCtrl;   // Cmd on macOS, Ctrl elsewhere
    if (modifier && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) m_commands.Redo(); else m_commands.Undo();
    }

    if (ui.show_viewport && viewport_texture)
        DrawViewport(world, ui, viewport_texture, viewport_aspect, camera);
    if (ui.show_hierarchy) DrawHierarchy(world, ui); // Calls DrawSceneGraph
    if (ui.show_inspector) DrawInspector(world, ui, assets);
    if (ui.show_graphics_settings) DrawGraphicsSettings(ui, assets, settings, camera);
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

void DebugUI::BuildDefaultLayout(unsigned dockspace_id) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id,
                              ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    ImGuiID centre = dockspace_id;
    const ImGuiID left   = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left,  0.18f, nullptr, &centre);
    const ImGuiID right  = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.22f, nullptr, &centre);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down,  0.24f, nullptr, &centre);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Renderer", right);
    ImGui::DockBuilderDockWindow("Statistics", bottom);
    ImGui::DockBuilderDockWindow("Viewport", centre);

    ImGui::DockBuilderFinish(dockspace_id);
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
        ImGui::MenuItem("Viewport", nullptr, &ui.show_viewport);
        ImGui::MenuItem("Hierarchy", nullptr, &ui.show_hierarchy);
        ImGui::MenuItem("Inspector", nullptr, &ui.show_inspector);
        ImGui::MenuItem("Graphics Settings", nullptr, &ui.show_graphics_settings);
        ImGui::MenuItem("Statistics", nullptr, &ui.show_stats);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset layout")) m_reset_layout = true;
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

void DebugUI::DrawViewport(World& world, UiState& ui, void* texture, f32 aspect,
                           const CameraController& camera) {
    // No padding: the image is the panel, and a border of window background
    // around a rendered frame reads as a bug.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("Viewport", &ui.show_viewport);
    ImGui::PopStyleVar();

    if (open) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        // Report the panel in *pixels*, not points. ImGui works in points and
        // the framebuffer is 2x that on a retina display: reporting points would
        // trace at half the resolution the panel actually shows and look soft.
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        ui.viewport_width  = i32(avail.x * (scale.x > 0.0f ? scale.x : 1.0f));
        ui.viewport_height = i32(avail.y * (scale.y > 0.0f ? scale.y : 1.0f));

        // The renderer is tracing at the panel's own aspect now, so the image
        // fills it. The letterbox stays for the frames right after a resize,
        // where the texture still carries the previous shape.
        ImVec2 size = avail;
        if (aspect > 0.0f && avail.y > 0.0f) {
            if (avail.x / avail.y > aspect) size.x = avail.y * aspect;
            else                            size.y = avail.x / aspect;
        }
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - size.x) * 0.5f,
                                   ImGui::GetCursorPosY() + (avail.y - size.y) * 0.5f));

        const ImVec2 image_min = ImGui::GetCursorScreenPos();
        ImGui::Image(reinterpret_cast<ImTextureID>(texture), size);

        // Click to select. Only on a release without a drag: press-and-drag in
        // the viewport is how the camera is flown, and losing the selection
        // every time you look around would be maddening.
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x == 0.0f &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y == 0.0f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const Vec2 ndc((mouse.x - image_min.x) / size.x * 2.0f - 1.0f,
                           1.0f - (mouse.y - image_min.y) / size.y * 2.0f);

            const Ray ray = RayThroughViewport(camera.Camera(), aspect, ndc);
            const PickResult hit = PickEntity(world.Entities(), ray);
            ui.selection = hit.entity;   // a miss clears the selection, as it should
        }

        // Toolbar for Gizmo
        ImGui::SetCursorPos(ImVec2(10, 30));
        ImGui::BeginGroup();
        if (ImGui::RadioButton("Translate", ui.gizmo_operation == 0)) ui.gizmo_operation = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", ui.gizmo_operation == 1)) ui.gizmo_operation = 1;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", ui.gizmo_operation == 2)) ui.gizmo_operation = 2;
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", ui.gizmo_space == 0)) ui.gizmo_space = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", ui.gizmo_space == 1)) ui.gizmo_space = 1;
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(20, 0));
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &ui.snap_enabled);
        ImGui::EndGroup();

        DrawGizmo(world, ui, const_cast<CameraController&>(camera), aspect);
    } else {
        ui.viewport_width = 0;
        ui.viewport_height = 0;
    }
    ImGui::End();
}

void DebugUI::DrawSceneGraph(World& world, UiState& ui, Entity current_parent) {
    Registry& entities = world.Entities();
    for (auto [entity, name] : entities.View<Name>().each()) {
        Entity its_parent = kNullEntity;
        if (Parent* p = entities.Get<Parent>(entity)) its_parent = p->entity;
        
        if (its_parent != current_parent) continue;

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
        
        bool has_children = false;
        for (auto [child] : entities.View<Parent>().each()) {
            if (entities.Get<Parent>(child)->entity == entity) { has_children = true; break; }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (ui.selection == entity) flags |= ImGuiTreeNodeFlags_Selected;
        if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool opened = ImGui::TreeNodeEx(name.value.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            ui.selection = entity;
        }

        if (ImGui::BeginDragDropSource()) {
            Entity dragged = entity;
            ImGui::SetDragDropPayload("ENTITY_PAYLOAD", &dragged, sizeof(Entity));
            ImGui::TextUnformatted(name.value.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
                Entity dropped = *static_cast<const Entity*>(payload->Data);
                if (dropped != entity) {
                    entities.AddOrReplace<Parent>(dropped, Parent{entity});
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (opened) {
            DrawSceneGraph(world, ui, entity);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void DebugUI::DrawHierarchy(World& world, UiState& ui) {
    if (!ImGui::Begin("Hierarchy", &ui.show_hierarchy)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();
    ImGui::TextDisabled("%zu entities", entities.Count());

    ImGui::SameLine(ImGui::GetWindowWidth() - 40);
    if (ImGui::Button("+")) ImGui::OpenPopup("AddPrimitivePopup");
    if (ImGui::BeginPopup("AddPrimitivePopup")) {
        auto add = [&](PrimitiveType type, const char* name) {
            ui.selection = CreatePrimitive(entities, type, Vec3(0,0,0), 0, name);
        };
        if (ImGui::MenuItem("Sphere")) add(PrimitiveType::Sphere, "Sphere");
        if (ImGui::MenuItem("Box")) add(PrimitiveType::Box, "Box");
        if (ImGui::MenuItem("Plane")) add(PrimitiveType::Plane, "Plane");
        if (ImGui::MenuItem("Cylinder")) add(PrimitiveType::Cylinder, "Cylinder");
        if (ImGui::MenuItem("Cone")) add(PrimitiveType::Cone, "Cone");
        if (ImGui::MenuItem("Torus")) add(PrimitiveType::Torus, "Torus");
        if (ImGui::MenuItem("Disk")) add(PrimitiveType::Disk, "Disk");
        ImGui::Separator();
        if (ImGui::MenuItem("Light")) {
            ui.selection = CreateLight(entities, Vec3(0, 1, 0), Vec3(1), 50.0f, 1.0f, "Light");
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    DrawSceneGraph(world, ui, kNullEntity);

    // Drop on empty space to unparent
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
            Entity dropped = *static_cast<const Entity*>(payload->Data);
            entities.Remove<Parent>(dropped);
        }
        ImGui::EndDragDropTarget();
    }

    if (entities.Count() == 0) {
        ImGui::TextDisabled("Nothing here yet.");
        ImGui::TextDisabled("Load a model, or open a project.");
    }

    ImGui::End();
}

void DebugUI::DrawInspector(World& world, UiState& ui, SceneAssets& assets) {
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

    if (Visibility* visibility = entities.Get<Visibility>(ui.selection)) {
        ImGui::Checkbox("Visible", &visibility->visible);
    }

    if (LocalTransform* local = entities.Get<LocalTransform>(ui.selection)) {
        if (BeginSection("Transform", true)) {
            Vec3Row("Position", local->position);
            TrackEdit(entities, ui.selection, *local, "Move");

            Vec3 euler = glm::degrees(glm::eulerAngles(local->rotation));
            if (Vec3Row("Rotation", euler, 0.5f)) {
                local->rotation = Quat(glm::radians(euler));
            }
            TrackEdit(entities, ui.selection, *local, "Rotate");

            ImGui::TextUnformatted("Scale");
            ImGui::SameLine(90.0f);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##scale", &local->scale, 0.01f, 0.001f, 1000.0f);
            TrackEdit(entities, ui.selection, *local, "Scale");
            EndSection();
        }
    }

    if (PrimitiveShape* shape = entities.Get<PrimitiveShape>(ui.selection)) {
        if (BeginSection("Shape", true)) {
            const char* type_names[] = { "Sphere", "Box", "Plane", "Cylinder", "Cone", "Torus", "Disk" };
            int type_idx = (int)shape->type;
            if (ImGui::Combo("Type", &type_idx, type_names, 7)) {
                shape->type = (PrimitiveType)type_idx;
            }

            if (shape->type == PrimitiveType::Sphere) {
                ImGui::DragFloat("Radius", &shape->size.x, 0.01f);
            } else if (shape->type == PrimitiveType::Box) {
                Vec3Row("Half Extents", shape->size);
            } else if (shape->type == PrimitiveType::Plane) {
                Vec3Row("Normal", shape->normal);
                ImGui::DragFloat("Offset", &shape->offset, 0.01f);
            } else if (shape->type == PrimitiveType::Cylinder || shape->type == PrimitiveType::Cone) {
                ImGui::DragFloat("Radius", &shape->size.x, 0.01f);
                ImGui::DragFloat("Height", &shape->cylinder_height, 0.01f);
            } else if (shape->type == PrimitiveType::Torus) {
                ImGui::DragFloat("Radius", &shape->size.x, 0.01f);
                ImGui::DragFloat("Inner Radius", &shape->inner_radius, 0.01f);
            } else if (shape->type == PrimitiveType::Disk) {
                ImGui::DragFloat("Radius", &shape->size.x, 0.01f);
                Vec3Row("Normal", shape->normal);
            }
            EndSection();
        }
    }

    if (MaterialRef* mat_ref = entities.Get<MaterialRef>(ui.selection)) {
        if (BeginSection("Material", true)) {
            if (mat_ref->index >= 0 && mat_ref->index < (i32)assets.materials.size()) {
                GPUMaterial& m = assets.materials[mat_ref->index];
                ImGui::Text("Material: %s", assets.material_names[mat_ref->index].c_str());
                const char* mat_types[] = { "Diffuse", "Metal", "Glass", "Emissive", "Checkerboard", "Water", "PBR" };
                ImGui::Combo("Type", &m.type, mat_types, 7);
                ImGui::ColorEdit3("Albedo", m.albedo);
                if (m.type == 3) ImGui::ColorEdit3("Emission", m.emission);
                ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f);
                if (m.type == 2 || m.type == 5) ImGui::SliderFloat("IOR", &m.refractive_index, 1.0f, 3.0f);

                const char* proc_types[] = { "None", "Marble", "Wood", "Rust", "Tiles", "Brushed", "Hex", "Rough Ramp", "Patina", "Concrete" };
                ImGui::Combo("Pattern", &m.proc_id, proc_types, 10);
            }
            EndSection();
        }
    }

    if (const WorldTransform* world_transform = entities.Get<WorldTransform>(ui.selection)) {
        if (BeginSection("World", true)) {
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

void DebugUI::DrawGraphicsSettings(UiState& ui, SceneAssets& assets, RenderSettings& settings, CameraController& camera) {
    if (!ImGui::Begin("Graphics Settings", &ui.show_graphics_settings)) {
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

    if (BeginSection("Environment", true)) {
        ImGui::ColorEdit3("Ambient", glm::value_ptr(assets.environment.ambient));
        ImGui::ColorEdit3("Sky Zenith", glm::value_ptr(assets.environment.sky_zenith));
        ImGui::ColorEdit3("Sky Horizon", glm::value_ptr(assets.environment.sky_horizon));
        ImGui::ColorEdit3("Sky Ground", glm::value_ptr(assets.environment.sky_ground));
        
        ImGui::Checkbox("Fog Enabled", &assets.environment.fog_enabled);
        if (assets.environment.fog_enabled) {
            ImGui::DragFloat("Fog Density", &assets.environment.fog_density, 0.001f);
            ImGui::SliderInt("Fog Steps", &assets.environment.fog_steps, 4, 64);
        }
        EndSection();
    }

    if (BeginSection("Grid (Blender Style)", true)) {
        ImGui::Checkbox("Enabled", &assets.environment.grid_enabled);
        if (assets.environment.grid_enabled) {
            ImGui::Checkbox("Auto-scale Grid", &assets.environment.grid_auto_scale);
            if (!assets.environment.grid_auto_scale) {
                ImGui::DragFloat("Spacing", &assets.environment.grid_spacing, 0.1f, 0.1f, 100.0f);
            }
            ImGui::SliderFloat("Opacity", &assets.environment.grid_opacity, 0.0f, 1.0f);
            ImGui::DragFloat("Fade Distance", &assets.environment.grid_fade, 1.0f, 10.0f, 1000.0f);
            ImGui::ColorEdit3("Grid Color", glm::value_ptr(assets.environment.grid_color));
            ImGui::ColorEdit3("X Axis", glm::value_ptr(assets.environment.grid_axis_x));
            ImGui::ColorEdit3("Z Axis", glm::value_ptr(assets.environment.grid_axis_z));
        }
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

void DebugUI::DrawGizmo(World& world, UiState& ui, CameraController& camera, f32 aspect) {
    if (!ui.show_viewport || ui.selection == kNullEntity) return;

    Registry& registry = world.Entities();
    LocalTransform* local = registry.Get<LocalTransform>(ui.selection);
    if (!local) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    
    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    vMin.x += ImGui::GetWindowPos().x;
    vMin.y += ImGui::GetWindowPos().y;
    vMax.x += ImGui::GetWindowPos().x;
    vMax.y += ImGui::GetWindowPos().y;
    ImGuizmo::SetRect(vMin.x, vMin.y, vMax.x - vMin.x, vMax.y - vMin.y);

    const CameraState& cam = camera.Camera();
    Mat4 view = cam.ViewMatrix();
    Mat4 proj = cam.ProjectionMatrix(aspect);

    Mat4 matrix = local->ToMatrix();

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (ui.gizmo_operation == 1) op = ImGuizmo::ROTATE;
    if (ui.gizmo_operation == 2) op = ImGuizmo::SCALE;

    ImGuizmo::MODE mode = ui.gizmo_space == 0 ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    float snap[3] = {0};
    if (ui.snap_enabled) {
        if (op == ImGuizmo::TRANSLATE) { snap[0] = ui.snap_position.x; snap[1] = ui.snap_position.y; snap[2] = ui.snap_position.z; }
        else if (op == ImGuizmo::ROTATE) { snap[0] = snap[1] = snap[2] = ui.snap_rotation; }
        else if (op == ImGuizmo::SCALE) { snap[0] = snap[1] = snap[2] = ui.snap_scale; }
    }

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, mode, glm::value_ptr(matrix), nullptr, ui.snap_enabled ? snap : nullptr)) {
        float translation[3], rotation[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), translation, rotation, scale);
        
        local->position = Vec3(translation[0], translation[1], translation[2]);
        local->rotation = Quat(glm::radians(Vec3(rotation[0], rotation[1], rotation[2])));
        local->scale = scale[0];
        
        if (!m_dragging) {
            m_drag_start = *local;
            m_dragging = true;
        }
    } else {
        if (m_dragging) {
            m_commands.Push(std::make_unique<TransformCommand>(registry, ui.selection, m_drag_start, *local, "Gizmo Edit"));
            m_dragging = false;
        }
    }
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
