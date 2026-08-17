// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "lucida/framework/EditorUI.h"
#include "lucida/core/diag/Log.h"
#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/Picking.h"
#include "lucida/framework/Theme.h"
#include "lucida/framework/Manual.h"
#include "lucida/framework/Script.h"
#include "lucida/framework/Systems.h"
#include "lucida/audio/Components.h"
#include "lucida/animation/Skeleton.h"
#include "lucida/animation/AnimationSystem.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/resource/MeshBuilder.h"
#include "lucida/resource/TextureManager.h"
#include "lucida/resource/Terrain.h"
#include "lucida/resource/Prefab.h"
#include "lucida/runtime/World.h"
#include "lucida/runtime/GameplayComponents.h"
#include "lucida/runtime/Particles.h"
#include "lucida/runtime/DebugDraw.h"

#include "ImGuiFileDialog.h"
#include "ImGuizmo.h"
#include "im_anim.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace lucida {

void RegisterConsoleLogSink();
void UnregisterConsoleLogSink();

inline Mat4 LocalToWorldMatrix(const Registry& registry, Entity entity) {
    if (entity == kNullEntity || !registry.Valid(entity)) return Mat4(1.0f);

    Mat4 local_mat(1.0f);
    if (const LocalTransform* lt = registry.Get<LocalTransform>(entity)) {
        local_mat = lt->ToMatrix();
    } else if (const WorldTransform* wt = registry.Get<WorldTransform>(entity)) {
        return wt->matrix;
    }

    if (const Parent* p = registry.Get<Parent>(entity)) {
        if (p->entity != kNullEntity && registry.Valid(p->entity)) {
            return LocalToWorldMatrix(registry, p->entity) * local_mat;
        }
    }
    return local_mat;
}

inline Mat4 ParentWorldInverse(const Registry& registry, Entity entity) {
    if (entity == kNullEntity || !registry.Valid(entity)) return Mat4(1.0f);
    if (const Parent* p = registry.Get<Parent>(entity)) {
        if (p->entity != kNullEntity && registry.Valid(p->entity)) {
            return glm::inverse(LocalToWorldMatrix(registry, p->entity));
        }
    }
    return Mat4(1.0f);
}

static bool g_show_tooltips = true;

inline void DrawTooltip(const char* text) {
    if (!g_show_tooltips) return;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    DrawTooltip(desc);
}

enum class VectorIcon {
    None,
    Play,
    Stop,
    Pause,
    Step,
    Focus,
    Eye,
    Camera,
    DownArrow,
    LeftArrow,
    RightArrow
};

