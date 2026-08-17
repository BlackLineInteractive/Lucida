// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

static void DrawOrientationGizmo(World& world, const UiState& ui, CameraController& camera, const ImVec2& image_min, const ImVec2& image_size) {
    if (image_size.x < 160.0f || image_size.y < 160.0f) return;

    const float radius = 30.0f;
    const ImVec2 center(image_min.x + image_size.x - radius - 20.0f, image_min.y + radius + 20.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const Vec3 right   = camera.Camera().Right();
    const Vec3 up      = camera.Camera().Up();
    const Vec3 forward = camera.Camera().Forward();

    struct AxisInfo {
        Vec3 dir;
        ImU32 color;
        const char* label;
        ViewPreset preset;
        float proj_x;
        float proj_y;
        float depth;
        bool is_positive;
    };

    AxisInfo axes[6] = {
        { Vec3( 1, 0, 0), IM_COL32(235, 65, 65, 255), "X", ViewPreset::Right, 0, 0, 0, true },
        { Vec3(-1, 0, 0), IM_COL32(160, 45, 45, 180), "-X", ViewPreset::Left, 0, 0, 0, false },
        { Vec3( 0, 1, 0), IM_COL32(65, 215, 85, 255), "Y", ViewPreset::Top, 0, 0, 0, true },
        { Vec3( 0,-1, 0), IM_COL32(45, 140, 55, 180), "-Y", ViewPreset::Bottom, 0, 0, 0, false },
        { Vec3( 0, 0, 1), IM_COL32(65, 145, 245, 255), "Z", ViewPreset::Front, 0, 0, 0, true },
        { Vec3( 0, 0,-1), IM_COL32(45, 95, 175, 180), "-Z", ViewPreset::Back, 0, 0, 0, false },
    };

    for (int i = 0; i < 6; ++i) {
        axes[i].proj_x = glm::dot(axes[i].dir, right);
        axes[i].proj_y = -glm::dot(axes[i].dir, up);
        axes[i].depth  = glm::dot(axes[i].dir, forward);
    }

    int order[6] = {0, 1, 2, 3, 4, 5};
    std::sort(order, order + 6, [&](int a, int b) {
        return axes[a].depth < axes[b].depth;
    });

    const ImVec2 mouse_pos = ImGui::GetMousePos();
    const float dist_to_center = std::hypot(mouse_pos.x - center.x, mouse_pos.y - center.y);
    const bool gizmo_hovered = (dist_to_center <= radius + 10.0f);

    const float dt = ImGui::GetIO().DeltaTime;
    const iam_ease_desc ez_quad{iam_ease_out_quad, 0, 0, 0, 0};
    const float bg_hover_t = iam_tween_float(ImGui::GetID("gizmo_bg"), 0, gizmo_hovered ? 1.0f : 0.0f, 0.18f, ez_quad, iam_policy_crossfade, dt, 0.0f);

    // Draw subtle glass circle background with animated hover glow
    const int bg_alpha = static_cast<int>(140 + bg_hover_t * 80.0f);
    const int border_alpha = static_cast<int>(120 + bg_hover_t * 100.0f);
    dl->AddCircleFilled(center, radius + 8.0f, IM_COL32(14, 16, 22, bg_alpha), 32);
    dl->AddCircle(center, radius + 8.0f, IM_COL32(60, 75, 110, border_alpha), 32, 1.0f + bg_hover_t * 0.5f);

    int clicked_preset = -1;

    for (int idx = 0; idx < 6; ++idx) {
        const int i = order[idx];
        const AxisInfo& a = axes[i];
        const ImVec2 pt(center.x + a.proj_x * radius, center.y + a.proj_y * radius);

        const float pt_dist = std::hypot(mouse_pos.x - pt.x, mouse_pos.y - pt.y);
        const float base_r = a.is_positive ? 7.5f : 5.0f;
        const bool pt_hovered = (pt_dist <= base_r + 3.0f);

        const float pt_hover_t = iam_tween_float(ImGui::GetID("gizmo_axis"), i, pt_hovered ? 1.0f : 0.0f, 0.12f, ez_quad, iam_policy_crossfade, dt, 0.0f);
        const float dot_r = base_r + pt_hover_t * 2.5f;

        if (a.is_positive || a.depth > -0.2f) {
            dl->AddLine(center, pt, a.color, a.is_positive ? (2.2f + pt_hover_t * 1.0f) : 1.4f);
        }

        if (pt_hover_t > 0.001f) {
            const int halo_alpha = static_cast<int>(pt_hover_t * 240.0f);
            dl->AddCircleFilled(pt, dot_r + 2.0f, IM_COL32(255, 255, 255, halo_alpha), 16);
            if (pt_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                clicked_preset = static_cast<int>(a.preset);
            }
        }
        dl->AddCircleFilled(pt, dot_r, a.color, 16);

        if (a.is_positive) {
            char lbl[2] = { a.label[0], 0 };
            const ImVec2 txt_size = ImGui::CalcTextSize(lbl);
            dl->AddText(ImVec2(pt.x - txt_size.x * 0.5f, pt.y - txt_size.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), lbl);
        }
    }

    // Center pivot dot / reset isometric with smooth hover
    const bool center_hovered = (dist_to_center <= 6.0f);
    const float center_t = iam_tween_float(ImGui::GetID("gizmo_center"), 0, center_hovered ? 1.0f : 0.0f, 0.12f, ez_quad, iam_policy_crossfade, dt, 0.0f);
    const float center_r = 4.0f + center_t * 3.0f;
    const int center_alpha = static_cast<int>(180 + center_t * 75.0f);
    dl->AddCircleFilled(center, center_r, IM_COL32(200, 210, 230, center_alpha), 16);
    if (center_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        clicked_preset = static_cast<int>(ViewPreset::Isometric);
    }
    static bool s_gizmo_dragging = false;
    if (gizmo_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        s_gizmo_dragging = true;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        s_gizmo_dragging = false;
    }

    if (clicked_preset >= 0) {
        camera.SetViewPreset(static_cast<ViewPreset>(clicked_preset));
        s_gizmo_dragging = false;
    } else if (s_gizmo_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);

        // Orbit camera around focus target smoothly (Blender standard direction)
        Vec3 focal_point(0.0f);
        if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
            if (const LocalTransform* lt = world.Entities().Get<LocalTransform>(ui.selection)) {
                focal_point = lt->position;
            }
        }
        const float dist = std::max(glm::length(camera.Camera().position - focal_point), 3.0f);
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        camera.Camera().yaw   += delta.x * 0.012f;
        camera.Camera().pitch -= delta.y * 0.012f;
        camera.Camera().pitch  = Clamp(camera.Camera().pitch, -1.55f, 1.55f);
        camera.Camera().position = focal_point - camera.Camera().Forward() * dist;
    }
}

