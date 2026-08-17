// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

void EditorUI::Init() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyTheme();
    RegisterConsoleLogSink();
}

void EditorUI::Shutdown() {
    UnregisterConsoleLogSink();
    ImGui::DestroyContext();
}

void EditorUI::Build(World& world, SceneAssets& assets, UiState& ui, RenderSettings& settings,
                    const RenderStats& stats, CameraController& camera,
                    const FrameTime& time, void* viewport_texture, f32 viewport_aspect,
                    IRenderBackend* renderer) {
    LUCIDA_PROFILE("editor-ui");

    g_show_tooltips = ui.show_tooltips;
    iam_update_begin_frame();

    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    if (!ui.show_menu) {
        ImGui::Render();
        return;
    }

    const f32 fps = time.real_delta > 0.0f ? 1.0f / time.real_delta : 0.0f;
    m_fps_ema = m_fps_ema * 0.92f + fps * 0.08f;

    const ImGuiID dockspace_id = ImGui::GetID("LucidaDockSpace");
    if (m_reset_layout || ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        BuildDefaultLayout(dockspace_id);
        m_reset_layout = false;
    }

    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    DrawMenuBar(world, assets, ui);

    const ImGuiIO& io = ImGui::GetIO();
    const bool modifier = io.KeySuper || io.KeyCtrl;   // Cmd on macOS, Ctrl elsewhere
    if (modifier && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) m_commands.Redo(); else m_commands.Undo();
    }
    if (modifier && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        m_commands.Redo();
    }

    // Global keyboard shortcuts outside text fields
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
            ui.show_manual_modal = !ui.show_manual_modal;
        }
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_Comma, false)) {
            ui.show_preferences_window = !ui.show_preferences_window;
        }

        if (modifier && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
            if (io.KeyShift) {
                if (ui.play_state == UiState::PlayState::Playing) ui.request_pause = true;
                else if (ui.play_state == UiState::PlayState::Paused) ui.request_play = true;
            } else {
                if (ui.play_state == UiState::PlayState::Edit) ui.request_play = true;
                else ui.request_stop = true;
            }
        }
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_Period, false)) {
            if (ui.play_state == UiState::PlayState::Paused) ui.request_step = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
            if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
                Entity to_del = ui.selection;
                ui.selection = kNullEntity;
                m_commands.Execute(std::make_unique<DestroyEntityCommand>(world.Entities(), to_del, "Delete Entity"));
            }
        }
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
                EntitySnapshot snap = EntitySnapshot::Capture(world.Entities(), ui.selection);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(world.Entities());
                ui.selection = dup;
                m_commands.Push(std::make_unique<CreateEntityCommand>(world.Entities(), dup, snap, "Duplicate Entity"));
            }
        }
    }

    if (ui.show_viewport && viewport_texture)
        DrawViewport(world, ui, viewport_texture, viewport_aspect, camera, assets, stats, settings, time);
    if (ui.show_hierarchy)          DrawHierarchy(world, ui, assets);
    if (ui.show_inspector)          DrawInspector(world, ui, assets, camera, renderer);
    if (ui.show_mesh_editor)        DrawMeshModeling(world, ui, assets, renderer);
    if (ui.show_texture_browser)    DrawTextureBrowser(ui, assets);
    if (ui.show_gameplay_debugger)  DrawGameplayDebugger(world, ui);
    if (ui.show_engine_diagnostics) DrawEngineDiagnostics(world, ui, stats);
    if (ui.show_graphics_settings)  DrawGraphicsSettings(ui, assets, settings, camera);
    if (ui.show_content_browser)    DrawContentBrowser(world, ui, assets);
    if (ui.show_console)            DrawConsole(ui);
    if (ui.show_stats_panel)        DrawStatsPanel(world, assets, stats, time, settings, ui);

    if (ui.show_manual_modal)       DrawManualModal(ui);
    if (ui.show_preferences_window) DrawPreferencesWindow(ui, camera, settings);

    if (ImGuiFileDialog::Instance()->Display("LoadModel", ImGuiWindowFlags_NoCollapse,
                                             ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            ui.pending_model_path = ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Render();
}

void EditorUI::BuildDefaultLayout(unsigned dockspace_id) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id,
                              ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    ImGuiID centre = dockspace_id;
    const ImGuiID left   = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left,  0.20f, nullptr, &centre);
    const ImGuiID right  = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.24f, nullptr, &centre);
    ImGuiID bottom       = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down,  0.26f, nullptr, &centre);
    const ImGuiID bottom_right = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.45f, nullptr, &bottom);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Mesh Editor", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Gameplay Debugger", right);
    ImGui::DockBuilderDockWindow("Engine Diagnostics", right);
    ImGui::DockBuilderDockWindow("Graphics Settings", right);
    ImGui::DockBuilderDockWindow("Content Browser", bottom);
    ImGui::DockBuilderDockWindow("Texture Maps", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom_right);
    ImGui::DockBuilderDockWindow("Statistics", bottom_right);
    ImGui::DockBuilderDockWindow("Viewport", centre);

    ImGui::DockBuilderFinish(dockspace_id);
}

