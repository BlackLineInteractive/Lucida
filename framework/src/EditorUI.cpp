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
    io.IniFilename = "imgui.ini";

    // Comprehensive Unicode Glyph Ranges (Latin, Cyrillic / Ukrainian, Greek, Arrows, Math, Geometric, Dingbats)
    static const ImWchar s_full_glyph_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0100, 0x024F, // Latin Extended-A + B
        0x0400, 0x052F, // Cyrillic + Ukrainian (І, Ї, Є, Ґ, etc.)
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0x2000, 0x206F, // General Punctuation
        0x20A0, 0x20CF, // Currency Symbols
        0x2100, 0x214F, // Letterlike Symbols
        0x2190, 0x21FF, // Arrows
        0x2200, 0x22FF, // Mathematical Operators
        0x2500, 0x257F, // Box Drawing
        0x25A0, 0x25FF, // Geometric Shapes (■, ▲, ▼, ▶, ▷, ▽, ◈, ◉, ⬡, etc.)
        0x2600, 0x26FF, // Miscellaneous Symbols (⚡, ⚯, ☀, ☼, ♪, etc.)
        0x2700, 0x27BF, // Dingbats (✓, ✔, ✕, ✖, ❐, ❖, etc.)
        0x2900, 0x297F, // Supplemental Arrows-B (⤹, ⤸, ⤢, etc.)
        0x2B00, 0x2BFF, // Miscellaneous Symbols and Arrows (⬡, ⬢, etc.)
        0
    };

    const char* font_candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/LucidaGrande.ttc",
        "/System/Library/Fonts/Geneva.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };

    ImFont* loaded_font = nullptr;
    for (const char* path : font_candidates) {
        FILE* f = std::fopen(path, "rb");
        if (f) {
            std::fclose(f);
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 2;
            cfg.PixelSnapH = true;
            loaded_font = io.Fonts->AddFontFromFileTTF(path, 15.0f, &cfg, s_full_glyph_ranges);
            if (loaded_font) break;
        }
    }

    if (!loaded_font) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 2;
        io.Fonts->AddFontDefault(&cfg);
    }

    ApplyTheme();
    RegisterConsoleLogSink();
}

void EditorUI::Shutdown() {
    ImGui::SaveIniSettingsToDisk("imgui.ini");
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

        // Select All (A) and Deselect All (Alt+A) in Blender style
        if (!modifier && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            if (io.KeyAlt) {
                ui.DeselectAll();
            } else {
                if (ui.selections.empty()) {
                    ui.SelectAll(world.Entities());
                } else {
                    ui.DeselectAll();
                }
            }
        }

        // Group (Ctrl+G) and Ungroup (Ctrl+Alt+G)
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
            if (io.KeyAlt) {
                if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
                    m_commands.Execute(std::make_unique<UngroupEntitiesCommand>(world.Entities(), ui.selection));
                }
            } else {
                if (ui.selections.size() > 1 || (ui.selection != kNullEntity && world.Entities().Valid(ui.selection))) {
                    m_commands.Execute(std::make_unique<GroupEntitiesCommand>(world.Entities(), ui.selections.empty() ? std::vector<Entity>{ui.selection} : ui.selections));
                }
            }
        }

        // Join Meshes (Ctrl+J in Blender style)
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_J, false)) {
            if (ui.selections.size() >= 2) {
                m_commands.Execute(std::make_unique<JoinMeshesCommand>(world.Entities(), ui.selections));
            }
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
            for (Entity to_del : ui.selections) {
                if (world.Entities().Valid(to_del)) {
                    m_commands.Execute(std::make_unique<DestroyEntityCommand>(world.Entities(), to_del, "Delete Entity"));
                }
            }
            if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
                Entity to_del = ui.selection;
                m_commands.Execute(std::make_unique<DestroyEntityCommand>(world.Entities(), to_del, "Delete Entity"));
            }
            ui.DeselectAll();
        }
        if (modifier && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (ui.selection != kNullEntity && world.Entities().Valid(ui.selection)) {
                EntitySnapshot snap = EntitySnapshot::Capture(world.Entities(), ui.selection);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(world.Entities());
                ui.SetSelected(dup, true, false);
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

void EditorUI::BuildDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main = dockspace_id;
    ImGuiID dock_left  = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left,  0.20f, nullptr, &dock_main);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.22f, nullptr, &dock_main);
    ImGuiID dock_down  = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down,  0.26f, nullptr, &dock_main);

    ImGuiID dock_left_bottom  = ImGui::DockBuilderSplitNode(dock_left,  ImGuiDir_Down, 0.38f, nullptr, &dock_left);
    ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.42f, nullptr, &dock_right);

    // 1. Center Viewport
    ImGui::DockBuilderDockWindow("Viewport", dock_main);

    // 2. Left Column: Hierarchy (top) & Content Browser (bottom)
    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Content Browser", dock_left_bottom);

    // 3. Right Column: Inspector & Mesh Modeling (top)
    ImGui::DockBuilderDockWindow("Mesh Modeling (Blender Mode)", dock_right);
    ImGui::DockBuilderDockWindow("Inspector", dock_right);

    // 4. Right Bottom Column: Graphics Settings & Texture Maps (bottom)
    ImGui::DockBuilderDockWindow("Graphics Settings", dock_right_bottom);
    ImGui::DockBuilderDockWindow("Texture Maps", dock_right_bottom);

    // 5. Center Bottom: Console, Engine Diagnostics, Statistics, Gameplay Debugger
    ImGui::DockBuilderDockWindow("Gameplay Debugger", dock_down);
    ImGui::DockBuilderDockWindow("Statistics", dock_down);
    ImGui::DockBuilderDockWindow("Engine Diagnostics", dock_down);
    ImGui::DockBuilderDockWindow("Console", dock_down);

    ImGui::DockBuilderFinish(dockspace_id);
    ImGui::SaveIniSettingsToDisk("imgui.ini");
}