void EditorUI::DrawStatsOverlay(World& world, const SceneAssets& assets, const RenderStats& stats,
                              const FrameTime& time, const RenderSettings& settings, const UiState& ui,
                              const ImVec2& image_min, const ImVec2& image_size) {
    if (image_size.x < 240.0f || image_size.y < 120.0f) return;

    // Place on the LEFT side under the top toolbar with zero overlap
    const float pad_x = 10.0f;
    const float pad_y = ui.viewport_toolbar_collapsed ? 42.0f : 48.0f;
    const float w     = 200.0f;
    const float h     = 90.0f;
    const ImVec2 pos(image_min.x + pad_x, image_min.y + pad_y);

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

void EditorUI::DrawViewport(World& world, UiState& ui, void* texture, f32 aspect,
                            const CameraController& camera, const SceneAssets& assets,
                            const RenderStats& stats, const RenderSettings& settings, const FrameTime& time) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("Viewport", &ui.show_viewport);
    ImGui::PopStyleVar();

    if (open) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        ui.viewport_width  = i32(avail.x * (scale.x > 0.0f ? scale.x : 1.0f));
        ui.viewport_height = i32(avail.y * (scale.y > 0.0f ? scale.y : 1.0f));

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

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                const char* dropped_path = static_cast<const char*>(payload->Data);
                if (dropped_path) {
                    std::string p = dropped_path;
                    std::string ext = std::filesystem::path(p).extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx") {
                        ui.pending_model_path = p;
                        LUCIDA_INFO(App, "Asset Browser: dropped model into viewport '%s'", p.c_str());
                    } else if (ext == ".json") {
                        ui.pending_model_path = p;
                        ui.request_scene_reload = true;
                        LUCIDA_INFO(App, "Asset Browser: dropped scene into viewport '%s'", p.c_str());
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ui.viewport_hovered = image_hovered;

        // RMB held over viewport -> activate look-around
        static bool rmb_started_in_viewport = false;
        if (image_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            rmb_started_in_viewport = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))
            rmb_started_in_viewport = false;
        ui.viewport_rmb = rmb_started_in_viewport;

        // Hotkeys for Gizmo mode and Camera Focus
        if (image_hovered && !ui.viewport_rmb) {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false) || ImGui::IsKeyPressed(ImGuiKey_1, false)) ui.gizmo_operation = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false) || ImGui::IsKeyPressed(ImGuiKey_2, false)) ui.gizmo_operation = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false) || ImGui::IsKeyPressed(ImGuiKey_3, false)) ui.gizmo_operation = 2;
            
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && ui.selection != kNullEntity) {
                if (const LocalTransform* lt = world.Entities().Get<LocalTransform>(ui.selection)) {
                    const_cast<CameraController&>(camera).Focus(lt->position, 4.0f);
                }
            }
        }

        // Adjust camera speed with Mouse Wheel while holding RMB
        if (ui.viewport_rmb && ImGui::GetIO().MouseWheel != 0.0f) {
            const_cast<CameraController&>(camera).AdjustSpeed(ImGui::GetIO().MouseWheel * 1.0f);
        }

        // LMB click to select entity
        if (image_hovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x == 0.0f &&
            ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y == 0.0f) {
            const ImVec2 mouse = ImGui::GetMousePos();
            const Vec2 ndc((mouse.x - image_min.x) / size.x * 2.0f - 1.0f,
                           1.0f - (mouse.y - image_min.y) / size.y * 2.0f);

            const Ray ray = RayThroughViewport(camera.Camera(), aspect, ndc);
            const PickResult hit = PickEntity(world.Entities(), ray);
            ui.selection = hit.entity;
        }

        // ---- Viewport Toolbar (Collapsible dark glass pill) ---------------
        ImGui::SetCursorPos(ImVec2(img_offset.x + 10.0f, img_offset.y + 10.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 16, 22, 235));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(55, 60, 75, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        if (ui.viewport_toolbar_collapsed) {
            if (ImGui::BeginChild("ViewportToolbarCollapsed", ImVec2(0, 32.0f),
                                   ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding,
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (VectorIconButton("vp_expand", VectorIcon::RightArrow, "Tools", ImVec2(78.0f, 22.0f))) {
                    ui.viewport_toolbar_collapsed = false;
                }
                DrawTooltip("Expand Viewport Toolbar");
            }
            ImGui::EndChild();
        } else {
            if (ImGui::BeginChild("ViewportToolbar", ImVec2(0, 32.0f),
                                   ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding,
                                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                // Collapse toggle button
                if (VectorIconButton("vp_collapse", VectorIcon::LeftArrow, "", ImVec2(24.0f, 22.0f))) {
                    ui.viewport_toolbar_collapsed = true;
                }
                DrawTooltip("Collapse Viewport Toolbar");

                ImGui::SameLine(0, 4.0f);

                // Camera Source Selector
                const char* cam_sources[] = { "Fly Cam", "Game Cam" };
                int cur_cam = static_cast<int>(ui.camera_source);
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::Combo("##CamSource", &cur_cam, cam_sources, 2)) {
                    ui.camera_source = static_cast<UiState::CameraSource>(cur_cam);
                }
                DrawTooltip("Camera Source: Choose between Viewport Fly Camera and in-scene Game Camera.");

                ImGui::SameLine(0, 4.0f);
                if (VectorIconButton("vp_view", VectorIcon::DownArrow, "View", ImVec2(58, 22))) {
                    ImGui::OpenPopup("ViewPresetsPopup");
                }
                DrawTooltip("View Presets: Snap camera to standard orthographic or isometric viewpoints.");
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
                    if (VectorIconButton("vp_focus", VectorIcon::Focus, "Focus", ImVec2(66, 22))) {
                        if (const LocalTransform* lt = world.Entities().Get<LocalTransform>(ui.selection)) {
                            const_cast<CameraController&>(camera).Focus(lt->position, 4.0f);
                        }
                    }
                    DrawTooltip("Focus (Hotkey: F): Instantly center and frame camera on the selected entity.");
                }

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // Mode buttons
                const char* ops[] = { "Translate (T)", "Rotate (R)", "Scale (S)" };
                const char* op_tips[] = {
                    "Translate Mode (T or 1): Move object along axes.",
                    "Rotate Mode (R or 2): Rotate object around axes.",
                    "Scale Mode (S or 3): Scale object along axes."
                };
                for (int i = 0; i < 3; ++i) {
                    const bool sel = (ui.gizmo_operation == i);
                    const ImU32 col = sel ? IM_COL32(50, 110, 220, 240) : 0;
                    if (VectorIconButton(ops[i], VectorIcon::None, ops[i], ImVec2(0, 22), col)) {
                        ui.gizmo_operation = i;
                    }
                    DrawTooltip(op_tips[i]);
                    ImGui::SameLine(0, 3.0f);
                }

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // Space buttons
                const char* spaces[] = { "Local", "World" };
                const char* space_tips[] = {
                    "Local Space: Orient Gizmo along entity's local rotation axes.",
                    "World Space: Orient Gizmo along fixed global XYZ axes."
                };
                for (int i = 0; i < 2; ++i) {
                    const bool sel = (ui.gizmo_space == i);
                    const ImU32 col = sel ? IM_COL32(40, 150, 90, 240) : 0;
                    if (VectorIconButton(spaces[i], VectorIcon::None, spaces[i], ImVec2(0, 22), col)) {
                        ui.gizmo_space = i;
                    }
                    DrawTooltip(space_tips[i]);
                    ImGui::SameLine(0, 3.0f);
                }

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // Snap toggle button
                const bool snap_active = ui.snap_enabled;
                const ImU32 snap_col = snap_active ? IM_COL32(200, 130, 40, 240) : 0;
                if (VectorIconButton("vp_snap", VectorIcon::None, "Snap", ImVec2(0, 22), snap_col)) {
                    ui.snap_enabled = !ui.snap_enabled;
                }
                DrawTooltip("Toggle grid and angle snapping during transform manipulation.");

                if (ui.snap_enabled) {
                    ImGui::SameLine(0, 4.0f);
                    ImGui::SetNextItemWidth(45.0f);
                    ImGui::DragFloat("##snap_val", &ui.snap_position.x, 0.05f, 0.01f, 10.0f, "%.2f");
                    DrawTooltip("Position snap increment in meters.");
                }

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // Stats HUD toggle button
                const bool stats_active = ui.show_stats_overlay;
                const ImU32 stats_col = stats_active ? IM_COL32(110, 60, 210, 240) : 0;
                if (VectorIconButton("vp_stats", VectorIcon::None, "Stats HUD", ImVec2(0, 22), stats_col)) {
                    ui.show_stats_overlay = !ui.show_stats_overlay;
                }
                DrawTooltip("Toggle real-time performance & ray statistics HUD overlay.");

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // 3D Visualizers toggle button with Eye vector icon
                const bool viz_active = ui.show_visualizers;
                const ImU32 viz_col = viz_active ? IM_COL32(30, 160, 180, 240) : 0;
                if (VectorIconButton("vp_overlays", VectorIcon::Eye, "Overlays", ImVec2(82, 22), viz_col)) {
                    ui.show_visualizers = !ui.show_visualizers;
                }
                DrawTooltip("Toggle 3D visualizers for light bounds, camera frustums, and bounding boxes.");

                ImGui::SameLine(0, 4.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0, 4.0f);

                // Play Mode quick buttons in Viewport
                if (ui.play_state == UiState::PlayState::Edit) {
                    if (VectorIconButton("vp_play", VectorIcon::Play, "Play", ImVec2(65, 22), IM_COL32(38, 145, 75, 230))) {
                        ui.request_play = true;
                    }
                    DrawTooltip("Play Mode (Cmd+P / Ctrl+P): Snapshots scene and begins physics simulation.");
                } else {
                    if (VectorIconButton("vp_stop", VectorIcon::Stop, "Stop", ImVec2(65, 22), IM_COL32(185, 45, 45, 230))) {
                        ui.request_stop = true;
                    }
                    DrawTooltip("Stop Mode (Cmd+P / Ctrl+P): Restores initial scene state.");

                    ImGui::SameLine(0, 4.0f);
                    if (ui.play_state == UiState::PlayState::Playing) {
                        if (VectorIconButton("vp_pause", VectorIcon::Pause, "Pause", ImVec2(70, 22), IM_COL32(195, 145, 25, 230))) {
                            ui.request_pause = true;
                        }
                        DrawTooltip("Pause Simulation (Cmd+Shift+P / Ctrl+Shift+P)");
                    } else {
                        if (VectorIconButton("vp_resume", VectorIcon::Play, "Resume", ImVec2(75, 22), IM_COL32(38, 145, 75, 230))) {
                            ui.request_play = true;
                        }
                        DrawTooltip("Resume Simulation (Cmd+Shift+P / Ctrl+Shift+P)");

                        ImGui::SameLine(0, 4.0f);
                        if (VectorIconButton("vp_step", VectorIcon::Step, "Step", ImVec2(65, 22), IM_COL32(45, 105, 195, 230))) {
                            ui.request_step = true;
                        }
                        DrawTooltip("Step 1 Frame (Cmd+. / Ctrl+.)");
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleVar(6);
        ImGui::PopStyleColor(2);

        // ---- Top-Right Play State Badge ----
        if (ui.play_state != UiState::PlayState::Edit) {
            ImGui::SetCursorPos(ImVec2(img_offset.x + size.x - 120.0f, img_offset.y + 10.0f));
            float alpha = 0.9f;
            if (ui.enable_ui_animations) {
                alpha = 0.75f + iam_oscillate(ImGui::GetID("badge_pulse"), 0.2f, 2.5f, iam_wave_sine, 0.0f, ImGui::GetIO().DeltaTime);
            }
            if (ui.play_state == UiState::PlayState::Playing) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.50f, 0.25f, alpha));
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 200, 100, 240));
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.65f, 0.45f, 0.10f, alpha));
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(240, 180, 50, 240));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
            if (ImGui::BeginChild("PlayBadge", ImVec2(110.0f, 26.0f), 0, ImGuiWindowFlags_NoScrollbar)) {
                if (ui.play_state == UiState::PlayState::Playing) {
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "* PLAYING");
                    DrawTooltip("Simulation is active. Physics & gameplay ticks are running in real-time.");
                } else {
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "|| PAUSED");
                    DrawTooltip("Simulation is paused. Use Step (Cmd+.) to advance by 1 tick.");
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);
        }

        if (ui.show_stats_overlay) {
            DrawStatsOverlay(world, assets, stats, time, settings, ui, image_min, size);
        }

        if (ui.show_visualizers) {
            DrawViewportVisualizers(world, ui, camera, aspect, image_min, size);
        }

        DrawGizmo(world, ui, const_cast<CameraController&>(camera), aspect, image_min, size);
        DrawOrientationGizmo(world, ui, const_cast<CameraController&>(camera), image_min, size);
    } else {
        ui.viewport_width = 0;
        ui.viewport_height = 0;
        ui.viewport_hovered = false;
        ui.viewport_rmb = false;
    }
    ImGui::End();
}

