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
#include "lucida/framework/Commands.h"
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
    bool show_graphics_settings = true;
    bool show_viewport  = true;

    bool request_quit       = false;
    bool request_fullscreen = false;

    // Gizmo state
    int  gizmo_operation = 0; // 0: Translate, 1: Rotate, 2: Scale
    int  gizmo_space = 0;     // 0: Local, 1: World
    bool snap_enabled = false;
    Vec3 snap_position{0.5f};
    f32  snap_rotation = 15.0f;
    f32  snap_scale = 0.25f;

    // What the inspector is looking at. One selection: multi-select without an
    // undo stack behind it is a fast way to lose work.
    Entity selection = kNullEntity;

    // Size of the viewport panel in framebuffer pixels, filled in each frame.
    // The renderer traces to this rather than to the window, so a third-screen
    // panel costs a third of the rays.
    i32 viewport_width  = 0;
    i32 viewport_height = 0;

    scenes::BuiltIn scene = scenes::BuiltIn::Empty;
    bool request_scene_reload = false;
    std::string pending_model_path;   // non-empty when the user picked a file
};

class DebugUI {
public:
    void Init();
    void Shutdown();

    CommandStack& Commands() { return m_commands; }

    // Between platform->OverlayNewFrame() and backend->Render().
    // viewport_texture is the backend's presented image, or null when the
    // renderer is drawing straight to the window.
    void Build(World& world, SceneAssets& assets, UiState& ui, RenderSettings& settings,
               const RenderStats& stats, CameraController& camera, const FrameTime& time,
               void* viewport_texture = nullptr, f32 viewport_aspect = 16.0f / 9.0f);

private:
    void BuildDefaultLayout(unsigned dockspace_id);
    void DrawMenuBar(UiState& ui);
    void DrawViewport(World& world, UiState& ui, void* texture, f32 aspect,
                      const CameraController& camera);
    void DrawHierarchy(World& world, UiState& ui);
    void DrawSceneGraph(World& world, UiState& ui, Entity root_or_null);
    void DrawInspector(World& world, UiState& ui, SceneAssets& assets);
    void DrawGraphicsSettings(UiState& ui, SceneAssets& assets, RenderSettings& settings, CameraController& camera);
    void DrawGizmo(World& world, UiState& ui, CameraController& camera, f32 aspect);
    void DrawStats(const RenderStats& stats, const FrameTime& time);
    void TrackEdit(Registry& registry, Entity entity, const LocalTransform& current,
                   const char* name);

    CommandStack m_commands;
    // Transform captured when a control was grabbed, so releasing it can push
    // one undo entry for the whole drag instead of one per frame.
    LocalTransform m_drag_start;
    bool m_dragging = false;

    f32  m_fps_ema = 60.0f;
    bool m_reset_layout = false;
};

} // namespace lucida