inline void DrawVectorIcon(ImDrawList* dl, VectorIcon icon, ImVec2 center, float size, ImU32 col) {
    const float half = size * 0.5f;
    switch (icon) {
    case VectorIcon::Play: {
        const ImVec2 p1(center.x - half * 0.65f, center.y - half * 0.85f);
        const ImVec2 p2(center.x + half * 0.85f, center.y);
        const ImVec2 p3(center.x - half * 0.65f, center.y + half * 0.85f);
        dl->AddTriangleFilled(p1, p2, p3, col);
        break;
    }
    case VectorIcon::Stop: {
        const float r = half * 0.70f;
        dl->AddRectFilled(ImVec2(center.x - r, center.y - r),
                          ImVec2(center.x + r, center.y + r), col, 1.5f);
        break;
    }
    case VectorIcon::Pause: {
        const float bw = size * 0.22f;
        const float bh = size * 0.80f;
        const float gap = size * 0.16f;
        dl->AddRectFilled(ImVec2(center.x - gap - bw, center.y - bh * 0.5f),
                          ImVec2(center.x - gap, center.y + bh * 0.5f), col, 1.0f);
        dl->AddRectFilled(ImVec2(center.x + gap, center.y - bh * 0.5f),
                          ImVec2(center.x + gap + bw, center.y + bh * 0.5f), col, 1.0f);
        break;
    }
    case VectorIcon::Step: {
        const ImVec2 p1(center.x - half * 0.80f, center.y - half * 0.80f);
        const ImVec2 p2(center.x + half * 0.20f, center.y);
        const ImVec2 p3(center.x - half * 0.80f, center.y + half * 0.80f);
        dl->AddTriangleFilled(p1, p2, p3, col);
        dl->AddRectFilled(ImVec2(center.x + half * 0.40f, center.y - half * 0.80f),
                          ImVec2(center.x + half * 0.75f, center.y + half * 0.80f), col, 1.0f);
        break;
    }
    case VectorIcon::Focus: {
        const float r = half * 0.75f;
        dl->AddCircle(center, r, col, 16, 1.6f);
        dl->AddCircleFilled(center, r * 0.35f, col);
        dl->AddLine(ImVec2(center.x - r * 1.35f, center.y), ImVec2(center.x + r * 1.35f, center.y), col, 1.2f);
        dl->AddLine(ImVec2(center.x, center.y - r * 1.35f), ImVec2(center.x, center.y + r * 1.35f), col, 1.2f);
        break;
    }
    case VectorIcon::Eye: {
        const float rx = half * 0.95f;
        const float ry = half * 0.52f;
        dl->AddEllipse(center, ImVec2(rx, ry), col, 0.0f, 16, 1.6f);
        dl->AddCircleFilled(center, ry * 0.55f, col);
        break;
    }
    case VectorIcon::Camera: {
        const float w = half * 1.5f;
        const float h = half * 1.0f;
        dl->AddRect(ImVec2(center.x - w * 0.5f, center.y - h * 0.4f),
                    ImVec2(center.x + w * 0.5f, center.y + h * 0.6f), col, 2.0f, 0, 1.4f);
        dl->AddCircle(ImVec2(center.x, center.y + h * 0.1f), h * 0.28f, col, 12, 1.3f);
        dl->AddRectFilled(ImVec2(center.x - w * 0.25f, center.y - h * 0.65f),
                          ImVec2(center.x + w * 0.25f, center.y - h * 0.4f), col, 1.0f);
        break;
    }
    case VectorIcon::DownArrow: {
        const float r = half * 0.55f;
        dl->AddTriangleFilled(
            ImVec2(center.x - r, center.y - r * 0.5f),
            ImVec2(center.x + r, center.y - r * 0.5f),
            ImVec2(center.x, center.y + r * 0.7f),
            col);
        break;
    }
    case VectorIcon::LeftArrow: {
        const float r = half * 0.55f;
        dl->AddTriangleFilled(
            ImVec2(center.x + r * 0.55f, center.y - r * 0.75f),
            ImVec2(center.x + r * 0.55f, center.y + r * 0.75f),
            ImVec2(center.x - r * 0.70f, center.y),
            col);
        break;
    }
    case VectorIcon::RightArrow: {
        const float r = half * 0.55f;
        dl->AddTriangleFilled(
            ImVec2(center.x - r * 0.55f, center.y - r * 0.75f),
            ImVec2(center.x - r * 0.55f, center.y + r * 0.75f),
            ImVec2(center.x + r * 0.70f, center.y),
            col);
        break;
    }
    default:
        break;
    }
}