void EditorUI::DrawGizmo(World& world, UiState& ui, CameraController& camera, f32 aspect,
                        const ImVec2& image_min, const ImVec2& image_size) {
    if (!ui.show_viewport || ui.selection == kNullEntity) return;

    Registry& registry = world.Entities();
    LocalTransform* local = registry.Get<LocalTransform>(ui.selection);
    if (!local) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(image_min.x, image_min.y, image_size.x, image_size.y);

    const CameraState& cam = camera.Camera();
    Mat4 view = glm::lookAt(cam.position, cam.position + cam.Forward(), cam.Up());
    Mat4 proj = glm::perspective(cam.fov_y, aspect, 0.1f, 1000.0f);

    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (ui.gizmo_operation == 1) op = ImGuizmo::ROTATE;
    if (ui.gizmo_operation == 2) op = ImGuizmo::SCALE;

    ImGuizmo::MODE mode = (ui.gizmo_space == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    Mat4 world_mat = LocalToWorldMatrix(registry, ui.selection);

    float* snap_ptr = nullptr;
    float snap_values[3] = {0, 0, 0};
    if (ui.snap_enabled) {
        if (ui.gizmo_operation == 0) {
            snap_values[0] = ui.snap_position.x;
            snap_values[1] = ui.snap_position.y;
            snap_values[2] = ui.snap_position.z;
        } else if (ui.gizmo_operation == 1) {
            snap_values[0] = ui.snap_rotation;
            snap_values[1] = ui.snap_rotation;
            snap_values[2] = ui.snap_rotation;
        } else if (ui.gizmo_operation == 2) {
            snap_values[0] = ui.snap_scale;
            snap_values[1] = ui.snap_scale;
            snap_values[2] = ui.snap_scale;
        }
        snap_ptr = snap_values;
    }

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                             op, mode, glm::value_ptr(world_mat),
                             nullptr, snap_ptr)) {
        Mat4 parent_inv = ParentWorldInverse(registry, ui.selection);
        Mat4 new_local  = parent_inv * world_mat;

        LocalTransform next = *local;
        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(new_local),
                                              matrixTranslation,
                                              matrixRotation,
                                              matrixScale);

        if (ui.gizmo_operation == 0) {
            next.position = Vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]);
        } else if (ui.gizmo_operation == 1) {
            Vec3 euler = glm::radians(Vec3(matrixRotation[0], matrixRotation[1], matrixRotation[2]));
            next.rotation = Quat(euler);
        } else if (ui.gizmo_operation == 2) {
            next.scale = Vec3(matrixScale[0], matrixScale[1], matrixScale[2]);
        }

        *local = next;
        UpdateWorldTransforms(registry);
        TrackEdit(registry, ui.selection, *local, "Gizmo Transform");
    } else {
        TrackEdit(registry, ui.selection, *local, "Gizmo Transform");
    }
}