void EditorUI::DrawMenuBar(World& world, SceneAssets& assets, UiState& ui) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu(TR("menu_file", "File"))) {
        if (ImGui::MenuItem("New Blank Scene", "Cmd+N")) {
            ui.scene = scenes::BuiltIn::Empty;
            ui.request_scene_reload = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Load 3D Model...")) {
            IGFD::FileDialogConfig config;
            config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("LoadModel", "Choose a model",
                                                    ".glb,.gltf,.obj,.fbx", config);
        }
        if (ImGui::MenuItem("Save Scene", "Cmd+S")) {
            // Save current scene state
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...", "Cmd+,")) {
            ui.show_preferences_window = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit Lucida", "Cmd+Q")) ui.request_quit = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu_edit", "Edit"))) {
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
        if (ImGui::MenuItem(TR("action_select_all", "Select All (A)"), "A")) {
            ui.SelectAll(world.Entities());
        }
        if (ImGui::MenuItem(TR("action_deselect_all", "Deselect All (Alt+A)"), "Alt+A")) {
            ui.DeselectAll();
        }
        ImGui::Separator();
        Registry& entities = world.Entities();
        const bool has_selection = (ui.selection != kNullEntity && entities.Valid(ui.selection)) || !ui.selections.empty();
        if (ImGui::MenuItem(TR("action_group", "Group (Ctrl+G)"), "Ctrl+G", false, has_selection)) {
            m_commands.Execute(std::make_unique<GroupEntitiesCommand>(entities, ui.selections.empty() ? std::vector<Entity>{ui.selection} : ui.selections));
        }
        const bool is_group = ui.selection != kNullEntity && entities.Get<GroupComponent>(ui.selection) != nullptr;
        if (ImGui::MenuItem(TR("action_ungroup", "Ungroup (Ctrl+Alt+G)"), "Ctrl+Alt+G", false, is_group)) {
            m_commands.Execute(std::make_unique<UngroupEntitiesCommand>(entities, ui.selection));
        }
        if (ImGui::MenuItem(TR("action_join", "Join Meshes (Ctrl+J)"), "Ctrl+J", false, ui.selections.size() >= 2)) {
            m_commands.Execute(std::make_unique<JoinMeshesCommand>(entities, ui.selections));
        }
        ImGui::Separator();
        if (ImGui::MenuItem(TR("action_duplicate", "Duplicate Selection"), "Cmd+D", false, has_selection)) {
            if (ui.selection != kNullEntity && entities.Valid(ui.selection)) {
                EntitySnapshot snap = EntitySnapshot::Capture(entities, ui.selection);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(entities);
                ui.SetSelected(dup, true, false);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, dup, snap, "Duplicate Entity"));
            }
        }
        if (ImGui::MenuItem(TR("action_delete", "Delete Selection"), "Del", false, has_selection)) {
            for (Entity to_del : ui.selections) {
                if (entities.Valid(to_del)) {
                    m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Entity"));
                }
            }
            if (ui.selection != kNullEntity && entities.Valid(ui.selection)) {
                Entity to_del = ui.selection;
                m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Entity"));
            }
            ui.DeselectAll();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Undo History", nullptr, false, m_commands.CanUndo() || m_commands.CanRedo())) {
            m_commands.Clear();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu_demos", "Demos"))) {
        if (ImGui::MenuItem("Radiance Cascades 3D (GI)", nullptr, ui.scene == scenes::BuiltIn::RadianceCascades3D)) {
            ui.scene = scenes::BuiltIn::RadianceCascades3D;
            ui.requested_backend = UiState::RenderBackendType::RadianceCascades3D;
            ui.request_scene_reload = true;
        }
        if (ImGui::MenuItem("Whitted RT Studio", nullptr, ui.scene == scenes::BuiltIn::BasicPrimitives)) {
            ui.scene = scenes::BuiltIn::BasicPrimitives;
            ui.requested_backend = UiState::RenderBackendType::MetalRayTracing;
            ui.request_scene_reload = true;
        }
        if (ImGui::MenuItem("Water & Volumetric Fog", nullptr, ui.scene == scenes::BuiltIn::WaterAndFog)) {
            ui.scene = scenes::BuiltIn::WaterAndFog;
            ui.request_scene_reload = true;
        }
        if (ImGui::MenuItem("Material Lab (PBR)", nullptr, ui.scene == scenes::BuiltIn::MaterialLab)) {
            ui.scene = scenes::BuiltIn::MaterialLab;
            ui.request_scene_reload = true;
        }
        if (ImGui::MenuItem("Physics Sandbox", nullptr, ui.scene == scenes::BuiltIn::PhysicsPlayground)) {
            ui.scene = scenes::BuiltIn::PhysicsPlayground;
            ui.request_scene_reload = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reload Current Scene", "R")) {
            ui.request_scene_reload = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu_renderer", "Renderer"))) {
        if (ImGui::MenuItem("Apple Metal Compute Ray Tracer (Whitted RT)", nullptr, ui.current_backend == UiState::RenderBackendType::MetalRayTracing)) {
            ui.requested_backend = UiState::RenderBackendType::MetalRayTracing;
        }
        if (ImGui::MenuItem("Radiance Cascades 3D Global Illumination (RC-3D)", nullptr, ui.current_backend == UiState::RenderBackendType::RadianceCascades3D)) {
            ui.requested_backend = UiState::RenderBackendType::RadianceCascades3D;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu_view", "View"))) {
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

    if (ImGui::BeginMenu(TR("menu_settings", "Settings"))) {
        if (ImGui::MenuItem("Engine Preferences...", "Cmd+,")) {
            ui.show_preferences_window = true;
        }
        if (ImGui::MenuItem("Graphics & Render Settings...")) {
            ui.show_graphics_settings = true;
        }
        if (ImGui::MenuItem("Engine Diagnostics & Profiler...")) {
            ui.show_engine_diagnostics = true;
        }
        ImGui::Separator();

        if (ImGui::BeginMenu(TR("menu_language", "Language"))) {
            if (ImGui::MenuItem("English", nullptr, Localization::GetLanguage() == Language::English)) {
                Localization::SetLanguage(Language::English);
            }
            if (ImGui::MenuItem("Українська", nullptr, Localization::GetLanguage() == Language::Ukrainian)) {
                Localization::SetLanguage(Language::Ukrainian);
            }
            if (ImGui::MenuItem("Русский", nullptr, Localization::GetLanguage() == Language::Russian)) {
                Localization::SetLanguage(Language::Russian);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Deutsch (German)", nullptr, Localization::GetLanguage() == Language::German)) {
                Localization::SetLanguage(Language::German);
            }
            if (ImGui::MenuItem("Français (French)", nullptr, Localization::GetLanguage() == Language::French)) {
                Localization::SetLanguage(Language::French);
            }
            if (ImGui::MenuItem("Español (Spanish)", nullptr, Localization::GetLanguage() == Language::Spanish)) {
                Localization::SetLanguage(Language::Spanish);
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::MenuItem("Show Spatial Grid", nullptr, &ui.viewport_show_grid);
        ImGui::MenuItem("Show 3D Light & Camera Icons", nullptr, &ui.viewport_show_lights);
        ImGui::MenuItem("Show Selection Corner Bounds", nullptr, &ui.viewport_show_bounds);
        ImGui::MenuItem("Show Statistics Overlay", nullptr, &ui.show_stats_overlay);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout to Default")) m_reset_layout = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu_help", "Help"))) {
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
        ImGui::TextDisabled("Lucida Engine v0.5.0 (BlackLine)");
        ImGui::EndMenu();
    }

    DrawPlayToolbar(ui);

    ImGui::EndMainMenuBar();
}

void EditorUI::DrawPlayToolbar(UiState& ui) {
    const float bar_width = ImGui::GetWindowWidth();

    // Center: Simulation Play/Pause/Stop/Step Controls
    const float sim_width = 260.0f;
    const float target_x = (bar_width - sim_width) * 0.5f;
    if (target_x > ImGui::GetCursorPosX()) {
        ImGui::SameLine(target_x);
    } else {
        ImGui::SameLine();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

    if (ui.play_state == UiState::PlayState::Edit) {
        if (VectorIconButton("play_btn", VectorIcon::Play, "Play (Cmd+P)", ImVec2(120, 22), IM_COL32(38, 145, 75, 230))) {
            ui.request_play = true;
        }
        DrawTooltip("Start Simulation (Cmd+P / Ctrl+P)\nSnapshots world state and activates real-time Jolt physics.");
    } else {
        float stop_pulse = 0.0f;
        if (ui.enable_ui_animations) {
            stop_pulse = iam_oscillate(ImGui::GetID("stop_btn_pulse"), 0.15f, 2.0f, iam_wave_sine, 0.0f, ImGui::GetIO().DeltaTime);
        }
        const int r_stop = std::clamp(static_cast<int>(185 + stop_pulse * 40.0f), 150, 240);
        if (VectorIconButton("stop_btn", VectorIcon::Stop, "Stop", ImVec2(70, 22), IM_COL32(r_stop, 45, 45, 230))) {
            ui.request_stop = true;
        }
        DrawTooltip("Stop Simulation (Cmd+P / Ctrl+P)\nRestores the entire scene to its exact pre-play state.");

        ImGui::SameLine();
        if (ui.play_state == UiState::PlayState::Playing) {
            if (VectorIconButton("pause_btn", VectorIcon::Pause, "Pause", ImVec2(70, 22), IM_COL32(195, 145, 25, 230))) {
                ui.request_pause = true;
            }
            DrawTooltip("Pause Simulation (Cmd+Shift+P / Ctrl+Shift+P)\nFreezes physics and gameplay ticks.");
        } else {
            if (VectorIconButton("resume_btn", VectorIcon::Play, "Resume", ImVec2(75, 22), IM_COL32(38, 145, 75, 230))) {
                ui.request_play = true;
            }
            DrawTooltip("Resume Simulation (Cmd+Shift+P / Ctrl+Shift+P)\nUnpauses physics simulation.");

            ImGui::SameLine();
            if (VectorIconButton("step_btn", VectorIcon::Step, "Step", ImVec2(65, 22), IM_COL32(45, 105, 195, 230))) {
                ui.request_step = true;
            }
            DrawTooltip("Step 1 Frame (Cmd+. / Ctrl+.)\nAdvances physics by exactly 1 tick (1/60s).");
        }
    }

    if (ui.play_state != UiState::PlayState::Edit) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(75.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        ImGui::SliderFloat("##timescale", &ui.gameplay_time_scale, 0.1f, 4.0f, "%.1fx");
        DrawTooltip("Simulation Time Scale\n0.1x = slow motion  |  1.0x = real-time  |  4.0x = fast forward");
        ImGui::PopStyleVar();
    }

    ImGui::PopStyleVar(2);

    // 3. Right: Active Backend Tag & FPS Readout
    const char* backend_tag = (ui.current_backend == UiState::RenderBackendType::RadianceCascades3D)
        ? " RC-3D GI " : " Metal RT ";
    const ImU32 backend_col = (ui.current_backend == UiState::RenderBackendType::RadianceCascades3D)
        ? IM_COL32(30, 140, 190, 220) : IM_COL32(50, 120, 60, 220);

    const float fps = ImGui::GetIO().Framerate;
    const float ms  = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
    char fps_buf[48];
    std::snprintf(fps_buf, sizeof(fps_buf), "%.0f FPS (%.1f ms)", fps, ms);

    const float right_group_width = 240.0f;
    const float right_x = bar_width - right_group_width;
    if (right_x > ImGui::GetCursorPosX() + 4.0f) {
        ImGui::SameLine(right_x);
    } else {
        ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button,        backend_col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, backend_col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  backend_col);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    if (ImGui::SmallButton(backend_tag)) {
        if (ui.current_backend == UiState::RenderBackendType::MetalRayTracing) {
            ui.requested_backend = UiState::RenderBackendType::RadianceCascades3D;
        } else {
            ui.requested_backend = UiState::RenderBackendType::MetalRayTracing;
        }
    }
    DrawTooltip("Click to switch Render Backend (Metal Ray Tracer <-> Radiance Cascades 3D)");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::TextDisabled("%s", fps_buf);
}

void EditorUI::ApplyTheme() {
    lucida::ApplyTheme(ThemeColors{});
}

} // namespace lucida
