#include "lucida/framework/Theme.h"

#include "im_anim.h"
#include "imgui.h"

#include <vector>

namespace lucida {
namespace {

ImVec4 ToImVec4(const f32 c[4]) { return ImVec4(c[0], c[1], c[2], c[3]); }

ImVec4 WithAlpha(const ImVec4& c, f32 a) { return ImVec4(c.x, c.y, c.z, a); }

// Indents pushed by BeginSection, so EndSection removes exactly what was added
// even while the tween is still moving.
std::vector<f32> g_section_indent;

} // namespace

void ApplyTheme(const ThemeColors& colors) {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding    = 7.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 5.0f;
    style.TabRounding       = 5.0f;
    style.WindowPadding     = ImVec2(12, 11);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(8, 7);
    style.ItemInnerSpacing  = ImVec2(7, 5);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 11.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.SeparatorTextBorderSize = 2.0f;
    style.SeparatorTextPadding    = ImVec2(18, 4);

    const ImVec4 accent     = ToImVec4(colors.accent);
    const ImVec4 accent_hi  = ToImVec4(colors.accent_hi);
    const ImVec4 accent_dim = ToImVec4(colors.accent_dim);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.075f, 0.075f, 0.085f, 0.98f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.055f, 0.055f, 0.065f, 0.92f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.085f, 0.085f, 0.098f, 0.99f);
    c[ImGuiCol_Border]            = ImVec4(0.24f, 0.24f, 0.28f, 0.60f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.135f, 0.135f, 0.155f, 1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.195f, 0.195f, 0.225f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.245f, 0.245f, 0.285f, 1.00f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.065f, 0.065f, 0.075f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.105f, 0.105f, 0.125f, 1.00f);
    c[ImGuiCol_MenuBarBg]         = ImVec4(0.095f, 0.095f, 0.110f, 1.00f);
    c[ImGuiCol_Header]            = WithAlpha(accent_dim, 0.55f);
    c[ImGuiCol_HeaderHovered]     = WithAlpha(accent, 0.45f);
    c[ImGuiCol_HeaderActive]      = WithAlpha(accent, 0.62f);
    c[ImGuiCol_Button]            = ImVec4(0.165f, 0.165f, 0.195f, 1.00f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.245f, 0.245f, 0.285f, 1.00f);
    c[ImGuiCol_ButtonActive]      = WithAlpha(accent, 0.75f);
    c[ImGuiCol_SliderGrab]        = accent;
    c[ImGuiCol_SliderGrabActive]  = accent_hi;
    c[ImGuiCol_CheckMark]         = accent_hi;
    c[ImGuiCol_Tab]               = ImVec4(0.105f, 0.105f, 0.125f, 1.00f);
    c[ImGuiCol_TabHovered]        = WithAlpha(accent, 0.50f);
    c[ImGuiCol_TabActive]         = ImVec4(0.135f, 0.165f, 0.205f, 1.00f);
    c[ImGuiCol_Separator]         = ImVec4(0.24f, 0.24f, 0.28f, 0.70f);
    c[ImGuiCol_SeparatorHovered]  = accent;
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.050f, 0.050f, 0.058f, 0.85f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.215f, 0.215f, 0.250f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.290f, 0.290f, 0.330f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = accent;
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.24f, 0.24f, 0.28f, 0.50f);
    c[ImGuiCol_ResizeGripHovered] = WithAlpha(accent, 0.60f);
    c[ImGuiCol_ResizeGripActive]  = accent;
    c[ImGuiCol_Text]              = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.48f, 0.48f, 0.53f, 1.00f);
    c[ImGuiCol_TextSelectedBg]    = WithAlpha(accent, 0.35f);
    c[ImGuiCol_NavHighlight]      = accent;
    c[ImGuiCol_TableHeaderBg]     = ImVec4(0.125f, 0.125f, 0.145f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = ImVec4(0.24f, 0.24f, 0.28f, 0.80f);
    c[ImGuiCol_TableBorderLight]  = ImVec4(0.18f, 0.18f, 0.21f, 0.60f);
    c[ImGuiCol_TableRowBgAlt]     = ImVec4(1.00f, 1.00f, 1.00f, 0.020f);
}

bool AnimatedButton(const char* label, f32 width) {
    ImGui::PushID(label);

    const ImVec2 size(width > 0.0f ? width : 0.0f, 0.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    // Measure first, then draw the background under the label ourselves: the
    // tweened colour has to land behind the text, not over it.
    const ImVec2 text = ImGui::CalcTextSize(label);
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 box(size.x > 0.0f ? size.x : text.x + style.FramePadding.x * 2.0f,
                     text.y + style.FramePadding.y * 2.0f);

    const bool pressed = ImGui::InvisibleButton("##btn", box);
    const bool hovered = ImGui::IsItemHovered();
    const bool held    = ImGui::IsItemActive();

    const f32 lift = iam_tween_float(ImGui::GetID("##btn"), ImGui::GetID("lift"),
                                     held ? 1.0f : (hovered ? 0.6f : 0.0f), 0.16f,
                                     iam_ease_preset(iam_ease_out_cubic),
                                     iam_policy_crossfade, ImGui::GetIO().DeltaTime);

    const ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    const ImVec4 warm = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    const ImVec4 bg(base.x + (warm.x - base.x) * lift,
                    base.y + (warm.y - base.y) * lift,
                    base.z + (warm.z - base.z) * lift, 1.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + box.x, origin.y + box.y),
                      ImGui::GetColorU32(bg), style.FrameRounding);
    dl->AddText(ImVec2(origin.x + (box.x - text.x) * 0.5f, origin.y + style.FramePadding.y),
                ImGui::GetColorU32(ImGuiCol_Text), label);

    ImGui::PopID();
    return pressed;
}

bool BeginSection(const char* label, bool default_open) {
    const ImGuiTreeNodeFlags flags = default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    const bool open = ImGui::CollapsingHeader(label, flags);
    if (!open) return false;

    // Contents ease in from the left instead of appearing at their final indent.
    const f32 indent = iam_tween_float(ImGui::GetID(label), ImGui::GetID("indent"),
                                       ImGui::GetStyle().IndentSpacing * 0.5f, 0.22f,
                                       iam_ease_preset(iam_ease_out_cubic),
                                       iam_policy_crossfade, ImGui::GetIO().DeltaTime);
    g_section_indent.push_back(indent);
    ImGui::Indent(indent);
    return true;
}

void EndSection() {
    if (g_section_indent.empty()) return;
    ImGui::Unindent(g_section_indent.back());
    g_section_indent.pop_back();
}

} // namespace lucida
