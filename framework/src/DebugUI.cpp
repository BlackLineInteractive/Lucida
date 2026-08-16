// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#define IMGUI_DEFINE_MATH_OPERATORS
#include "lucida/framework/DebugUI.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/Picking.h"
#include "lucida/framework/Theme.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

#include "lucida/framework/SceneAssets.h"
#include "lucida/resource/Terrain.h"
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
        DrawViewport(world, ui, viewport_texture, viewport_aspect, camera, assets, stats, settings, time);
    if (ui.show_hierarchy) DrawHierarchy(world, ui, assets); // Calls DrawSceneGraph
    if (ui.show_inspector) DrawInspector(world, ui, assets, camera);
    if (ui.show_graphics_settings) DrawGraphicsSettings(ui, assets, settings, camera);
    if (ui.show_stats_panel)       DrawStatsPanel(world, assets, stats, time, settings, ui);

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
        ImGui::Separator();
        ImGui::MenuItem("Statistics (HUD Overlay)", nullptr, &ui.show_stats_overlay);
        ImGui::MenuItem("Statistics (Docked Panel)", nullptr, &ui.show_stats_panel);
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
                           const CameraController& camera, const SceneAssets& assets,
                           const RenderStats& stats, const RenderSettings& settings, const FrameTime& time) {
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
        const ImVec2 img_offset(ImGui::GetCursorPosX() + (avail.x - size.x) * 0.5f,
                                ImGui::GetCursorPosY() + (avail.y - size.y) * 0.5f);
        ImGui::SetCursorPos(img_offset);

        const ImVec2 image_min = ImGui::GetCursorScreenPos();
        ImGui::Image(reinterpret_cast<ImTextureID>(texture), size);
        const bool image_hovered = ImGui::IsItemHovered();

        // Track hover state so SandboxApp can gate camera input.
        ui.viewport_hovered = image_hovered;

        // RMB held over viewport -> activate look-around (Blender / UE style).
        static bool rmb_started_in_viewport = false;
        if (image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            rmb_started_in_viewport = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            rmb_started_in_viewport = false;
        ui.viewport_rmb = rmb_started_in_viewport;

        // Hotkeys for Gizmo mode and Camera Focus (when viewport is hovered and not flying camera)
        if (image_hovered && !ui.viewport_rmb) {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false) || ImGui::IsKeyPressed(ImGuiKey_1, false)) ui.gizmo_operation = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false) || ImGui::IsKeyPressed(ImGuiKey_2, false)) ui.gizmo_operation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false) || ImGui::IsKeyPressed(ImGuiKey_3, false)) ui.gizmo_operation = 2;
            
            // F hotkey to Focus on selected entity
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && ui.selection != kNullEntity) {
                if (const LocalTransform* lt = world.Entities().Get<LocalTransform>(ui.selection)) {
                    const_cast<CameraController&>(camera).Focus(lt->position, 4.0f);
                }
            }
        }

        // Adjust camera speed with Mouse Wheel while holding RMB (Unreal / Unity style)
        if (ui.viewport_rmb && ImGui::GetIO().MouseWheel != 0.0f) {
            const_cast<CameraController&>(camera).AdjustSpeed(ImGui::GetIO().MouseWheel * 1.0f);
        }

        // LMB click to select (no drag, not clicking/hovering gizmo handles).
        if (image_hovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x == 0.0f &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y == 0.0f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const Vec2 ndc((mouse.x - image_min.x) / size.x * 2.0f - 1.0f,
                           1.0f - (mouse.y - image_min.y) / size.y * 2.0f);

            const Ray ray = RayThroughViewport(camera.Camera(), aspect, ndc);
            const PickResult hit = PickEntity(world.Entities(), ray);
            ui.selection = hit.entity;   // a miss clears the selection, as it should
        }

        // ---- Viewport Toolbar (Auto-resizing dark glass pill) ---------------
        ImGui::SetCursorPos(ImVec2(img_offset.x + 10.0f, img_offset.y + 10.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 16, 22, 235));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(55, 60, 75, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ImGui::BeginChild("ViewportToolbar", ImVec2(0, 32.0f),
                               ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding,
                               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            // Camera Source Selector
            const char* cam_sources[] = { "🎥 Viewport", "🎬 Game Cam" };
            int cur_cam = static_cast<int>(ui.camera_source);
            ImGui::SetNextItemWidth(110.0f);
            if (ImGui::Combo("##CamSource", &cur_cam, cam_sources, 2)) {
                ui.camera_source = static_cast<UiState::CameraSource>(cur_cam);
            }

            ImGui::SameLine(0, 4.0f);
            if (ImGui::Button("View ▼")) {
                ImGui::OpenPopup("ViewPresetsPopup");
            }
            if (ImGui::BeginPopup("ViewPresetsPopup")) {
                if (ImGui::MenuItem("Top (Y+)"))        const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Top);
                if (ImGui::MenuItem("Bottom (Y-)"))     const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Bottom);
                if (ImGui::MenuItem("Front (Z+)"))      const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Front);
                if (ImGui::MenuItem("Back (Z-)"))       const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Back);
                if (ImGui::MenuItem("Right (X+)"))      const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Right);
                if (ImGui::MenuItem("Left (X-)"))       const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Left);
                if (ImGui::MenuItem("Isometric"))       const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Isometric);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset View"))      const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Reset);
                ImGui::EndPopup();
            }

            if (ui.selection != kNullEntity) {
                ImGui::SameLine(0, 4.0f);
                if (ImGui::Button("🎯 Focus")) {
                    if (const LocalTransform* lt = world.Entities().Get<LocalTransform>(ui.selection)) {
                        const_cast<CameraController&>(camera).Focus(lt->position, 4.0f);
                    }
                }
            }

            ImGui::SameLine(0, 4.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 4.0f);

            // Mode buttons
            const char* ops[] = { "Translate (T)", "Rotate (R)", "Scale (S)" };
            for (int i = 0; i < 3; ++i) {
                const bool sel = (ui.gizmo_operation == i);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 110, 220, 240));
                else     ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 48, 200));
                if (ImGui::Button(ops[i])) ui.gizmo_operation = i;
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            ImGui::SameLine(0, 4.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 4.0f);

            // Space buttons
            const char* spaces[] = { "Local", "World" };
            for (int i = 0; i < 2; ++i) {
                const bool sel = (ui.gizmo_space == i);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 150, 90, 240));
                else     ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 48, 200));
                if (ImGui::Button(spaces[i])) ui.gizmo_space = i;
                ImGui::PopStyleColor();
                ImGui::SameLine();
            }

            ImGui::SameLine(0, 4.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 4.0f);

            // Snap toggle button
            const bool snap_active = ui.snap_enabled;
            if (snap_active) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(200, 130, 40, 240));
            else             ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 48, 200));
            if (ImGui::Button("Snap")) ui.snap_enabled = !ui.snap_enabled;
            ImGui::PopStyleColor();

            if (ui.snap_enabled) {
                ImGui::SameLine(0, 4.0f);
                ImGui::SetNextItemWidth(45.0f);
                ImGui::DragFloat("##snap_val", &ui.snap_position.x, 0.05f, 0.01f, 10.0f, "%.2f");
            }

            ImGui::SameLine(0, 4.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 4.0f);

            // Stats HUD toggle button
            const bool stats_active = ui.show_stats_overlay;
            if (stats_active) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(110, 60, 210, 240));
            else              ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 48, 200));
            if (ImGui::Button("Stats HUD")) ui.show_stats_overlay = !ui.show_stats_overlay;
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 4.0f);
            ImGui::TextDisabled("|");
            ImGui::SameLine(0, 4.0f);

            // 3D Visualizers toggle button
            const bool viz_active = ui.show_visualizers;
            if (viz_active) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(30, 160, 180, 240));
            else            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 48, 200));
            if (ImGui::Button("👁️ Overlays")) ui.show_visualizers = !ui.show_visualizers;
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(6);
        ImGui::PopStyleColor(2);

        // Draw Stats Overlay directly on Viewport if enabled
        if (ui.show_stats_overlay) {
            DrawStatsOverlay(world, assets, stats, time, settings, ui, image_min, size);
        }

        if (ui.show_visualizers) {
            DrawViewportVisualizers(world, ui, camera, aspect, image_min, size);
        }

        DrawGizmo(world, ui, const_cast<CameraController&>(camera), aspect, image_min, size);
    } else {
        ui.viewport_width = 0;
        ui.viewport_height = 0;
        ui.viewport_hovered = false;
        ui.viewport_rmb = false;
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
        for (auto [child, parent_comp] : entities.View<Parent>().each()) {
            if (parent_comp.entity == entity) { has_children = true; break; }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (ui.selection == entity) flags |= ImGuiTreeNodeFlags_Selected;
        if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool opened = ImGui::TreeNodeEx(name.value.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            ui.selection = entity;
        }

        if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
            ui.selection = entity;
            if (ImGui::MenuItem("Unparent", nullptr, false, its_parent != kNullEntity)) {
                entities.Remove<Parent>(entity);
            }
            if (ImGui::MenuItem("Duplicate")) {
                Entity dup = entities.Create(name.value + "_copy");
                if (LocalTransform* lt = entities.Get<LocalTransform>(entity)) {
                    *entities.Get<LocalTransform>(dup) = *lt;
                    entities.Get<LocalTransform>(dup)->position += Vec3(0.5f, 0.0f, 0.5f);
                }
                if (PrimitiveShape* ps = entities.Get<PrimitiveShape>(entity)) {
                    entities.Add<PrimitiveShape>(dup, *ps);
                }
                if (MaterialRef* mr = entities.Get<MaterialRef>(entity)) {
                    entities.Add<MaterialRef>(dup, *mr);
                }
                if (LocalBounds* lb = entities.Get<LocalBounds>(entity)) {
                    entities.Add<LocalBounds>(dup, *lb);
                }
                if (RigidBody* rb = entities.Get<RigidBody>(entity)) {
                    entities.Add<RigidBody>(dup, *rb);
                }
                if (its_parent != kNullEntity) {
                    entities.Add<Parent>(dup, Parent{its_parent});
                }
                ui.selection = dup;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Delete")) {
                if (ui.selection == entity) ui.selection = kNullEntity;
                entities.Destroy(entity);
            }
            ImGui::EndPopup();
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
                    entities.Add<Parent>(dropped, Parent{entity});
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

void DebugUI::DrawHierarchy(World& world, UiState& ui, SceneAssets& assets) {
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
            const std::string mat_name = std::string(name) + "_mat_" + std::to_string(assets.materials.size());
            const i32 mat_idx = assets.AddMaterial(
                Material(DIFFUSE, {0.75f, 0.75f, 0.78f}, {0, 0, 0}, 0.5f, 0.0f),
                PROC_NONE, mat_name);
            ui.selection = CreatePrimitive(entities, type, Vec3(0,0,0), mat_idx, name);
        };

        if (ImGui::BeginMenu("3D Object")) {
            if (ImGui::MenuItem("Sphere"))   add(PrimitiveType::Sphere, "Sphere");
            if (ImGui::MenuItem("Cube"))     add(PrimitiveType::Box, "Cube");
            if (ImGui::MenuItem("Plane"))    add(PrimitiveType::Plane, "Plane");
            if (ImGui::MenuItem("Cylinder")) add(PrimitiveType::Cylinder, "Cylinder");
            if (ImGui::MenuItem("Cone"))     add(PrimitiveType::Cone, "Cone");
            if (ImGui::MenuItem("Torus"))    add(PrimitiveType::Torus, "Torus");
            if (ImGui::MenuItem("Disk"))     add(PrimitiveType::Disk, "Disk");
            ImGui::Separator();
            if (ImGui::MenuItem("Terrain (Procedural)")) {
                TerrainComponent cfg{};
                i32 mat_idx = assets.AddMaterial(
                    Material(DIFFUSE, {0.35f, 0.55f, 0.25f}, {0, 0, 0}, 0.85f, 0.0f),
                    PROC_NONE, "Terrain_mat");
                ui.selection = CreateTerrain(entities, assets, cfg, mat_idx, "Terrain");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Perspective Camera")) {
                ui.selection = CreateCamera(entities, Vec3(0.0f, 2.0f, 6.0f), 60.0f, ProjectionType::Perspective, "Main Camera");
            }
            if (ImGui::MenuItem("Orthographic Camera")) {
                ui.selection = CreateCamera(entities, Vec3(0.0f, 6.0f, 6.0f), 60.0f, ProjectionType::Orthographic, "Ortho Camera");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Point Light")) {
                ui.selection = CreateLight(entities, LightType::Point, Vec3(0, 3, 0), Vec3(1.0f, 0.95f, 0.9f), 50.0f, 1.0f, Vec3(0,-1,0), "Point Light");
            }
            if (ImGui::MenuItem("Directional Light (Sun)")) {
                ui.selection = CreateLight(entities, LightType::Directional, Vec3(0, 10, 0), Vec3(1.0f, 0.98f, 0.92f), 10.0f, 5.0f, Vec3(-0.3f, -1.0f, -0.4f), "Directional Light");
            }
            if (ImGui::MenuItem("Spot Light")) {
                ui.selection = CreateLight(entities, LightType::Spot, Vec3(0, 4, 0), Vec3(1.0f, 0.95f, 0.85f), 75.0f, 1.0f, Vec3(0.0f, -1.0f, 0.0f), "Spot Light");
            }
            if (ImGui::MenuItem("Area Light")) {
                ui.selection = CreateLight(entities, LightType::Area, Vec3(0, 3, 0), Vec3(1.0f, 1.0f, 1.0f), 60.0f, 2.0f, Vec3(0.0f, -1.0f, 0.0f), "Area Light");
            }
            ImGui::EndMenu();
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

void DebugUI::DrawInspector(World& world, UiState& ui, SceneAssets& assets, CameraController& camera) {
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
            if (!assets.materials.empty()) {
                if (mat_ref->index < 0 || mat_ref->index >= (i32)assets.materials.size()) {
                    mat_ref->index = 0;
                }

                // Dropdown to pick existing material slot
                const std::string& current_name = assets.material_names[mat_ref->index];
                if (ImGui::BeginCombo("Slot", current_name.c_str())) {
                    for (i32 i = 0; i < (i32)assets.materials.size(); ++i) {
                        const bool is_selected = (mat_ref->index == i);
                        if (ImGui::Selectable(assets.material_names[i].c_str(), is_selected)) {
                            mat_ref->index = i;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Action buttons: "Make Unique" and "+ New Material"
                if (ImGui::Button("Make Unique")) {
                    GPUMaterial cloned = assets.materials[mat_ref->index];
                    std::string new_name = assets.material_names[mat_ref->index] + "_unique";
                    assets.materials.push_back(cloned);
                    assets.material_names.push_back(new_name);
                    mat_ref->index = (i32)assets.materials.size() - 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("+ New Material")) {
                    i32 new_idx = assets.AddMaterial(
                        Material(DIFFUSE, {0.75f, 0.75f, 0.78f}, {0, 0, 0}, 0.5f, 0.0f),
                        PROC_NONE, "mat_" + std::to_string(assets.materials.size()));
                    mat_ref->index = new_idx;
                }

                GPUMaterial& m = assets.materials[mat_ref->index];

                // Material Presets
                ImGui::TextDisabled("Presets:");
                ImGui::SameLine();
                if (ImGui::SmallButton("Gold")) {
                    m.type = 1; m.albedo[0]=1.0f; m.albedo[1]=0.76f; m.albedo[2]=0.33f;
                    m.roughness=0.15f; m.metallic=1.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Chrome")) {
                    m.type = 1; m.albedo[0]=0.95f; m.albedo[1]=0.95f; m.albedo[2]=0.95f;
                    m.roughness=0.05f; m.metallic=1.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Copper")) {
                    m.type = 1; m.albedo[0]=0.95f; m.albedo[1]=0.64f; m.albedo[2]=0.54f;
                    m.roughness=0.20f; m.metallic=1.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Glass")) {
                    m.type = 2; m.albedo[0]=1.0f; m.albedo[1]=1.0f; m.albedo[2]=1.0f;
                    m.roughness=0.0f; m.metallic=0.0f; m.refractive_index=1.52f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Neon")) {
                    m.type = 3; m.albedo[0]=0.1f; m.albedo[1]=0.8f; m.albedo[2]=1.0f;
                    m.emission[0]=1.5f; m.emission[1]=12.0f; m.emission[2]=15.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Rubber")) {
                    m.type = 0; m.albedo[0]=0.12f; m.albedo[1]=0.12f; m.albedo[2]=0.13f;
                    m.roughness=0.90f; m.metallic=0.0f;
                }

                ImGui::Separator();

                // Material Property Editor
                
                // Editable Name
                char name_buf[64];
                std::strncpy(name_buf, assets.material_names[mat_ref->index].c_str(), sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = '\0';
                if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                    assets.material_names[mat_ref->index] = name_buf;
                }

                const char* mat_types[] = { "Diffuse", "Metal", "Glass", "Emissive", "Checkerboard", "Water", "PBR" };
                ImGui::Combo("Type", &m.type, mat_types, 7);

                ImGui::ColorEdit3("Albedo", m.albedo);
                if (m.type == 4) { // Checkerboard
                    ImGui::ColorEdit3("Albedo 2", m.albedo2);
                }
                if (m.type == 3) { // Emissive
                    ImGui::ColorEdit3("Emission", m.emission);
                }

                ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
                ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f);
                if (m.type == 2 || m.type == 5) { // Glass or Water
                    ImGui::SliderFloat("IOR", &m.refractive_index, 1.0f, 3.0f);
                }

                const char* proc_types[] = {
                    "None", "Marble", "Wood", "Rust", "Tiles", "Brushed", "Hex",
                    "Rough Ramp", "Patina", "Concrete", "Perlin Noise", "Voronoi Cells"
                };
                ImGui::Combo("Pattern", &m.proc_id, proc_types, 12);
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

    if (RigidBody* rb = entities.Get<RigidBody>(ui.selection)) {
        if (BeginSection("Physics / RigidBody", true)) {
            const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
            int type_idx = static_cast<int>(rb->type);
            if (ImGui::Combo("Body Type", &type_idx, body_types, 3)) {
                rb->type = static_cast<BodyType>(type_idx);
            }

            const char* shape_types[] = { "Box", "Sphere", "Capsule", "Plane", "Mesh" };
            int shape_idx = static_cast<int>(rb->shape);
            if (ImGui::Combo("Collider Shape", &shape_idx, shape_types, 5)) {
                rb->shape = static_cast<ShapeType>(shape_idx);
            }

            if (rb->type == BodyType::Dynamic) {
                ImGui::DragFloat("Mass (kg)", &rb->mass, 0.1f, 0.01f, 10000.0f, "%.2f");
                ImGui::DragFloat("Gravity Scale", &rb->gravity_scale, 0.05f, 0.0f, 10.0f, "%.2f");
                ImGui::SliderFloat("Linear Damping", &rb->linear_damping, 0.0f, 1.0f);
                ImGui::SliderFloat("Angular Damping", &rb->angular_damping, 0.0f, 1.0f);
            }

            ImGui::SliderFloat("Friction", &rb->friction, 0.0f, 1.0f);
            ImGui::SliderFloat("Restitution", &rb->restitution, 0.0f, 1.0f);
            ImGui::Checkbox("Simulate", &rb->is_active);

            ImGui::Separator();
            if (ImGui::Button("Remove Physics", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<RigidBody>(ui.selection);
            }
            EndSection();
        }
    } else {
        if (BeginSection("Physics", false)) {
            if (ImGui::Button("+ Add RigidBody Component", ImVec2(-1.0f, 24.0f))) {
                RigidBody rb{};
                if (PrimitiveShape* ps = entities.Get<PrimitiveShape>(ui.selection)) {
                    if (ps->type == PrimitiveType::Sphere) rb.shape = ShapeType::Sphere;
                    else if (ps->type == PrimitiveType::Plane) { rb.shape = ShapeType::Plane; rb.type = BodyType::Static; }
                    else rb.shape = ShapeType::Box;
                }
                entities.Add<RigidBody>(ui.selection, rb);
            }
            EndSection();
        }
    }

    if (LightSource* light = entities.Get<LightSource>(ui.selection)) {
        if (BeginSection("Light Source", true)) {
            const char* light_types[] = { "Point Light", "Directional Light (Sun)", "Spot Light", "Area Light" };
            int type_idx = static_cast<int>(light->type);
            if (ImGui::Combo("Light Type", &type_idx, light_types, 4)) {
                light->type = static_cast<LightType>(type_idx);
            }

            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));

            // Quick Color Temperature presets
            ImGui::TextDisabled("Color Temp:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Candle 2000K"))   light->color = Vec3(1.00f, 0.58f, 0.16f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Warm 3200K"))     light->color = Vec3(1.00f, 0.82f, 0.62f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Daylight 6500K")) light->color = Vec3(1.00f, 1.00f, 1.00f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Sky 9000K"))      light->color = Vec3(0.80f, 0.88f, 1.00f);

            ImGui::DragFloat("Intensity", &light->intensity, 0.5f, 0.0f, 10000.0f, "%.1f");
            ImGui::DragFloat("Radius / Range", &light->radius, 0.05f, 0.01f, 100.0f, "%.2f");

            if (light->type == LightType::Directional || light->type == LightType::Spot) {
                ImGui::Separator();
                Vec3Row("Direction", light->direction);
                if (glm::length(light->direction) > 0.001f) {
                    light->direction = glm::normalize(light->direction);
                }
            }

            if (light->type == LightType::Spot) {
                ImGui::SliderFloat("Inner Cone", &light->inner_angle, 1.0f, 89.0f, "%.1f deg");
                ImGui::SliderFloat("Outer Cone", &light->outer_angle, light->inner_angle, 90.0f, "%.1f deg");
            }

            ImGui::Checkbox("Cast Shadows", &light->cast_shadows);
            EndSection();
        }
    }

    if (TerrainComponent* terrain = entities.Get<TerrainComponent>(ui.selection)) {
        if (BeginSection("Terrain Generator", true)) {
            bool changed = false;
            changed |= ImGui::SliderInt("Resolution", &terrain->resolution, 8, 256);
            changed |= ImGui::DragFloat("World Size", &terrain->size, 0.5f, 5.0f, 500.0f, "%.1f m");
            changed |= ImGui::DragFloat("Max Height", &terrain->max_height, 0.1f, 0.5f, 100.0f, "%.1f m");
            changed |= ImGui::SliderFloat("Frequency", &terrain->frequency, 0.001f, 0.2f, "%.4f");
            changed |= ImGui::SliderInt("Octaves", &terrain->octaves, 1, 8);
            changed |= ImGui::SliderFloat("Persistence", &terrain->persistence, 0.1f, 0.9f, "%.2f");
            changed |= ImGui::SliderFloat("Lacunarity", &terrain->lacunarity, 1.0f, 4.0f, "%.2f");
            
            ImGui::InputScalar("Seed", ImGuiDataType_U32, &terrain->seed);
            ImGui::SameLine();
            if (ImGui::Button("Random")) {
                terrain->seed = static_cast<u32>(rand());
                changed = true;
            }

            if (changed) terrain->dirty = true;

            ImGui::Separator();
            if (ImGui::Button("Regenerate Terrain", ImVec2(-1.0f, 26.0f))) {
                terrain->dirty = true;
            }
            EndSection();
        }
    }

    if (CameraComponent* cam = entities.Get<CameraComponent>(ui.selection)) {
        if (BeginSection("Camera Component", true)) {
            const char* proj_types[] = { "Perspective", "Orthographic" };
            int cur_proj = static_cast<int>(cam->projection);
            if (ImGui::Combo("Projection", &cur_proj, proj_types, 2)) {
                cam->projection = static_cast<ProjectionType>(cur_proj);
            }

            if (cam->projection == ProjectionType::Perspective) {
                ImGui::SliderFloat("Field of View", &cam->fov, 10.0f, 130.0f, "%.1f deg");
            } else {
                ImGui::DragFloat("Ortho Size", &cam->ortho_size, 0.1f, 0.1f, 100.0f, "%.1f m");
            }

            ImGui::DragFloat("Near Clip", &cam->near_clip, 0.01f, 0.001f, 10.0f, "%.3f m");
            ImGui::DragFloat("Far Clip", &cam->far_clip, 1.0f, 10.0f, 10000.0f, "%.0f m");
            ImGui::Checkbox("Main Scene Camera", &cam->is_primary);
            ImGui::DragFloat("Exposure", &cam->exposure, 0.05f, 0.1f, 10.0f, "%.2f");

            ImGui::Separator();
            if (LocalTransform* lt = entities.Get<LocalTransform>(ui.selection)) {
                if (ImGui::Button("🎬 Align Camera to View", ImVec2(-1.0f, 24.0f))) {
                    lt->position = camera.Camera().position;
                    const Vec3 fwd = camera.Camera().Forward();
                    lt->rotation = glm::quatLookAt(fwd, Vec3(0, 1, 0));
                }
                if (ImGui::Button("🎥 Align View to Camera", ImVec2(-1.0f, 24.0f))) {
                    const_cast<CameraController&>(camera).Camera().position = lt->position;
                    const Vec3 fwd = lt->rotation * Vec3(0, 0, -1);
                    const_cast<CameraController&>(camera).LookAt(lt->position, lt->position + fwd);
                }
            }
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

    if (BeginSection("Navigation", true)) {
        const Vec3& p = camera.Camera().position;
        LabelledText("Position", "%.2f  %.2f  %.2f", p.x, p.y, p.z);
        ImGui::SliderFloat("fly speed",   &camera.Tuning().fly_speed,  1.0f, 50.0f, "%.1f m/s");
        ImGui::SliderFloat("sprint mul",  &camera.Tuning().sprint_mul, 1.0f, 6.0f, "%.1fx");
        ImGui::SliderFloat("sensitivity", &camera.Tuning().look_sensitivity, 0.0005f, 0.01f, "%.4f");
        
        f32 fov_deg = glm::degrees(camera.Camera().fov_y);
        if (ImGui::SliderFloat("Field of View", &fov_deg, 30.0f, 120.0f, "%.0f deg")) {
            const_cast<CameraController&>(camera).Camera().fov_y = glm::radians(fov_deg);
        }

        ImGui::Separator();
        ImGui::TextDisabled("View Presets:");
        if (ImGui::Button("Top"))   const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Top);
        ImGui::SameLine();
        if (ImGui::Button("Front")) const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Front);
        ImGui::SameLine();
        if (ImGui::Button("Right")) const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Right);
        ImGui::SameLine();
        if (ImGui::Button("Iso"))   const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Isometric);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) const_cast<CameraController&>(camera).SetViewPreset(ViewPreset::Reset);

        ImGui::Separator();
        ImGui::TextDisabled("RMB drag: look   WASD: move   Q/E: up/down   Shift: sprint   Wheel: speed");
        EndSection();
    }

    ImGui::End();
}

void DebugUI::DrawGizmo(World& world, UiState& ui, CameraController& camera, f32 aspect,
                        const ImVec2& image_min, const ImVec2& image_size) {
    if (!ui.show_viewport || ui.selection == kNullEntity) return;

    Registry& registry = world.Entities();
    LocalTransform* local = registry.Get<LocalTransform>(ui.selection);
    if (!local) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();

    // Use the actual rendered image rect, not the window content region.
    // The window has a toolbar above the image, so the content-region approach
    // misaligns the gizmo by the toolbar height.
    ImGuizmo::SetRect(image_min.x, image_min.y, image_size.x, image_size.y);

    const CameraState& cam = camera.Camera();
    Vec3 fwd   = cam.Forward();
    Vec3 right = cam.Right();
    Vec3 up    = cam.Up();
    // View matrix from basis vectors
    Mat4 view = Mat4(1.0f);
    view[0][0] = right.x; view[1][0] = right.y; view[2][0] = right.z; view[3][0] = -glm::dot(right, cam.position);
    view[0][1] = up.x;    view[1][1] = up.y;    view[2][1] = up.z;    view[3][1] = -glm::dot(up,    cam.position);
    view[0][2] = -fwd.x;  view[1][2] = -fwd.y;  view[2][2] = -fwd.z; view[3][2] =  glm::dot(fwd,  cam.position);
    view[0][3] = 0;       view[1][3] = 0;       view[2][3] = 0;       view[3][3] = 1.0f;
    // Perspective projection
    Mat4 proj = glm::perspective(cam.fov_y, aspect, 0.01f, 1000.0f);

    // Check if entity has a parent
    Entity parent_entity = kNullEntity;
    const Parent* parent = registry.Get<Parent>(ui.selection);
    if (parent && registry.Valid(parent->entity)) {
        parent_entity = parent->entity;
    }

    Mat4 parent_world(1.0f);
    if (parent_entity != kNullEntity) {
        parent_world = ComputeWorldTransform(registry, parent_entity);
    }

    Mat4 matrix = parent_world * local->ToMatrix();

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
        Mat4 new_local = (parent_entity != kNullEntity) ? glm::inverse(parent_world) * matrix : matrix;

        float translation[3], rotation[3], scale[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(new_local), translation, rotation, scale);
        
        local->position = Vec3(translation[0], translation[1], translation[2]);
        local->rotation = Quat(glm::radians(Vec3(rotation[0], rotation[1], rotation[2])));
        if (scale[0] > 0.001f) local->scale = scale[0];

        // Keep picking bounds in sync with scale
        if (PrimitiveShape* shape = registry.Get<PrimitiveShape>(ui.selection)) {
            const Vec3 half = shape->HalfExtents() * local->scale;
            if (LocalBounds* bounds = registry.Get<LocalBounds>(ui.selection)) {
                bounds->min = -half;
                bounds->max =  half;
            }
        }
        
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

void DebugUI::DrawStatsPanel(World& world, const SceneAssets& assets, const RenderStats& stats,
                            const FrameTime& time, const RenderSettings& settings, const UiState& ui) {
    bool open = ui.show_stats_panel;
    if (!ImGui::Begin("Statistics", const_cast<bool*>(&ui.show_stats_panel))) {
        ImGui::End();
        return;
    }

    if (BeginSection("Performance", true)) {
        const ImVec4 fps_color = (m_fps_ema >= 50.0f) ? ImVec4(0.3f, 0.9f, 0.4f, 1.0f)
                               : (m_fps_ema >= 30.0f) ? ImVec4(0.9f, 0.8f, 0.2f, 1.0f)
                                                      : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(fps_color, "%.1f FPS", m_fps_ema);
        ImGui::SameLine(130.0f);
        ImGui::Text("%.2f ms frame", time.real_delta * 1000.0f);
        LabelledText("CPU", "%.2f ms", time.real_delta * 1000.0f);
        LabelledText("GPU", "%.2f ms", stats.gpu_frame_ms);
        LabelledText("Ticks", "%u", time.tick_count);
        EndSection();
    }

    if (BeginSection("Ray Tracer", true)) {
        LabelledText("Viewport", "%d x %d", ui.viewport_width, ui.viewport_height);
        LabelledText("Scale", "%.2f (%.0f%%)", settings.render_scale, settings.render_scale * 100.0f);
        LabelledText("Rays", "%d", stats.ray_count);
        LabelledText("Samples", "%d spp", settings.samples);
        LabelledText("Max Depth", "%d", settings.max_depth);
        EndSection();
    }

    if (BeginSection("Scene Breakdown", true)) {
        LabelledText("Entities", "%zu", world.Entities().Count());
        LabelledText("Triangles", "%d", stats.tri_count);
        LabelledText("Materials", "%zu", assets.materials.size());
        EndSection();
    }

    usize slot_count = 0;
    const ProfileSlot* slots = ProfileSlots(slot_count);
    if (slot_count > 0 && BeginSection("CPU Profiler", true)) {
        for (usize i = 0; i < slot_count; ++i) {
            if (slots[i].name) LabelledText(slots[i].name, "%.3f ms", slots[i].millis_avg);
        }
        EndSection();
    }

    ImGui::End();
}

void DebugUI::DrawStatsOverlay(World& world, const SceneAssets& assets, const RenderStats& stats,
                              const FrameTime& time, const RenderSettings& settings, const UiState& ui,
                              const ImVec2& image_min, const ImVec2& image_size) {
    if (image_size.x < 240.0f || image_size.y < 120.0f) return;

    // Place at top-right corner of the rendered viewport
    const float pad = 10.0f;
    const float w   = 200.0f;
    const float h   = 90.0f;
    const ImVec2 pos(image_min.x + image_size.x - w - pad, image_min.y + pad);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rect_max(pos.x + w, pos.y + h);
    dl->AddRectFilled(pos, rect_max, IM_COL32(14, 16, 22, 225), 6.0f);
    dl->AddRect(pos, rect_max, IM_COL32(55, 60, 75, 180), 6.0f);

    ImVec2 cur(pos.x + 8.0f, pos.y + 6.0f);

    // FPS with color coding
    const ImU32 fps_col = (m_fps_ema >= 50.0f) ? IM_COL32(80, 220, 100, 255)
                        : (m_fps_ema >= 30.0f) ? IM_COL32(230, 200, 60, 255)
                                               : IM_COL32(230, 70, 70, 255);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "%.1f FPS", m_fps_ema);
    dl->AddText(cur, fps_col, buf);

    std::snprintf(buf, sizeof(buf), "%.2f ms", time.real_delta * 1000.0f);
    dl->AddText(ImVec2(pos.x + w - 65.0f, cur.y), IM_COL32(200, 200, 215, 255), buf);

    cur.y += 18.0f;
    dl->AddLine(ImVec2(pos.x + 6.0f, cur.y), ImVec2(pos.x + w - 6.0f, cur.y), IM_COL32(50, 55, 70, 180));
    cur.y += 4.0f;

    std::snprintf(buf, sizeof(buf), "Res: %dx%d (%.0f%%)",
                  ui.viewport_width, ui.viewport_height, settings.render_scale * 100.0f);
    dl->AddText(cur, IM_COL32(170, 175, 190, 255), buf);
    cur.y += 16.0f;

    const usize entity_count = world.Entities().Count();
    std::snprintf(buf, sizeof(buf), "Entities: %zu  |  Mats: %zu", entity_count, assets.materials.size());
    dl->AddText(cur, IM_COL32(170, 175, 190, 255), buf);
    cur.y += 16.0f;

    std::snprintf(buf, sizeof(buf), "Rays: %d  |  Tris: %d", stats.ray_count, stats.tri_count);
    dl->AddText(cur, IM_COL32(170, 175, 190, 255), buf);
}

void DebugUI::DrawViewportVisualizers(World& world, const UiState& ui, const CameraController& camera, f32 aspect,
                                     const ImVec2& image_min, const ImVec2& image_size) {
    if (!ui.show_viewport || image_size.x <= 0 || image_size.y <= 0) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) return;

    const CameraState& cam = camera.Camera();
    const Vec3 fwd   = cam.Forward();
    const Vec3 right = cam.Right();
    const Vec3 up    = cam.Up();

    Mat4 view(1.0f);
    view[0][0] = right.x; view[1][0] = right.y; view[2][0] = right.z; view[3][0] = -glm::dot(right, cam.position);
    view[0][1] = up.x;    view[1][1] = up.y;    view[2][1] = up.z;    view[3][1] = -glm::dot(up,    cam.position);
    view[0][2] = -fwd.x;  view[1][2] = -fwd.y;  view[2][2] = -fwd.z; view[3][2] =  glm::dot(fwd,  cam.position);
    view[0][3] = 0;       view[1][3] = 0;       view[2][3] = 0;       view[3][3] = 1.0f;

    Mat4 proj = glm::perspective(cam.fov_y, aspect, 0.01f, 1000.0f);
    Mat4 view_proj = proj * view;

    auto WorldToScreen = [&](const Vec3& p, ImVec2& out_screen) -> bool {
        Vec4 clip = view_proj * Vec4(p, 1.0f);
        if (clip.w <= 0.05f) return false;
        Vec3 ndc = Vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        out_screen.x = image_min.x + (ndc.x * 0.5f + 0.5f) * image_size.x;
        out_screen.y = image_min.y + (-ndc.y * 0.5f + 0.5f) * image_size.y;
        return true;
    };

    auto Draw3DLine = [&](const Vec3& a, const Vec3& b, ImU32 col, float thickness = 1.5f) {
        ImVec2 sa, sb;
        if (WorldToScreen(a, sa) && WorldToScreen(b, sb)) {
            draw_list->AddLine(sa, sb, col, thickness);
        }
    };

    auto Draw3DCircle = [&](const Vec3& center, const Vec3& normal, float radius, ImU32 col, int segments = 24, float thickness = 1.2f) {
        Vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : Vec3(0, 1, 0);
        Vec3 u = (std::abs(n.y) < 0.99f) ? glm::normalize(glm::cross(n, Vec3(0, 1, 0))) : Vec3(1, 0, 0);
        Vec3 v = glm::cross(n, u);
        ImVec2 prev_s;
        bool has_prev = false;
        ImVec2 first_s;
        bool has_first = false;
        for (int i = 0; i <= segments; ++i) {
            float theta = (float(i) / float(segments)) * glm::two_pi<float>();
            Vec3 pt = center + (u * std::cos(theta) + v * std::sin(theta)) * radius;
            ImVec2 s;
            if (WorldToScreen(pt, s)) {
                if (has_prev) draw_list->AddLine(prev_s, s, col, thickness);
                else { first_s = s; has_first = true; }
                prev_s = s;
                has_prev = true;
            } else {
                has_prev = false;
            }
        }
        if (has_prev && has_first) {
            draw_list->AddLine(prev_s, first_s, col, thickness);
        }
    };

    Registry& entities = const_cast<World&>(world).Entities();

    // 1. Camera Frustums
    for (auto [e, cam_comp, lt] : entities.View<CameraComponent, LocalTransform>().each()) {
        const bool selected = (e == ui.selection);
        const ImU32 col = selected ? IM_COL32(80, 220, 255, 255) : IM_COL32(70, 180, 220, 180);
        const float thick = selected ? 2.0f : 1.2f;

        const Vec3 pos = lt.position;
        const Vec3 c_fwd = lt.rotation * Vec3(0, 0, -1);
        const Vec3 c_up  = lt.rotation * Vec3(0, 1, 0);
        const Vec3 c_right = lt.rotation * Vec3(1, 0, 0);

        const float d_near = 0.4f;
        const float d_far  = 2.2f;
        const float half_h = d_far * std::tan(glm::radians(cam_comp.fov * 0.5f));
        const float half_w = half_h * 1.777f;

        const float near_h = d_near * std::tan(glm::radians(cam_comp.fov * 0.5f));
        const float near_w = near_h * 1.777f;

        // 4 corners of far plane
        const Vec3 far_center = pos + c_fwd * d_far;
        const Vec3 f_tl = far_center - c_right * half_w + c_up * half_h;
        const Vec3 f_tr = far_center + c_right * half_w + c_up * half_h;
        const Vec3 f_br = far_center + c_right * half_w - c_up * half_h;
        const Vec3 f_bl = far_center - c_right * half_w - c_up * half_h;

        // 4 corners of near plane
        const Vec3 near_center = pos + c_fwd * d_near;
        const Vec3 n_tl = near_center - c_right * near_w + c_up * near_h;
        const Vec3 n_tr = near_center + c_right * near_w + c_up * near_h;
        const Vec3 n_br = near_center + c_right * near_w - c_up * near_h;
        const Vec3 n_bl = near_center - c_right * near_w - c_up * near_h;

        // Draw 4 pyramid rays from eye
        Draw3DLine(pos, f_tl, col, thick);
        Draw3DLine(pos, f_tr, col, thick);
        Draw3DLine(pos, f_br, col, thick);
        Draw3DLine(pos, f_bl, col, thick);

        // Draw far quad
        Draw3DLine(f_tl, f_tr, col, thick);
        Draw3DLine(f_tr, f_br, col, thick);
        Draw3DLine(f_br, f_bl, col, thick);
        Draw3DLine(f_bl, f_tl, col, thick);

        // Draw near quad
        Draw3DLine(n_tl, n_tr, col, thick * 0.8f);
        Draw3DLine(n_tr, n_br, col, thick * 0.8f);
        Draw3DLine(n_br, n_bl, col, thick * 0.8f);
        Draw3DLine(n_bl, n_tl, col, thick * 0.8f);

        // Top orientation notch (triangle indicating "Up")
        const Vec3 notch_peak = far_center + c_up * (half_h + 0.35f);
        Draw3DLine(f_tl, notch_peak, col, thick);
        Draw3DLine(f_tr, notch_peak, col, thick);
    }

    // 2. Light Visualizers
    for (auto [e, light, lt] : entities.View<LightSource, LocalTransform>().each()) {
        const bool selected = (e == ui.selection);
        const Vec3 pos = lt.position;

        if (light.type == LightType::Spot) {
            const ImU32 col = selected ? IM_COL32(255, 225, 70, 255) : IM_COL32(230, 200, 50, 180);
            const float thick = selected ? 2.0f : 1.2f;

            Vec3 dir = light.direction;
            if (glm::length(dir) < 0.001f) dir = Vec3(0, -1, 0);
            else dir = glm::normalize(dir);

            const float range = std::min(std::max(light.radius * 3.5f, 3.0f), 8.0f);
            const float outer_rad = range * std::tan(glm::radians(light.outer_angle));
            const float inner_rad = range * std::tan(glm::radians(light.inner_angle));

            const Vec3 base_center = pos + dir * range;

            // Perpendicular axes
            Vec3 u = (std::abs(dir.y) < 0.99f) ? glm::normalize(glm::cross(dir, Vec3(0, 1, 0))) : Vec3(1, 0, 0);
            Vec3 v = glm::cross(dir, u);

            // Outer cone circle
            Draw3DCircle(base_center, dir, outer_rad, col, 24, thick);
            // Inner cone circle
            Draw3DCircle(base_center, dir, inner_rad, col, 16, thick * 0.7f);

            // 4 cone generator rays
            Draw3DLine(pos, base_center + u * outer_rad, col, thick);
            Draw3DLine(pos, base_center - u * outer_rad, col, thick);
            Draw3DLine(pos, base_center + v * outer_rad, col, thick);
            Draw3DLine(pos, base_center - v * outer_rad, col, thick);

            // Center direction arrow
            Draw3DLine(pos, base_center, col, thick * 0.8f);
        }
        else if (light.type == LightType::Directional) {
            const ImU32 col = selected ? IM_COL32(255, 240, 90, 255) : IM_COL32(240, 220, 60, 190);
            const float thick = selected ? 2.0f : 1.2f;

            Vec3 dir = light.direction;
            if (glm::length(dir) < 0.001f) dir = Vec3(0, -1, 0);
            else dir = glm::normalize(dir);

            // Sun disk
            Draw3DCircle(pos, dir, 0.7f, col, 20, thick);

            // Perpendicular axes
            Vec3 u = (std::abs(dir.y) < 0.99f) ? glm::normalize(glm::cross(dir, Vec3(0, 1, 0))) : Vec3(1, 0, 0);
            Vec3 v = glm::cross(dir, u);

            const float ray_len = 2.5f;
            // 4 parallel sunlight rays from disk edge
            Vec3 r1 = pos + u * 0.7f;
            Vec3 r2 = pos - u * 0.7f;
            Vec3 r3 = pos + v * 0.7f;
            Vec3 r4 = pos - v * 0.7f;

            Draw3DLine(r1, r1 + dir * ray_len, col, thick);
            Draw3DLine(r2, r2 + dir * ray_len, col, thick);
            Draw3DLine(r3, r3 + dir * ray_len, col, thick);
            Draw3DLine(r4, r4 + dir * ray_len, col, thick);
            Draw3DLine(pos, pos + dir * (ray_len * 1.2f), col, thick * 1.5f);
        }
        else if (light.type == LightType::Point) {
            const ImU32 col = selected ? IM_COL32(255, 210, 50, 255) : IM_COL32(240, 190, 40, 160);
            const float thick = selected ? 1.8f : 1.0f;
            const float rad = std::max(light.radius, 0.3f);

            // 3 orthogonal circles (XY, XZ, YZ)
            Draw3DCircle(pos, Vec3(0, 1, 0), rad, col, 24, thick);
            Draw3DCircle(pos, Vec3(1, 0, 0), rad, col, 24, thick);
            Draw3DCircle(pos, Vec3(0, 0, 1), rad, col, 24, thick);

            // Center star icon
            const float icon_d = 0.15f;
            Draw3DLine(pos - Vec3(icon_d, 0, 0), pos + Vec3(icon_d, 0, 0), col, 2.0f);
            Draw3DLine(pos - Vec3(0, icon_d, 0), pos + Vec3(0, icon_d, 0), col, 2.0f);
            Draw3DLine(pos - Vec3(0, 0, icon_d), pos + Vec3(0, 0, icon_d), col, 2.0f);
        }
    }

    // 3. Selected Entity Bounding Box Wireframe
    if (ui.selection != kNullEntity) {
        if (const LocalTransform* lt = entities.Get<LocalTransform>(ui.selection)) {
            if (const LocalBounds* bounds = entities.Get<LocalBounds>(ui.selection)) {
                const ImU32 col = IM_COL32(255, 175, 30, 220);
                const float thick = 1.5f;

                const Mat4 world_mat = lt->ToMatrix();
                const Vec3 min = bounds->min;
                const Vec3 max = bounds->max;

                Vec3 c[8] = {
                    Vec3(world_mat * Vec4(min.x, min.y, min.z, 1.0f)),
                    Vec3(world_mat * Vec4(max.x, min.y, min.z, 1.0f)),
                    Vec3(world_mat * Vec4(max.x, max.y, min.z, 1.0f)),
                    Vec3(world_mat * Vec4(min.x, max.y, min.z, 1.0f)),
                    Vec3(world_mat * Vec4(min.x, min.y, max.z, 1.0f)),
                    Vec3(world_mat * Vec4(max.x, min.y, max.z, 1.0f)),
                    Vec3(world_mat * Vec4(max.x, max.y, max.z, 1.0f)),
                    Vec3(world_mat * Vec4(min.x, max.y, max.z, 1.0f))
                };

                // Bottom face
                Draw3DLine(c[0], c[1], col, thick);
                Draw3DLine(c[1], c[5], col, thick);
                Draw3DLine(c[5], c[4], col, thick);
                Draw3DLine(c[4], c[0], col, thick);

                // Top face
                Draw3DLine(c[3], c[2], col, thick);
                Draw3DLine(c[2], c[6], col, thick);
                Draw3DLine(c[6], c[7], col, thick);
                Draw3DLine(c[7], c[3], col, thick);

                // Vertical edges
                Draw3DLine(c[0], c[3], col, thick);
                Draw3DLine(c[1], c[2], col, thick);
                Draw3DLine(c[5], c[6], col, thick);
                Draw3DLine(c[4], c[7], col, thick);
            }
        }
    }
}

} // namespace lucida