void EditorUI::DrawMenuBar(World& world, SceneAssets& assets, UiState& ui) {
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

    if (ImGui::BeginMenu("Edit")) {
        char undo_label[64];
        if (m_commands.CanUndo())
            std::snprintf(undo_label, sizeof(undo_label), "Undo %s", m_commands.UndoName());
        else
            std::snprintf(undo_label, sizeof(undo_label), "Undo");

        char redo_label[64];
        if (m_commands.CanRedo())
            std::snprintf(redo_label, sizeof(redo_label), "Redo %s", m_commands.RedoName());
        else
            std::snprintf(redo_label, sizeof(redo_label), "Redo");

        if (ImGui::MenuItem(undo_label, "Cmd+Z", false, m_commands.CanUndo())) {
            m_commands.Undo();
        }
        if (ImGui::MenuItem(redo_label, "Cmd+Shift+Z", false, m_commands.CanRedo())) {
            m_commands.Redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Undo History", nullptr, false, m_commands.CanUndo() || m_commands.CanRedo())) {
            m_commands.Clear();
        }
        ImGui::Separator();
        Registry& entities = world.Entities();
        const bool has_selection = (ui.selection != kNullEntity && entities.Valid(ui.selection));
        if (ImGui::MenuItem("Duplicate Selection", "Cmd+D", false, has_selection)) {
            EntitySnapshot snap = EntitySnapshot::Capture(entities, ui.selection);
            snap.name += "_copy";
            snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
            Entity dup = snap.Restore(entities);
            ui.selection = dup;
            m_commands.Push(std::make_unique<CreateEntityCommand>(entities, dup, snap, "Duplicate Entity"));
        }
        if (ImGui::MenuItem("Delete Selection", "Del", false, has_selection)) {
            Entity to_del = ui.selection;
            ui.selection = kNullEntity;
            m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Entity"));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...", "Cmd+, / Ctrl+,")) {
            ui.show_preferences_window = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Viewport", nullptr, &ui.show_viewport);
        ImGui::MenuItem("Hierarchy", nullptr, &ui.show_hierarchy);
        ImGui::MenuItem("Inspector", nullptr, &ui.show_inspector);
        ImGui::MenuItem("Mesh Editor", nullptr, &ui.show_mesh_editor);
        ImGui::MenuItem("Gameplay Debugger", nullptr, &ui.show_gameplay_debugger);
        ImGui::MenuItem("Engine Diagnostics", nullptr, &ui.show_engine_diagnostics);
        ImGui::MenuItem("Graphics Settings", nullptr, &ui.show_graphics_settings);
        ImGui::MenuItem("Content Browser", nullptr, &ui.show_content_browser);
        ImGui::MenuItem("Texture Maps", nullptr, &ui.show_texture_browser);
        ImGui::MenuItem("Console", nullptr, &ui.show_console);
        ImGui::Separator();
        ImGui::MenuItem("Statistics (HUD Overlay)", nullptr, &ui.show_stats_overlay);
        ImGui::MenuItem("Statistics (Docked Panel)", nullptr, &ui.show_stats_panel);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset layout")) m_reset_layout = true;
        if (ImGui::MenuItem("Fullscreen", "F11")) ui.request_fullscreen = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Renderer")) {
        if (ImGui::MenuItem("Metal Ray Tracer (Whitted RT)", nullptr, ui.current_backend == UiState::RenderBackendType::MetalRayTracing)) {
            ui.requested_backend = UiState::RenderBackendType::MetalRayTracing;
        }
        if (ImGui::MenuItem("Radiance Cascades 3D (GI)", nullptr, ui.current_backend == UiState::RenderBackendType::RadianceCascades3D)) {
            ui.requested_backend = UiState::RenderBackendType::RadianceCascades3D;
        }
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

    if (ImGui::BeginMenu("Play")) {
        if (ui.play_state == UiState::PlayState::Edit) {
            if (ImGui::MenuItem("Play", "Cmd+P")) ui.request_play = true;
        } else {
            if (ImGui::MenuItem("Stop", "Cmd+P")) ui.request_stop = true;
        }
        if (ui.play_state == UiState::PlayState::Playing) {
            if (ImGui::MenuItem("Pause", "Cmd+Shift+P")) ui.request_pause = true;
        } else if (ui.play_state == UiState::PlayState::Paused) {
            if (ImGui::MenuItem("Resume", "Cmd+Shift+P")) ui.request_play = true;
            if (ImGui::MenuItem("Step Frame", "Cmd+.")) ui.request_step = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Controls & Manual", "F1")) {
            ui.show_manual_modal = true;
        }
        if (ImGui::MenuItem("Keyboard Shortcuts Sheet")) {
            ui.show_manual_modal = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...", "Cmd+,")) {
            ui.show_preferences_window = true;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Lucida Engine v0.1.0 (BlackLine)");
        ImGui::EndMenu();
    }

    DrawPlayToolbar(ui);

    ImGui::EndMainMenuBar();
}

void EditorUI::DrawPlayToolbar(UiState& ui) {
    const float bar_width = ImGui::GetWindowWidth();
    const float toolbar_width = 250.0f;
    const float target_x = (bar_width - toolbar_width) * 0.5f;
    if (target_x > ImGui::GetCursorPosX()) {
        ImGui::SameLine(target_x);
    } else {
        ImGui::SameLine();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

    if (ui.play_state == UiState::PlayState::Edit) {
        if (VectorIconButton("play_btn", VectorIcon::Play, "Play (Cmd+P)", ImVec2(125, 22), IM_COL32(38, 145, 75, 230))) {
            ui.request_play = true;
        }
        DrawTooltip("Start Play Mode (Cmd+P / Ctrl+P)\nSnapshots world state and activates real-time Jolt physics.");
    } else {
        float stop_pulse = 0.0f;
        if (ui.enable_ui_animations) {
            stop_pulse = iam_oscillate(ImGui::GetID("stop_btn_pulse"), 0.15f, 2.0f, iam_wave_sine, 0.0f, ImGui::GetIO().DeltaTime);
        }
        const int r_stop = std::clamp(static_cast<int>(185 + stop_pulse * 40.0f), 150, 240);
        if (VectorIconButton("stop_btn", VectorIcon::Stop, "Stop", ImVec2(75, 22), IM_COL32(r_stop, 45, 45, 230))) {
            ui.request_stop = true;
        }
        DrawTooltip("Stop Simulation (Cmd+P / Ctrl+P)\nRestores the entire scene to its exact pre-play state.");

        ImGui::SameLine();
        if (ui.play_state == UiState::PlayState::Playing) {
            if (VectorIconButton("pause_btn", VectorIcon::Pause, "Pause", ImVec2(75, 22), IM_COL32(195, 145, 25, 230))) {
                ui.request_pause = true;
            }
            DrawTooltip("Pause Simulation (Cmd+Shift+P / Ctrl+Shift+P)\nFreezes physics and gameplay ticks.");
        } else {
            if (VectorIconButton("resume_btn", VectorIcon::Play, "Resume", ImVec2(80, 22), IM_COL32(38, 145, 75, 230))) {
                ui.request_play = true;
            }
            DrawTooltip("Resume Simulation (Cmd+Shift+P / Ctrl+Shift+P)\nUnpauses physics simulation.");

            ImGui::SameLine();
            if (VectorIconButton("step_btn", VectorIcon::Step, "Step", ImVec2(70, 22), IM_COL32(45, 105, 195, 230))) {
                ui.request_step = true;
            }
            DrawTooltip("Step 1 Frame (Cmd+. / Ctrl+.)\nAdvances physics by exactly 1 tick (1/60s).");
        }
    }

    ImGui::PopStyleVar(2);

    if (ui.play_state != UiState::PlayState::Edit) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::SliderFloat("##timescale", &ui.gameplay_time_scale, 0.1f, 4.0f, "%.1fx");
        DrawTooltip("Time Scale\n0.1x = slow motion  |  1.0x = real-time  |  4.0x = fast forward");
        ImGui::PopStyleVar();

        const char* badge_text = (ui.play_state == UiState::PlayState::Playing) ? "  PLAYING  " : "  PAUSED  ";
        ImU32 badge_col = (ui.play_state == UiState::PlayState::Playing)
            ? IM_COL32(38, 155, 70, 220) : IM_COL32(195, 145, 25, 220);
        ImVec2 badge_size = ImGui::CalcTextSize(badge_text);
        float right_x = ImGui::GetWindowWidth() - badge_size.x - 16.0f;
        if (right_x > ImGui::GetCursorPosX() + 4.0f) ImGui::SameLine(right_x);
        else ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        badge_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, badge_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  badge_col);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::SmallButton(badge_text);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

void EditorUI::ApplyTheme() {
    lucida::ApplyTheme(ThemeColors{});
}

} // namespace lucida