void EditorUI::DrawViewportVisualizers(World& world, const UiState& ui, const CameraController& camera, f32 aspect,
                                     const ImVec2& image_min, const ImVec2& image_size) {
    if (!ui.show_visualizers || image_size.x <= 0.0f || image_size.y <= 0.0f) return;

    Registry& registry = world.Entities();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const CameraState& cam = camera.Camera();
    Mat4 view = glm::lookAt(cam.position, cam.position + cam.Forward(), cam.Up());
    Mat4 proj = glm::perspective(cam.fov_y, aspect, 0.1f, 1000.0f);
    Mat4 vp   = proj * view;

    auto project = [&](const Vec3& p_world, ImVec2& out_screen) -> bool {
        Vec4 clip = vp * Vec4(p_world, 1.0f);
        if (clip.w <= 0.01f) return false;
        Vec3 ndc = Vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        out_screen.x = image_min.x + (ndc.x * 0.5f + 0.5f) * image_size.x;
        out_screen.y = image_min.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * image_size.y;
        return true;
    };

    if (ui.selection != kNullEntity && registry.Valid(ui.selection)) {
        if (const LocalBounds* lb = registry.Get<LocalBounds>(ui.selection)) {
            Mat4 wm = LocalToWorldMatrix(registry, ui.selection);
            Vec3 corners[8] = {
                Vec3(wm * Vec4(lb->min.x, lb->min.y, lb->min.z, 1.0f)),
                Vec3(wm * Vec4(lb->max.x, lb->min.y, lb->min.z, 1.0f)),
                Vec3(wm * Vec4(lb->max.x, lb->max.y, lb->min.z, 1.0f)),
                Vec3(wm * Vec4(lb->min.x, lb->max.y, lb->min.z, 1.0f)),
                Vec3(wm * Vec4(lb->min.x, lb->min.y, lb->max.z, 1.0f)),
                Vec3(wm * Vec4(lb->max.x, lb->min.y, lb->max.z, 1.0f)),
                Vec3(wm * Vec4(lb->max.x, lb->max.y, lb->max.z, 1.0f)),
                Vec3(wm * Vec4(lb->min.x, lb->max.y, lb->max.z, 1.0f))
            };

            ImVec2 sc[8];
            bool visible[8];
            for (int i = 0; i < 8; ++i) visible[i] = project(corners[i], sc[i]);

            auto draw_edge = [&](int i, int j) {
                if (visible[i] && visible[j])
                    dl->AddLine(sc[i], sc[j], IM_COL32(255, 175, 40, 220), 1.5f);
            };

            draw_edge(0,1); draw_edge(1,2); draw_edge(2,3); draw_edge(3,0);
            draw_edge(4,5); draw_edge(5,6); draw_edge(6,7); draw_edge(7,4);
            draw_edge(0,4); draw_edge(1,5); draw_edge(2,6); draw_edge(3,7);
        }
    }
}

} // namespace lucida
