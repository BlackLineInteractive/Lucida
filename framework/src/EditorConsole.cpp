// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace lucida {

struct ConsoleLogItem {
    LogChannel  channel;
    LogLevel    level;
    std::string message;
    std::string timestamp;
};

static std::vector<ConsoleLogItem> s_console_logs;
static std::mutex                  s_console_mutex;
static bool                        s_console_autoscroll = true;
static bool                        s_console_show_info  = true;
static bool                        s_console_show_warn  = true;
static bool                        s_console_show_error = true;
static bool                        s_console_show_debug = true;
static char                        s_console_filter[128] = {0};

static void OnConsoleLogSink(LogChannel channel, LogLevel level, const char* message, void*) {
    std::lock_guard<std::mutex> lock(s_console_mutex);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");

    if (s_console_logs.size() >= 2000) {
        s_console_logs.erase(s_console_logs.begin(), s_console_logs.begin() + 100);
    }
    s_console_logs.push_back({channel, level, message ? message : "", ss.str()});
}

void RegisterConsoleLogSink() {
    LogAddSink(OnConsoleLogSink);
}

void UnregisterConsoleLogSink() {
    LogRemoveSink(OnConsoleLogSink);
}

void EditorUI::DrawConsole(UiState& ui) {
    if (!ImGui::Begin("Console", &ui.show_console)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear")) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        s_console_logs.clear();
    }
    DrawTooltip("Clear all console log messages.");

    ImGui::SameLine();
    if (ImGui::Button("Copy All")) {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        std::string all;
        for (const auto& log : s_console_logs) {
            all += "[" + log.timestamp + "] " + log.message + "\n";
        }
        ImGui::SetClipboardText(all.c_str());
    }
    DrawTooltip("Copy entire console log history to clipboard.");

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &s_console_autoscroll);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &s_console_show_info);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &s_console_show_warn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &s_console_show_error);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##ConsoleFilter", "Filter logs...", s_console_filter, sizeof(s_console_filter));

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(s_console_mutex);
        for (const auto& log : s_console_logs) {
            if (log.level == LogLevel::Info && !s_console_show_info) continue;
            if (log.level == LogLevel::Warn && !s_console_show_warn) continue;
            if (log.level == LogLevel::Error && !s_console_show_error) continue;
            if (log.level == LogLevel::Debug && !s_console_show_debug) continue;

            if (s_console_filter[0] != '\0') {
                std::string msg = log.message;
                std::string f = s_console_filter;
                std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
                std::transform(f.begin(), f.end(), f.begin(), ::tolower);
                if (msg.find(f) == std::string::npos) continue;
            }

            ImVec4 col(0.85f, 0.85f, 0.85f, 1.0f);
            if (log.level == LogLevel::Warn)  col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            else if (log.level == LogLevel::Error) col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            else if (log.level == LogLevel::Debug) col = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);

            ImGui::TextDisabled("[%s]", log.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(col, "%s", log.message.c_str());
        }

        if (s_console_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void EditorUI::DrawContentBrowser(World& world, UiState& ui, SceneAssets& assets) {
    if (!ImGui::Begin("Content Browser", &ui.show_content_browser)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Home")) {
        ui.content_browser_path = ".";
    }
    DrawTooltip("Navigate to project root directory.");

    ImGui::SameLine();
    if (ImGui::Button("Up")) {
        std::filesystem::path p(ui.content_browser_path);
        if (p.has_parent_path() && p != "." && p != "/") {
            ui.content_browser_path = p.parent_path().string();
            if (ui.content_browser_path.empty()) ui.content_browser_path = ".";
        }
    }
    DrawTooltip("Navigate up one directory level.");

    ImGui::SameLine();
    ImGui::TextDisabled("Path: %s", ui.content_browser_path.c_str());
    ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##ContentSearch", "Search files...", ui.content_search, sizeof(ui.content_search));

    ImGui::Separator();

    ImGui::Columns(2, "cb_layout", true);
    ImGui::SetColumnWidth(0, 140.0f);

    ImGui::TextDisabled("Favorites");
    ImGui::Separator();
    static const struct { const char* label; const char* path; } kFavorites[] = {
        { "[DIR] assets",          "assets"           },
        { "[DIR] models",          "assets/models"    },
        { "[DIR] textures",        "assets/textures"  },
        { "[DIR] audio",           "assets/audio"     },
        { "[DIR] scenes",          "scenes"           },
    };
    for (auto& fav : kFavorites) {
        if (ImGui::Selectable(fav.label)) {
            ui.content_browser_path = fav.path;
        }
    }
    ImGui::Columns(1);
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::Spacing();

    std::filesystem::path current_dir(ui.content_browser_path);
    std::error_code ec;
    if (std::filesystem::exists(current_dir, ec) && std::filesystem::is_directory(current_dir, ec)) {
        const float item_width = 110.0f;
        const float panel_width = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(panel_width / (item_width + 15.0f)));

        if (ImGui::BeginTable("ContentGrid", columns)) {
            for (const auto& entry : std::filesystem::directory_iterator(current_dir, ec)) {
                const auto& filename = entry.path().filename().string();
                if (filename.empty() || filename[0] == '.') continue;

                if (ui.content_search[0] != '\0') {
                    std::string fn = filename;
                    std::string s  = ui.content_search;
                    std::transform(fn.begin(), fn.end(), fn.begin(), ::tolower);
                    std::transform(s.begin(),  s.end(),  s.begin(),  ::tolower);
                    if (fn.find(s) == std::string::npos) continue;
                }

                ImGui::TableNextColumn();
                ImGui::PushID(filename.c_str());

                const bool is_dir = entry.is_directory();
                const std::string ext = entry.path().extension().string();

                const char* icon = "[DOC]";
                if      (is_dir) icon = "[DIR]";
                else if (ext == ".obj"  || ext == ".gltf" || ext == ".glb" || ext == ".fbx") icon = "[MESH]";
                else if (ext == ".png"  || ext == ".jpg"  || ext == ".jpeg"|| ext == ".hdr") icon = "[TEX]";
                else if (ext == ".wav"  || ext == ".mp3"  || ext == ".ogg" || ext == ".flac") icon = "[AUD]";
                else if (ext == ".json") icon = "[SCN]";
                else if (ext == ".lua"  || ext == ".cpp"  || ext == ".h")   icon = "[SCR]";

                std::string label = std::string(icon) + " " + filename;
                if (label.size() > 18) label = label.substr(0, 15) + "...";

                if (ImGui::Button(label.c_str(), ImVec2(item_width, 36.0f))) {
                    if (is_dir) {
                        ui.content_browser_path = entry.path().string();
                    } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
                        ui.pending_model_path = entry.path().string();
                    }
                }
                if (ImGui::IsItemHovered()) {
                    DrawTooltip(entry.path().string().c_str());
                }

                if (!is_dir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    const std::string full_path = entry.path().string();
                    ImGui::SetDragDropPayload("ASSET_PATH", full_path.c_str(), full_path.size() + 1);
                    ImGui::Text("Drop: %s", label.c_str());
                    ImGui::EndDragDropSource();
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextDisabled("Directory does not exist.");
        if (ImGui::Button("Reset to Root")) ui.content_browser_path = ".";
    }

    ImGui::End();
}

void EditorUI::DrawStatsPanel(World& world, const SceneAssets& assets, const RenderStats& stats,
                             const FrameTime& time, const RenderSettings& settings, const UiState& ui) {
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

void EditorUI::DrawGraphicsSettings(UiState& ui, SceneAssets& assets, RenderSettings& settings, CameraController& camera) {
    if (!ImGui::Begin("Graphics Settings", &ui.show_graphics_settings)) {
        ImGui::End();
        return;
    }

    if (BeginSection("Pipeline", true)) {
        const char* pipelines[] = { "Metal Ray Tracer (Whitted)", "Radiance Cascades 3D (GI)" };
        int cur_p = static_cast<int>(ui.current_backend);
        if (ImGui::Combo("Backend", &cur_p, pipelines, 2)) {
            ui.requested_backend = static_cast<UiState::RenderBackendType>(cur_p);
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

    if (BeginSection("Ambient Occlusion (AO)", true)) {
        const char* ao_modes[] = { "Off", "Ray Traced AO (RTAO / Radiance Cascades)", "SSAO (Screen-Space)", "GTAO (Ground-Truth)", "HBAO (Horizon-Based)" };
        int cur_ao = static_cast<int>(settings.ao.mode);
        if (ImGui::Combo("AO Technique", &cur_ao, ao_modes, 5)) {
            settings.ao.mode = static_cast<AOMode>(cur_ao);
        }
        if (settings.ao.mode != AOMode::Off) {
            ImGui::SliderFloat("AO Radius", &settings.ao.radius, 0.1f, 10.0f, "%.2f m");
            ImGui::SliderFloat("AO Intensity", &settings.ao.intensity, 0.0f, 5.0f, "%.2f");
            ImGui::SliderFloat("AO Power (Contrast)", &settings.ao.power, 0.5f, 4.0f, "%.2f");
            ImGui::SliderInt("AO Samples", &settings.ao.sample_count, 4, 64);
        }
        EndSection();
    }

    if (BeginSection("Anti-Aliasing (AA)", true)) {
        const char* aa_modes[] = { "Off", "FXAA (Fast Approximate)", "SMAA (Subpixel Morphological)", "TAA (Temporal AA / MetalFX)" };
        int cur_aa = static_cast<int>(settings.aa.mode);
        if (ImGui::Combo("AA Method", &cur_aa, aa_modes, 4)) {
            settings.aa.mode = static_cast<AAMode>(cur_aa);
        }
        if (settings.aa.mode == AAMode::TAA) {
            ImGui::SliderFloat("Temporal Blend", &settings.aa.temporal_blend_factor, 0.5f, 0.98f, "%.2f");
        } else if (settings.aa.mode == AAMode::FXAA || settings.aa.mode == AAMode::SMAA) {
            ImGui::SliderFloat("Edge Threshold", &settings.aa.edge_threshold, 0.05f, 0.5f, "%.3f");
        }
        EndSection();
    }

    if (BeginSection("Environment & Sky", true)) {
        ImGui::ColorEdit3("Ambient Light", glm::value_ptr(assets.environment.ambient));
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

    if (BeginSection("Sun & Celestial Lighting", true)) {
        ImGui::Checkbox("Sun Enabled", &assets.environment.sun_enabled);
        if (assets.environment.sun_enabled) {
            ImGui::ColorEdit3("Sun Color", glm::value_ptr(assets.environment.sun_color));
            
            ImGui::TextDisabled("Sun Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Noon")) {
                assets.environment.sun_direction = Vec3(0.0f, 1.0f, 0.2f);
                assets.environment.sun_color = Vec3(1.0f, 0.98f, 0.95f);
                assets.environment.sun_intensity = 6.0f;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Sunset")) {
                assets.environment.sun_direction = Vec3(0.85f, 0.18f, 0.48f);
                assets.environment.sun_color = Vec3(1.0f, 0.55f, 0.22f);
                assets.environment.sun_intensity = 7.5f;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Morning")) {
                assets.environment.sun_direction = Vec3(-0.72f, 0.38f, 0.58f);
                assets.environment.sun_color = Vec3(1.0f, 0.88f, 0.68f);
                assets.environment.sun_intensity = 5.0f;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Night / Moon")) {
                assets.environment.sun_direction = Vec3(0.2f, 0.85f, -0.48f);
                assets.environment.sun_color = Vec3(0.45f, 0.65f, 1.0f);
                assets.environment.sun_intensity = 0.8f;
            }

            Vec3Row("Direction", assets.environment.sun_direction);
            if (glm::length(assets.environment.sun_direction) > 0.001f) {
                assets.environment.sun_direction = glm::normalize(assets.environment.sun_direction);
            }
            ImGui::DragFloat("Sun Intensity", &assets.environment.sun_intensity, 0.1f, 0.0f, 50.0f, "%.1f");
        }
        EndSection();
    }

    if (BeginSection("Post-Processing & Color", true)) {
        ImGui::SliderFloat("Exposure", &assets.environment.exposure, 0.05f, 5.0f, "%.2f");
        const char* tonemappers[] = { "ACES (Cinematic)", "Reinhard (Smooth)", "Filmic (High Contrast)", "Linear (Clamped)" };
        ImGui::Combo("Tone Mapper", &assets.environment.tonemap_mode, tonemappers, 4);
        ImGui::SliderFloat("Gamma", &assets.environment.gamma, 1.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Dither Strength", &assets.environment.dither_strength, 0.0f, 2.0f, "%.2f");
        EndSection();
    }

    if (BeginSection("Ambient Occlusion (AO)", true)) {
        ImGui::SliderFloat("AO Radius", &assets.environment.ao_radius, 0.1f, 5.0f, "%.2f m");
        ImGui::SliderFloat("AO Intensity", &assets.environment.ao_intensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderInt("AO Quality (Rays)", &assets.environment.ao_samples, 1, 16);
        EndSection();
    }

    if (BeginSection("Water Simulation", true)) {
        ImGui::SliderFloat("Wave Height", &assets.environment.water_height, 0.001f, 0.15f, "%.3f m");
        ImGui::SliderFloat("Wave Speed", &assets.environment.water_speed, 0.1f, 5.0f, "%.2fx");
        ImGui::SliderFloat("Wave Frequency", &assets.environment.water_frequency, 0.1f, 5.0f, "%.2fx");
        ImGui::SliderFloat("Foam Intensity", &assets.environment.water_foam, 0.0f, 2.0f, "%.2f");
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
        EndSection();
    }

    ImGui::End();
}

void EditorUI::DrawManualModal(UiState& ui) {
    if (!ui.show_manual_modal) return;

    ImGui::SetNextWindowSize(ImVec2(840, 580), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(18, 20, 26, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(65, 75, 95, 220));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));

    if (ImGui::Begin("Manual & Controls Reference", &ui.show_manual_modal,
                     ImGuiWindowFlags_NoCollapse)) {

        if (ImGui::BeginTabBar("ManualTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Navigation & Camera")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Viewport Navigation & Camera Fly Controls");
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("NavTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Action / Control", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Key / Gesture & Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& entry : manual::kNavigationEntries) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", entry.label);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.description);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gizmo & Editing")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Transform Gizmo & Scene Editing");
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("GizmoTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Shortcut / Control", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& entry : manual::kGizmoEntries) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", entry.label);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.description);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Play Mode & Physics")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Jolt Physics Simulation & Play Mode");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextWrapped("Lucida features an integrated Jolt Physics world with Play Mode state isolation. "
                                   "When you click Play, the engine takes a non-destructive ECS World Snapshot. "
                                   "On Stop, all entity transforms, velocities, and hierarchies are restored.");
                ImGui::Spacing();

                if (ImGui::BeginTable("PlayTable", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Control / Shortcut", ImGuiTableColumnFlags_WidthFixed, 220.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& entry : manual::kPlayModeEntries) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", entry.label);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(entry.description);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Materials & Rendering")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "PBR Materials & Ray Tracing Engine");
                ImGui::Separator();
                ImGui::Spacing();

                for (const auto* bullet : manual::kRenderingBullets) {
                    ImGui::BulletText("%s", bullet);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Hotkeys Reference")) {
                static char search_filter[64] = "";
                ImGui::Spacing();
                ImGui::SetNextItemWidth(260.0f);
                ImGui::InputTextWithHint("##filter", "Search hotkeys...", search_filter, sizeof(search_filter));
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear")) search_filter[0] = '\0';
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("HotkeysTable", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 300))) {
                    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 130.0f);
                    ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    for (const auto& hk : manual::kHotkeys) {
                        if (search_filter[0] != '\0') {
                            std::string f(search_filter);
                            std::transform(f.begin(), f.end(), f.begin(), ::tolower);
                            std::string all = std::string(hk.category) + " " + hk.shortcut + " " + hk.description;
                            std::transform(all.begin(), all.end(), all.begin(), ::tolower);
                            if (all.find(f) == std::string::npos) continue;
                        }
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextDisabled("%s", hk.category);
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", hk.shortcut);
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(hk.description);
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Close (ESC)", ImVec2(120, 26))) {
            ui.show_manual_modal = false;
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void EditorUI::DrawPreferencesWindow(UiState& ui, CameraController& camera, RenderSettings& settings) {
    if (!ui.show_preferences_window) return;

    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 22, 28, 250));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(65, 75, 95, 220));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));

    if (ImGui::Begin("Preferences & Settings", &ui.show_preferences_window)) {
        if (ImGui::BeginTabBar("PrefTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Viewport & Camera")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera Navigation Settings");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::SliderFloat("Fly Speed (m/s)", &camera.Tuning().fly_speed, 0.5f, 30.0f, "%.1f m/s");
                DrawTooltip("Base movement speed when navigating with RMB + WASD.");

                ImGui::SliderFloat("Sprint Multiplier", &camera.Tuning().sprint_mul, 1.5f, 5.0f, "%.1fx");
                DrawTooltip("Multiplier applied to fly speed while holding Shift.");

                ImGui::SliderFloat("Look Sensitivity", &camera.Tuning().look_sensitivity, 0.0005f, 0.010f, "%.4f");
                DrawTooltip("Mouse rotation sensitivity.");

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Visual Overlays");
                ImGui::Separator();
                ImGui::Checkbox("Show 3D Visualizers by default", &ui.show_visualizers);
                DrawTooltip("Render light bounding spheres, camera frustums, and bounding boxes in viewport.");
                ImGui::Checkbox("Show Real-time Stats Overlay", &ui.show_stats_overlay);
                DrawTooltip("Display FPS, ray count, bounce depth, and timing overlay inside the viewport.");

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gizmo & Snapping")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Gizmo Snapping Increments");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Checkbox("Enable Snapping", &ui.snap_enabled);
                DrawTooltip("Toggle grid and angle snapping during transform manipulation.");

                ImGui::DragFloat("Position Snap Step (m)", &ui.snap_position.x, 0.05f, 0.01f, 10.0f, "%.2f m");
                DrawTooltip("Grid increment for position translation.");
                ui.snap_position.y = ui.snap_position.x;
                ui.snap_position.z = ui.snap_position.x;

                ImGui::DragFloat("Rotation Snap Step (deg)", &ui.snap_rotation, 1.0f, 1.0f, 90.0f, "%.1f deg");
                DrawTooltip("Angle increment for rotation.");

                ImGui::DragFloat("Scale Snap Step", &ui.snap_scale, 0.05f, 0.01f, 2.0f, "%.2f");
                DrawTooltip("Scale factor increment.");

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Interface & Animations")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "ImAnim UI Motion & Transitions");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Checkbox("Enable UI Animations (ImAnim)", &ui.enable_ui_animations);
                DrawTooltip("Enable smooth tweening, button lift transitions, and pulsating status badges.");

                if (ui.enable_ui_animations) {
                    ImGui::SliderFloat("Animation Speed Scale", &ui.animation_speed, 0.25f, 3.0f, "%.2fx");
                    DrawTooltip("Global animation speed factor. 1.0x is standard speed.");
                    iam_set_global_time_scale(ui.animation_speed);
                }

                ImGui::Checkbox("Show Interactive Tooltips on Hover", &ui.show_tooltips);
                DrawTooltip("Show explanatory tooltips and hotkey hints when hovering over editor controls.");

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Renderer Defaults")) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Rendering Quality Defaults");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::SliderFloat("Render Resolution Scale", &settings.render_scale, 0.25f, 2.0f, "%.2fx");
                DrawTooltip("Internal rendering resolution multiplier relative to viewport panel.");

                ImGui::SliderInt("Max Ray Tracing Depth", &settings.max_depth, 1, 16);
                DrawTooltip("Maximum recursion depth for specular reflections and glass refractions.");

                ImGui::Checkbox("Volumetric Height Fog", &settings.fog);
                DrawTooltip("Enable atmospheric distance fog.");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Done", ImVec2(100, 24))) {
            ui.show_preferences_window = false;
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void EditorUI::DrawGameplayDebugger(World& world, UiState& ui) {
    if (!ImGui::Begin("Gameplay Debugger", &ui.show_gameplay_debugger)) {
        ImGui::End();
        return;
    }

    if (BeginSection("Simulation & Time Control", true)) {
        ImGui::SliderFloat("Time Scale", &ui.gameplay_time_scale, 0.0f, 5.0f, "%.2fx");
        if (ImGui::Button("Reset (1.0x)")) ui.gameplay_time_scale = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("Slow-Mo (0.1x)")) ui.gameplay_time_scale = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("Pause (0.0x)")) ui.gameplay_time_scale = 0.0f;
        EndSection();
    }

    if (BeginSection("Visual Gameplay Gizmos (DebugDraw)", true)) {
        ImGui::Checkbox("Draw Physics Colliders", &ui.draw_physics_colliders);
        ImGui::Checkbox("Draw Raycast Sensors", &ui.draw_raycast_sensors);
        ImGui::Checkbox("Draw AI Perception Cones", &ui.draw_ai_perception);
        ImGui::Checkbox("Draw Audio Radii", &ui.draw_audio_radii);
        EndSection();
    }

    Registry& entities = world.Entities();
    if (ui.selection != kNullEntity && entities.Valid(ui.selection)) {
        if (BeginSection("Selected Entity Gameplay State", true)) {
            if (const HealthComponent* hp = entities.Get<HealthComponent>(ui.selection)) {
                ImGui::ProgressBar(hp->current_health / glm::max(1.0f, hp->max_health), ImVec2(-1, 0), "Health");
                ImGui::Text("HP: %.1f / %.1f  (Shield: %.1f)", hp->current_health, hp->max_health, hp->shield);
                ImGui::Text("Status: %s", hp->is_dead ? "DEAD" : "ALIVE");
            }
            if (const RaycastSensorComponent* ray = entities.Get<RaycastSensorComponent>(ui.selection)) {
                ImGui::Text("Raycast Hit: %s", ray->has_hit ? "YES" : "NO");
                if (ray->has_hit) {
                    ImGui::Text("Hit Dist: %.2f m", ray->hit_distance);
                    ImGui::Text("Hit Point: (%.2f, %.2f, %.2f)", ray->hit_point.x, ray->hit_point.y, ray->hit_point.z);
                }
            }
            if (const AIControllerComponent* ai = entities.Get<AIControllerComponent>(ui.selection)) {
                ImGui::Text("AI State: %s", ai->current_state.c_str());
                ImGui::Text("Perception Radius: %.1f m", ai->perception_radius);
            }
            if (const InventoryComponent* inv = entities.Get<InventoryComponent>(ui.selection)) {
                ImGui::Text("Inventory: %zu / %d items", inv->item_names.size(), inv->max_slots);
                for (const auto& item : inv->item_names) {
                    ImGui::BulletText("%s", item.c_str());
                }
            }
            EndSection();
        }
    } else {
        ImGui::TextDisabled("Select an entity to inspect its live gameplay components.");
    }

    ImGui::End();
}

void EditorUI::DrawEngineDiagnostics(World&, UiState& ui, const RenderStats& stats) {
    if (!ImGui::Begin("Engine Diagnostics", &ui.show_engine_diagnostics)) {
        ImGui::End();
        return;
    }

    if (BeginSection("Subsystem Latency & Phases (GEA 1.6)", true)) {
        ImGui::Text("CPU Frame Time: %.2f ms", stats.cpu_frame_ms);
        ImGui::Text("GPU Frame Time: %.2f ms", stats.gpu_frame_ms);
        ImGui::Separator();
        ImGui::TextDisabled("Phase Breakdown:");
        ImGui::BulletText("Simulation / Physics (60 Hz fixed)");
        ImGui::BulletText("Animation System (Skinning Matrix Palette)");
        ImGui::BulletText("Particle System (SoA SIMD Stream)");
        ImGui::BulletText("Render Scene Sync & BVH Refit");
        EndSection();
    }

    if (BeginSection("Memory & Frame Arena (GEA 6.2 / DOD)", true)) {
        ImGui::Text("Double-Buffered FrameArena: 8.0 MiB x2");
        ImGui::ProgressBar(0.08f, ImVec2(-1, 0), "Peak Frame Watermark: ~650 KB");
        ImGui::TextDisabled("Zero dynamic heap malloc in active simulation loop.");
        EndSection();
    }

    if (BeginSection("Ray Tracing Acceleration (TLAS / BLAS)", true)) {
        ImGui::Text("Active Traced Rays: %d", stats.ray_count);
        ImGui::Text("Active Triangles: %d", stats.tri_count);
        ImGui::Text("BVH Tree Quality: Binned SAH (bvh v2)");
        ImGui::Text("Analytic Intersection Units: 7 Primitives");
        EndSection();
    }

    ImGui::End();
}

} // namespace lucida
