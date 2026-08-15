// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// The editor shell: menu bar, dock space and panels.
//
// Immediate mode, so there is no widget state to keep in sync — every panel
// reads the live structs and writes them back. The framework layer is where
// tools live; nothing below it links ImGui.

#include "lucida/core/ecs/Registry.h"
#include "lucida/framework/CameraController.h"
#include "lucida/framework/SceneLibrary.h"
#include "lucida/render/RenderBackend.h"

#include <string>

namespace lucida {

class World;

struct UiState {
    bool show_menu      = true;
    bool show_hierarchy = true;
    bool show_inspector = true;
    bool show_stats     = true;
    bool show_renderer  = true;
    bool show_viewport  = true;

    bool request_quit       = false;
    bool request_fullscreen = false;

    // What the inspector is looking at. One selection: multi-select without an
    // undo stack behind it is a fast way to lose work.
    Entity selection = kNullEntity;

    scenes::BuiltIn scene = scenes::BuiltIn::WaterAndFog;
    bool request_scene_reload = false;
    std::string pending_model_path;   // non-empty when the user picked a file
};

class DebugUI {
public:
    void Init();
    void Shutdown();

    // Between platform->OverlayNewFrame() and backend->Render().
    // viewport_texture is the backend's presented image, or null when the
    // renderer is drawing straight to the window.
    void Build(World& world, UiState& ui, RenderSettings& settings,
               const RenderStats& stats, CameraController& camera, const FrameTime& time,
               void* viewport_texture = nullptr, f32 viewport_aspect = 16.0f / 9.0f);

private:
    void BuildDefaultLayout(unsigned dockspace_id);
    void DrawMenuBar(UiState& ui);
    void DrawViewport(UiState& ui, void* texture, f32 aspect);
    void DrawHierarchy(World& world, UiState& ui);
    void DrawInspector(World& world, UiState& ui);
    void DrawRenderer(UiState& ui, RenderSettings& settings, CameraController& camera);
    void DrawStats(const RenderStats& stats, const FrameTime& time);

    f32  m_fps_ema = 60.0f;
    bool m_reset_layout = false;
};

} // namespace lucida
