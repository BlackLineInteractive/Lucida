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

        // Multi-Selection Box & Lasso interactive logic
        const bool shift_held = ImGui::GetIO().KeyShift;
        const bool ctrl_held = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
        const bool is_multi = shift_held || ctrl_held;

        if (image_hovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
            const ImVec2 mouse = ImGui::GetMousePos();

            if (ui.select_tool == UiState::SelectTool::Point) {
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                    ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x == 0.0f &&
                    ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y == 0.0f) {
                    const Vec2 ndc((mouse.x - image_min.x) / size.x * 2.0f - 1.0f,
                                   1.0f - (mouse.y - image_min.y) / size.y * 2.0f);
                    const Ray ray = RayThroughViewport(camera.Camera(), aspect, ndc);
                    const PickResult hit = PickEntity(world.Entities(), ray);
                    if (hit.entity != kNullEntity) {
                        ui.SetSelected(hit.entity, !ui.IsSelected(hit.entity) || !is_multi, is_multi);
                    } else if (!is_multi) {
                        ui.DeselectAll();
                    }
                }
            } else if (ui.select_tool == UiState::SelectTool::Box) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ui.is_box_selecting = true;
                    ui.selection_drag_start = Vec2(mouse.x, mouse.y);
                }
                if (ui.is_box_selecting) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        ImVec2 bmin(std::min(ui.selection_drag_start.x, mouse.x), std::min(ui.selection_drag_start.y, mouse.y));
                        ImVec2 bmax(std::max(ui.selection_drag_start.x, mouse.x), std::max(ui.selection_drag_start.y, mouse.y));
                        ImGui::GetWindowDrawList()->AddRectFilled(bmin, bmax, IM_COL32(0, 150, 255, 45));
                        ImGui::GetWindowDrawList()->AddRect(bmin, bmax, IM_COL32(50, 180, 255, 230), 0.0f, 0, 1.5f);
                    } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        ui.is_box_selecting = false;
                        ImVec2 bmin(std::min(ui.selection_drag_start.x, mouse.x), std::min(ui.selection_drag_start.y, mouse.y));
                        ImVec2 bmax(std::max(ui.selection_drag_start.x, mouse.x), std::max(ui.selection_drag_start.y, mouse.y));

                        if (!is_multi) ui.DeselectAll();

                        const CameraState& c = camera.Camera();
                        Mat4 view = glm::lookAt(c.position, c.position + c.Forward(), c.Up());
                        Mat4 proj = glm::perspective(c.fov_y, aspect, 0.1f, 1000.0f);
                        Mat4 vp = proj * view;

                        for (auto [e, name] : world.Entities().View<Name>().each()) {
                            Mat4 wm = LocalToWorldMatrix(world.Entities(), e);
                            Vec3 p_world(wm[3]);
                            Vec4 clip = vp * Vec4(p_world, 1.0f);
                            if (clip.w <= 0.01f) continue;
                            Vec3 ndc = Vec3(clip) / clip.w;
                            if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
                            float sx = image_min.x + (ndc.x * 0.5f + 0.5f) * size.x;
                            float sy = image_min.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y;
                            if (sx >= bmin.x && sx <= bmax.x && sy >= bmin.y && sy <= bmax.y) {
                                ui.SetSelected(e, true, true);
                            }
                        }
                    }
                }
            } else if (ui.select_tool == UiState::SelectTool::Lasso) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ui.is_lasso_selecting = true;
                    ui.lasso_points.clear();
                    ui.lasso_points.push_back(Vec2(mouse.x, mouse.y));
                }
                if (ui.is_lasso_selecting) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (ui.lasso_points.empty() || 
                            (std::abs(mouse.x - ui.lasso_points.back().x) > 3.0f || std::abs(mouse.y - ui.lasso_points.back().y) > 3.0f)) {
                            ui.lasso_points.push_back(Vec2(mouse.x, mouse.y));
                        }
                        if (ui.lasso_points.size() >= 2) {
                            std::vector<ImVec2> draw_pts;
                            draw_pts.reserve(ui.lasso_points.size());
                            for (const auto& pt : ui.lasso_points) draw_pts.emplace_back(pt.x, pt.y);

                            ImGui::GetWindowDrawList()->AddPolyline(draw_pts.data(), static_cast<int>(draw_pts.size()),
                                                                    IM_COL32(255, 170, 0, 230), ImDrawFlags_None, 2.0f);
                            if (draw_pts.size() >= 3) {
                                ImGui::GetWindowDrawList()->AddConvexPolyFilled(draw_pts.data(), static_cast<int>(draw_pts.size()),
                                                                               IM_COL32(255, 170, 0, 35));
                            }
                        }
                    } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        ui.is_lasso_selecting = false;
                        if (ui.lasso_points.size() >= 3) {
                            if (!is_multi) ui.DeselectAll();

                            const CameraState& c = camera.Camera();
                            Mat4 view = glm::lookAt(c.position, c.position + c.Forward(), c.Up());
                            Mat4 proj = glm::perspective(c.fov_y, aspect, 0.1f, 1000.0f);
                            Mat4 vp = proj * view;

                            auto pointInPoly = [](const Vec2& pt, const std::vector<Vec2>& poly) -> bool {
                                bool inside = false;
                                for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
                                    if (((poly[i].y > pt.y) != (poly[j].y > pt.y)) &&
                                        (pt.x < (poly[j].x - poly[i].x) * (pt.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
                                        inside = !inside;
                                    }
                                }
                                return inside;
                            };

                            for (auto [e, name] : world.Entities().View<Name>().each()) {
                                Mat4 wm = LocalToWorldMatrix(world.Entities(), e);
                                Vec3 p_world(wm[3]);
                                Vec4 clip = vp * Vec4(p_world, 1.0f);
                                if (clip.w <= 0.01f) continue;
                                Vec3 ndc = Vec3(clip) / clip.w;
                                if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
                                float sx = image_min.x + (ndc.x * 0.5f + 0.5f) * size.x;
                                float sy = image_min.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * size.y;
                                if (pointInPoly(Vec2(sx, sy), ui.lasso_points)) {
                                    ui.SetSelected(e, true, true);
                                }
                            }
                        }
                        ui.lasso_points.clear();
                    }
                }
            }
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

                // Selection Tool (Point, Box, Lasso)
                const char* sel_tools[] = { "Point", "Box", "Lasso" };
                int cur_sel = static_cast<int>(ui.select_tool);
                ImGui::SetNextItemWidth(70.0f);
                if (ImGui::Combo("##SelectTool", &cur_sel, sel_tools, 3)) {
                    ui.select_tool = static_cast<UiState::SelectTool>(cur_sel);
                }
                DrawTooltip("Selection Tool: Point (Single/Shift Pick), Box (Marquee Drag), or Lasso (Freehand Polygon).");

                ImGui::SameLine(0, 4.0f);

                // Camera Source Selector
                const char* cam_sources[] = { "Fly Cam", "Game Cam" };
                int cur_cam = static_cast<int>(ui.camera_source);
                ImGui::SetNextItemWidth(85.0f);
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
                // 3D Visualizers toggle button with Overlays settings dropdown
                const bool viz_active = ui.show_visualizers;
                const ImU32 viz_col = viz_active ? IM_COL32(30, 160, 180, 240) : 0;
                if (VectorIconButton("vp_overlays", VectorIcon::Eye, "Overlays", ImVec2(75, 22), viz_col)) {
                    ui.show_visualizers = !ui.show_visualizers;
                }
                DrawTooltip("Toggle 3D visualizers for lights, cameras, and selection bounds.");
                ImGui::SameLine(0, 1.0f);
                if (ImGui::Button("▼##OverlaysMenu", ImVec2(18, 22))) {
                    ImGui::OpenPopup("ViewportOverlaysPopup");
                }
                if (ImGui::BeginPopup("ViewportOverlaysPopup")) {
                    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "Viewport Overlays");
                    ImGui::Separator();
                    ImGui::Checkbox("Enable 3D Overlays", &ui.show_visualizers);
                    ImGui::Checkbox("Light Icons & Rays", &ui.show_light_visualizers);
                    ImGui::Checkbox("Camera Frustums", &ui.show_camera_frustums);
                    ImGui::Checkbox("Selection Corner Brackets", &ui.show_selection_bounds);
                    ImGui::Checkbox("Physics Colliders", &ui.show_collider_wireframes);
                    ImGui::Checkbox("Performance HUD", &ui.show_stats_overlay);
                    ImGui::EndPopup();
                }

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

    auto draw_3d_line = [&](const Vec3& a, const Vec3& b, ImU32 col, float thickness = 1.2f) {
        ImVec2 sa, sb;
        if (project(a, sa) && project(b, sb)) {
            dl->AddLine(sa, sb, col, thickness);
        }
    };

    auto draw_3d_circle = [&](const Vec3& center, const Vec3& normal, float radius, ImU32 col, int segments = 24, float thickness = 1.0f) {
        Vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : Vec3(0, 1, 0);
        Vec3 u = std::abs(n.y) < 0.99f ? glm::normalize(glm::cross(n, Vec3(0, 1, 0))) : glm::normalize(glm::cross(n, Vec3(1, 0, 0)));
        Vec3 v = glm::cross(n, u);

        ImVec2 prev_s;
        bool has_prev = false;
        const float step = 6.2831853f / static_cast<float>(segments);
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) * step;
            Vec3 p = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
            ImVec2 cur_s;
            bool vis = project(p, cur_s);
            if (vis) {
                if (has_prev) {
                    dl->AddLine(prev_s, cur_s, col, thickness);
                }
                prev_s = cur_s;
                has_prev = true;
            } else {
                has_prev = false;
            }
        }
    };

    auto draw_3d_sphere_rings = [&](const Vec3& center, float radius, ImU32 col, float thickness = 1.0f) {
        draw_3d_circle(center, Vec3(0, 1, 0), radius, col, 24, thickness); // XZ (horizontal)
        draw_3d_circle(center, Vec3(1, 0, 0), radius, col, 24, thickness); // YZ (vertical)
        draw_3d_circle(center, Vec3(0, 0, 1), radius, col, 24, thickness); // XY (vertical)
    };

    // 1. Light Source 3D Icons, Rays and Influence Bounds
    if (ui.show_light_visualizers) {
        for (auto [entity, ls] : registry.View<LightSource>().each()) {
            Mat4 wm = LocalToWorldMatrix(registry, entity);
            Vec3 light_pos = Vec3(wm * Vec4(0, 0, 0, 1));
            const bool is_sel = ui.IsSelected(entity);

            const int cr = std::clamp(static_cast<int>(ls.color.r * 255.0f), 0, 255);
            const int cg = std::clamp(static_cast<int>(ls.color.g * 255.0f), 0, 255);
            const int cb = std::clamp(static_cast<int>(ls.color.b * 255.0f), 0, 255);
            const ImU32 light_col = IM_COL32(cr, cg, cb, 255);
            const ImU32 wire_col  = is_sel ? IM_COL32(255, 230, 80, 240) : IM_COL32(cr, cg, cb, 140);

            ImVec2 screen_pos;
            const bool on_screen = project(light_pos, screen_pos);

            // Draw glowing 3D billboard bulb glyph icon at light position
            if (on_screen) {
                // Outer glow aura
                dl->AddCircleFilled(screen_pos, is_sel ? 9.0f : 7.0f, IM_COL32(cr, cg, cb, is_sel ? 90 : 50));
                // Core bulb disk
                dl->AddCircleFilled(screen_pos, is_sel ? 5.5f : 4.5f, light_col);
                // Center bright highlight
                dl->AddCircleFilled(screen_pos, 2.0f, IM_COL32(255, 255, 255, 255));

                // 8 Starburst sun rays sticking out of the light glyph
                for (int k = 0; k < 8; ++k) {
                    float ang = static_cast<float>(k) * 3.14159265f * 0.25f;
                    float r0 = is_sel ? 6.5f : 5.5f;
                    float r1 = is_sel ? 11.5f : 9.5f;
                    dl->AddLine(
                        ImVec2(screen_pos.x + std::cos(ang) * r0, screen_pos.y + std::sin(ang) * r0),
                        ImVec2(screen_pos.x + std::cos(ang) * r1, screen_pos.y + std::sin(ang) * r1),
                        is_sel ? IM_COL32(255, 240, 120, 255) : light_col,
                        is_sel ? 1.8f : 1.2f
                    );
                }

                // If selected: draw selection ring
                if (is_sel) {
                    dl->AddCircle(screen_pos, 14.0f, IM_COL32(80, 227, 194, 255), 0, 1.5f);
                }
            }

            // 3D Geometry representation per light type
            if (ls.type == LightType::Point) {
                if (is_sel) {
                    const float r = std::clamp(ls.radius, 0.5f, 50.0f);
                    draw_3d_sphere_rings(light_pos, r, wire_col, is_sel ? 1.5f : 1.0f);
                }
            } else if (ls.type == LightType::Directional) {
                Vec3 dir = ls.direction;
                if (glm::length(dir) < 0.001f) dir = Vec3(0, -1, 0);
                dir = glm::normalize(Vec3(wm * Vec4(dir, 0.0f)));

                Vec3 u = std::abs(dir.y) < 0.99f ? glm::normalize(glm::cross(dir, Vec3(0, 1, 0))) : glm::normalize(glm::cross(dir, Vec3(1, 0, 0)));
                Vec3 v = glm::cross(dir, u);

                // Draw 4 directional parallel arrows
                const float ray_len = 2.4f;
                const float ring_r = 0.7f;
                for (int k = 0; k < 4; ++k) {
                    float ang = static_cast<float>(k) * 3.14159265f * 0.5f;
                    Vec3 r_start = light_pos + (u * std::cos(ang) + v * std::sin(ang)) * ring_r;
                    Vec3 r_end   = r_start + dir * ray_len;
                    draw_3d_line(r_start, r_end, wire_col, is_sel ? 1.8f : 1.2f);

                    // Arrowhead
                    Vec3 side = (u * std::cos(ang) + v * std::sin(ang)) * 0.18f - dir * 0.28f;
                    draw_3d_line(r_end, r_end + side, wire_col, 1.2f);
                }
                draw_3d_circle(light_pos, dir, ring_r, wire_col, 16, 1.0f);
            } else if (ls.type == LightType::Spot) {
                Vec3 dir = ls.direction;
                if (glm::length(dir) < 0.001f) dir = Vec3(0, -1, 0);
                dir = glm::normalize(Vec3(wm * Vec4(dir, 0.0f)));

                Vec3 u = std::abs(dir.y) < 0.99f ? glm::normalize(glm::cross(dir, Vec3(0, 1, 0))) : glm::normalize(glm::cross(dir, Vec3(1, 0, 0)));
                Vec3 v = glm::cross(dir, u);

                const float dist = std::clamp(ls.radius, 1.5f, 15.0f);
                const float rad_outer = glm::radians(std::clamp(ls.outer_angle * 0.5f, 1.0f, 89.0f));
                const float rad_inner = glm::radians(std::clamp(ls.inner_angle * 0.5f, 1.0f, 89.0f));
                const float r_outer = dist * std::tan(rad_outer);
                const float r_inner = dist * std::tan(rad_inner);

                Vec3 base_pos = light_pos + dir * dist;

                // Base circle outer & inner
                draw_3d_circle(base_pos, dir, r_outer, wire_col, 24, is_sel ? 1.5f : 1.0f);
                if (ls.inner_angle > 0.1f && ls.inner_angle < ls.outer_angle) {
                    draw_3d_circle(base_pos, dir, r_inner, IM_COL32(cr, cg, cb, 80), 24, 1.0f);
                }

                // 4 Cone generator lines
                draw_3d_line(light_pos, base_pos + u * r_outer, wire_col, 1.0f);
                draw_3d_line(light_pos, base_pos - u * r_outer, wire_col, 1.0f);
                draw_3d_line(light_pos, base_pos + v * r_outer, wire_col, 1.0f);
                draw_3d_line(light_pos, base_pos - v * r_outer, wire_col, 1.0f);
            }
        }
    }

    // 2. In-Scene Game Cameras 3D Frustums & Up-Guides
    if (ui.show_camera_frustums) {
        for (auto [entity, cam_comp] : registry.View<CameraComponent>().each()) {
            Mat4 wm = LocalToWorldMatrix(registry, entity);
            Vec3 cam_pos = Vec3(wm * Vec4(0, 0, 0, 1));
            const bool is_sel = ui.IsSelected(entity);

            Vec3 fwd = -glm::normalize(Vec3(wm[2]));
            Vec3 up  =  glm::normalize(Vec3(wm[1]));
            Vec3 rgt =  glm::normalize(Vec3(wm[0]));

            const ImU32 cam_col = is_sel ? IM_COL32(199, 146, 234, 255) : IM_COL32(160, 120, 200, 180);

            // Screen glyph icon
            ImVec2 screen_pos;
            if (project(cam_pos, screen_pos)) {
                dl->AddCircleFilled(screen_pos, is_sel ? 7.0f : 5.0f, cam_col);
                dl->AddCircleFilled(screen_pos, 2.5f, IM_COL32(255, 255, 255, 255));
                if (is_sel) {
                    dl->AddCircle(screen_pos, 12.0f, IM_COL32(80, 227, 194, 255), 0, 1.5f);
                }
            }

            // Draw Camera Frustum
            const float d_near = std::max(cam_comp.near_clip, 0.3f);
            const float d_far  = std::clamp(cam_comp.far_clip, 1.5f, 5.0f);
            const float half_fov = glm::radians(std::clamp(cam_comp.fov * 0.5f, 5.0f, 85.0f));

            const float hn = d_near * std::tan(half_fov);
            const float wn = hn * aspect;
            const float hf = d_far * std::tan(half_fov);
            const float wf = hf * aspect;

            Vec3 n0 = cam_pos + fwd * d_near - rgt * wn + up * hn;
            Vec3 n1 = cam_pos + fwd * d_near + rgt * wn + up * hn;
            Vec3 n2 = cam_pos + fwd * d_near + rgt * wn - up * hn;
            Vec3 n3 = cam_pos + fwd * d_near - rgt * wn - up * hn;

            Vec3 f0 = cam_pos + fwd * d_far - rgt * wf + up * hf;
            Vec3 f1 = cam_pos + fwd * d_far + rgt * wf + up * hf;
            Vec3 f2 = cam_pos + fwd * d_far + rgt * wf - up * hf;
            Vec3 f3 = cam_pos + fwd * d_far - rgt * wf - up * hf;

            // Pyramid edges
            draw_3d_line(cam_pos, f0, cam_col, 1.0f);
            draw_3d_line(cam_pos, f1, cam_col, 1.0f);
            draw_3d_line(cam_pos, f2, cam_col, 1.0f);
            draw_3d_line(cam_pos, f3, cam_col, 1.0f);

            // Far rectangle
            draw_3d_line(f0, f1, cam_col, is_sel ? 1.6f : 1.0f);
            draw_3d_line(f1, f2, cam_col, is_sel ? 1.6f : 1.0f);
            draw_3d_line(f2, f3, cam_col, is_sel ? 1.6f : 1.0f);
            draw_3d_line(f3, f0, cam_col, is_sel ? 1.6f : 1.0f);

            // Near rectangle
            draw_3d_line(n0, n1, cam_col, 1.0f);
            draw_3d_line(n1, n2, cam_col, 1.0f);
            draw_3d_line(n2, n3, cam_col, 1.0f);
            draw_3d_line(n3, n0, cam_col, 1.0f);

            // Top orientation tick (showing camera "Up")
            Vec3 top_mid = (f0 + f1) * 0.5f;
            Vec3 top_peak = top_mid + up * (hf * 0.35f);
            draw_3d_line(top_mid - rgt * (wf * 0.2f), top_peak, cam_col, 1.2f);
            draw_3d_line(top_peak, top_mid + rgt * (wf * 0.2f), cam_col, 1.2f);
        }
    }

    // 3. Selection Corner Brackets (Logical & Clean, No Obtrusive Wireframe Cage)
    if (ui.show_selection_bounds) {
        std::vector<Entity> targets = ui.selections;
        if (targets.empty() && ui.selection != kNullEntity) {
            targets.push_back(ui.selection);
        }

        for (Entity sel_entity : targets) {
            if (!registry.Valid(sel_entity)) continue;

            // Skip drawing bounding box for Lights, Cameras, and Groups (they have their own 3D glyphs)
            if (registry.Get<LightSource>(sel_entity) || registry.Get<CameraComponent>(sel_entity) || registry.Get<GroupComponent>(sel_entity)) {
                continue;
            }

            LocalBounds bounds{};
            bool has_bounds = false;
            if (const LocalBounds* lb = registry.Get<LocalBounds>(sel_entity)) {
                bounds = *lb;
                has_bounds = true;
            } else if (const PrimitiveShape* ps = registry.Get<PrimitiveShape>(sel_entity)) {
                Vec3 he = ps->HalfExtents();
                bounds.min = -he;
                bounds.max = he;
                has_bounds = true;
            } else if (const EditableMeshComponent* emc = registry.Get<EditableMeshComponent>(sel_entity)) {
                if (!emc->mesh.vertices.empty()) {
                    bounds.min = emc->mesh.vertices[0].position;
                    bounds.max = emc->mesh.vertices[0].position;
                    for (const auto& v : emc->mesh.vertices) {
                        bounds.min = glm::min(bounds.min, v.position);
                        bounds.max = glm::max(bounds.max, v.position);
                    }
                    has_bounds = true;
                }
            }

            if (!has_bounds) continue;

            const bool is_primary = (sel_entity == ui.selection);
            const ImU32 bracket_col = is_primary ? IM_COL32(80, 227, 194, 240) : IM_COL32(120, 200, 255, 190);
            const float line_thick  = is_primary ? 1.8f : 1.2f;

            Mat4 wm = LocalToWorldMatrix(registry, sel_entity);

            const Vec3 bmin = bounds.min;
            const Vec3 bmax = bounds.max;
            const float lx = bmax.x - bmin.x;
            const float ly = bmax.y - bmin.y;
            const float lz = bmax.z - bmin.z;

            // Corner bracket length: 25% of edge dimension
            const float bx = lx * 0.25f;
            const float by = ly * 0.25f;
            const float bz = lz * 0.25f;

            auto draw_corner = [&](const Vec3& origin, const Vec3& dx, const Vec3& dy, const Vec3& dz) {
                Vec3 w_origin = Vec3(wm * Vec4(origin, 1.0f));
                Vec3 w_px     = Vec3(wm * Vec4(origin + dx, 1.0f));
                Vec3 w_py     = Vec3(wm * Vec4(origin + dy, 1.0f));
                Vec3 w_pz     = Vec3(wm * Vec4(origin + dz, 1.0f));

                draw_3d_line(w_origin, w_px, bracket_col, line_thick);
                draw_3d_line(w_origin, w_py, bracket_col, line_thick);
                draw_3d_line(w_origin, w_pz, bracket_col, line_thick);
            };

            // 8 Clean Corner Brackets
            draw_corner(Vec3(bmin.x, bmin.y, bmin.z), Vec3( bx, 0, 0), Vec3(0,  by, 0), Vec3(0, 0,  bz));
            draw_corner(Vec3(bmax.x, bmin.y, bmin.z), Vec3(-bx, 0, 0), Vec3(0,  by, 0), Vec3(0, 0,  bz));
            draw_corner(Vec3(bmax.x, bmax.y, bmin.z), Vec3(-bx, 0, 0), Vec3(0, -by, 0), Vec3(0, 0,  bz));
            draw_corner(Vec3(bmin.x, bmax.y, bmin.z), Vec3( bx, 0, 0), Vec3(0, -by, 0), Vec3(0, 0,  bz));

            draw_corner(Vec3(bmin.x, bmin.y, bmax.z), Vec3( bx, 0, 0), Vec3(0,  by, 0), Vec3(0, 0, -bz));
            draw_corner(Vec3(bmax.x, bmin.y, bmax.z), Vec3(-bx, 0, 0), Vec3(0,  by, 0), Vec3(0, 0, -bz));
            draw_corner(Vec3(bmax.x, bmax.y, bmax.z), Vec3(-bx, 0, 0), Vec3(0, -by, 0), Vec3(0, 0, -bz));
            draw_corner(Vec3(bmin.x, bmax.y, bmax.z), Vec3( bx, 0, 0), Vec3(0, -by, 0), Vec3(0, 0, -bz));
        }
    }
}

} // namespace lucida