inline bool VectorIconButton(const char* str_id, VectorIcon icon, const char* label,
                            const ImVec2& size_arg = ImVec2(0, 0), ImU32 bg_override = 0) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const ImGuiID id = window->GetID(str_id);
    const ImVec2 label_size = label && label[0] ? ImGui::CalcTextSize(label, nullptr, true) : ImVec2(0, 0);
    const float icon_w = (icon != VectorIcon::None) ? 14.0f : 0.0f;
    const float gap    = (icon != VectorIcon::None && label && label[0]) ? 5.0f : 0.0f;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + icon_w + gap + ImGui::GetStyle().FramePadding.x * 2.0f,
                                      label_size.y + ImGui::GetStyle().FramePadding.y * 2.0f);

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, ImGui::GetStyle().FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    const float dt = ImGui::GetIO().DeltaTime;
    const iam_ease_desc ez_quad{iam_ease_out_quad, 0, 0, 0, 0};
    const float hover_t = iam_tween_float(id, 0, (hovered || held) ? 1.0f : 0.0f, 0.14f, ez_quad, iam_policy_crossfade, dt, 0.0f);
    const float press_t = iam_tween_float(id, 1, held ? 1.0f : 0.0f, 0.08f, ez_quad, iam_policy_crossfade, dt, 0.0f);

    ImU32 col;
    if (bg_override != 0) {
        ImVec4 base_col = ImGui::ColorConvertU32ToFloat4(bg_override);
        ImVec4 col_vec = base_col;
        if (press_t > 0.001f) {
            col_vec = iam_get_blended_color(col_vec, ImVec4(1.0f, 1.0f, 1.0f, base_col.w), press_t * 0.28f, iam_col_srgb);
        } else if (hover_t > 0.001f) {
            col_vec = iam_get_blended_color(col_vec, ImVec4(1.0f, 1.0f, 1.0f, base_col.w), hover_t * 0.16f, iam_col_srgb);
        }
        col = ImGui::ColorConvertFloat4ToU32(col_vec);
    } else {
        ImVec4 idle_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
        ImVec4 hov_col  = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
        ImVec4 act_col  = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
        ImVec4 cur_col  = iam_get_blended_color(idle_col, hov_col, hover_t, iam_col_srgb);
        if (press_t > 0.001f) {
            cur_col = iam_get_blended_color(cur_col, act_col, press_t, iam_col_srgb);
        }
        col = ImGui::ColorConvertFloat4ToU32(cur_col);
    }

    ImGui::RenderNavHighlight(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImGui::GetStyle().FrameRounding);

    const float y_offset = press_t * 1.0f;
    float content_w = icon_w + gap + label_size.x;
    float start_x   = bb.Min.x + (bb.GetWidth() - content_w) * 0.5f;

    if (icon != VectorIcon::None) {
        ImVec2 icon_center(start_x + icon_w * 0.5f, bb.Min.y + bb.GetHeight() * 0.5f + y_offset);
        DrawVectorIcon(window->DrawList, icon, icon_center, icon_w, ImGui::GetColorU32(ImGuiCol_Text));
        start_x += icon_w + gap;
    }

    if (label && label[0]) {
        ImVec2 text_pos(start_x, bb.Min.y + (bb.GetHeight() - label_size.y) * 0.5f + y_offset);
        ImGui::RenderText(text_pos, label);
    }

    return pressed;
}

// Unit mode: 0 = meters, 1 = centimeters
static int g_units_mode = 0;

inline float ToDisplay(float v)   { return g_units_mode == 1 ? v * 100.0f : v; }
inline float FromDisplay(float v) { return g_units_mode == 1 ? v * 0.01f : v; }
inline const char* UnitSuffix()   { return g_units_mode == 1 ? " cm" : " m"; }
inline float DragSpeed()          { return g_units_mode == 1 ? 1.0f : 0.01f; }

inline bool Vec3Row(const char* label, Vec3& value, f32 speed = 0.01f) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(90.0f);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::DragFloat3("##v", &value.x, speed);
    ImGui::PopID();
    return changed;
}

inline bool Vec3RowUnits(const char* label, Vec3& value) {
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine(90.0f);
    ImGui::SetNextItemWidth(-60.0f);
    Vec3 disp(ToDisplay(value.x), ToDisplay(value.y), ToDisplay(value.z));
    bool changed = ImGui::DragFloat3("##v", &disp.x, DragSpeed(), 0.0f, 0.0f,
                                     g_units_mode == 1 ? "%.1f" : "%.3f");
    if (changed) {
        value.x = FromDisplay(disp.x);
        value.y = FromDisplay(disp.y);
        value.z = FromDisplay(disp.z);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", UnitSuffix());
    ImGui::PopID();
    return changed;
}

inline bool DragFloatUnits(const char* label, float& value, float min_v = 0.001f, float max_v = 1000.0f) {
    float disp = ToDisplay(value);
    float spd  = DragSpeed();
    bool changed = ImGui::DragFloat(label, &disp, spd, ToDisplay(min_v), ToDisplay(max_v),
                                    g_units_mode == 1 ? "%.1f" : "%.3f");
    if (changed) value = FromDisplay(disp);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", UnitSuffix());
    return changed;
}

inline void LabelledText(const char* label, const char* fmt, ...) {
    ImGui::TextUnformatted(label);
    ImGui::SameLine(90.0f);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

} // namespace lucida
