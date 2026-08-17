// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/core/math/Math.h"
#include "lucida/core/platform/Platform.h"
#include "lucida/framework/CameraController.h"
#include "lucida/framework/Commands.h"
#include "lucida/framework/SceneAssets.h"
#include "lucida/framework/SceneLibrary.h"
#include "lucida/render/RenderBackend.h"
#include "lucida/resource/MeshBuilder.h"
#include "lucida/runtime/World.h"

struct ImVec2;

#include <string>
#include <vector>
#include <unordered_set>

namespace lucida {

struct UiState {
    bool show_menu      = true;
    bool show_hierarchy = true;
    bool show_inspector = true;
    bool show_stats_panel   = false;
    bool show_stats_overlay = true;
    bool show_graphics_settings = true;
    bool show_viewport  = true;
    bool show_console   = true;
    bool show_content_browser = true;
    bool show_mesh_editor     = true;
    bool show_texture_browser = true;
    bool show_gameplay_debugger   = true;
    bool show_engine_diagnostics  = true;

    // Gameplay Debugger & Diagnostics State
    f32  gameplay_time_scale     = 1.0f;
    bool draw_physics_colliders  = true;
    bool draw_raycast_sensors    = true;
    bool draw_ai_perception      = true;
    bool draw_audio_radii        = true;

    char hierarchy_search[128] = {0};
    char content_search[128]   = {0};
    std::string content_browser_path = "assets";

    // Blender-Style Mesh Sub-Element Editing state
    int   mesh_edit_mode        = 0; // 0: Object, 1: Vertex, 2: Edge, 3: Face
    int   selected_vertex_index = 0;
    int   selected_edge_index   = 0;
    int   selected_face_index   = 0;
    f32   extrude_distance      = 0.5f;
    f32   inset_amount          = 0.2f;
    f32   weld_threshold        = 0.001f;
    int   uv_projection_mode    = 3; // 0: PlanarX, 1: PlanarY, 2: PlanarZ, 3: Box, 4: Spherical, 5: Cylindrical
    Vec2  mesh_uv_scale{1.0f, 1.0f};
    Vec2  mesh_uv_offset{0.0f, 0.0f};

    // PBR Texture Maps sub-panel states (Albedo, Normal, Metallic, Roughness, AO, Emissive, Height)
    struct TextureMapSlot {
        std::string path;
        f32         factor = 1.0f;
        Vec4        tint{1.0f, 1.0f, 1.0f, 1.0f};
        Vec2        uv_scale{1.0f, 1.0f};
        Vec2        uv_offset{0.0f, 0.0f};
        int         channel = 0; // 0: All/RGB, 1: R, 2: G, 3: B, 4: A
        bool        srgb = true;
        bool        invert = false;
        bool        flip_green_normal = false;
        f32         normal_strength = 1.0f;
        f32         emissive_intensity = 1.0f;
        f32         height_scale = 0.05f;
        int         pom_steps = 16;
    };
    TextureMapSlot map_albedo;
    TextureMapSlot map_normal;
    TextureMapSlot map_metallic;
    TextureMapSlot map_roughness;
    TextureMapSlot map_ao;
    TextureMapSlot map_emissive;
    TextureMapSlot map_height;
    int            active_texture_map_tab = 0; // 0: All, 1: Albedo, 2: Normal, 3: Metallic, 4: Roughness, 5: AO, 6: Emissive, 7: Height

    bool request_quit       = false;
    bool request_fullscreen = false;

    // Gizmo state
    int  gizmo_operation = 0; // 0: Translate, 1: Rotate, 2: Scale
    int  gizmo_space = 0;     // 0: Local, 1: World
    bool snap_enabled = false;
    Vec3 snap_position{0.5f};
    f32  snap_rotation = 15.0f;
    f32  snap_scale = 0.25f;

    // Multi-Selection state
    Entity              selection = kNullEntity;
    std::vector<Entity> selections;

    bool IsSelected(Entity e) const {
        if (e == kNullEntity) return false;
        if (e == selection) return true;
        return std::find(selections.begin(), selections.end(), e) != selections.end();
    }
    void SetSelected(Entity e, bool selected, bool multi_select = false) {
        if (!multi_select) {
            selections.clear();
            if (selected && e != kNullEntity) {
                selection = e;
                selections.push_back(e);
            } else {
                selection = kNullEntity;
            }
        } else {
            if (selected && e != kNullEntity) {
                if (std::find(selections.begin(), selections.end(), e) == selections.end()) {
                    selections.push_back(e);
                }
                selection = e;
            } else {
                selections.erase(std::remove(selections.begin(), selections.end(), e), selections.end());
                if (selection == e) {
                    selection = selections.empty() ? kNullEntity : selections.back();
                }
            }
        }
    }
    void SelectAll(Registry& registry) {
        selections.clear();
        for (auto [e, name] : registry.View<Name>().each()) {
            selections.push_back(e);
        }
        selection = selections.empty() ? kNullEntity : selections.front();
    }
    void DeselectAll() {
        selections.clear();
        selection = kNullEntity;
    }

    // Collapsed hierarchy nodes
    std::unordered_set<Entity> collapsed_nodes;
    bool IsCollapsed(Entity e) const { return collapsed_nodes.find(e) != collapsed_nodes.end(); }
    void SetCollapsed(Entity e, bool collapsed) {
        if (collapsed) collapsed_nodes.insert(e);
        else collapsed_nodes.erase(e);
    }
    void ToggleCollapsed(Entity e) {
        if (IsCollapsed(e)) collapsed_nodes.erase(e);
        else collapsed_nodes.insert(e);
    }

    // Selection Tools (Point, Box, Lasso)
    enum class SelectTool : u8 { Point = 0, Box, Lasso };
    SelectTool select_tool = SelectTool::Point;
    bool       is_box_selecting = false;
    bool       is_lasso_selecting = false;
    Vec2       selection_drag_start{0.0f, 0.0f};
    std::vector<Vec2> lasso_points;

    // Size of the viewport panel in framebuffer pixels
    i32 viewport_width  = 0;
    i32 viewport_height = 0;
    bool viewport_hovered = false;   // true when mouse is over the viewport panel
    bool viewport_rmb     = false;   // true while RMB is held inside the viewport
    bool viewport_toolbar_collapsed = false; // collapse top viewport toolbar

    scenes::BuiltIn scene = scenes::BuiltIn::Empty;
    bool request_scene_reload = false;
    std::string pending_model_path;   // non-empty when the user picked a file

    // Camera view source: Editor Viewport Camera vs In-Scene Game Camera
    enum class CameraSource : u8 { Viewport = 0, GameCamera };
    CameraSource camera_source = CameraSource::Viewport;

    // Render Backend Pipeline selection (switchable at runtime)
    enum class RenderBackendType : u8 {
        MetalRayTracing = 0,
        RadianceCascades3D
    };
    RenderBackendType current_backend   = RenderBackendType::MetalRayTracing;
    RenderBackendType requested_backend = RenderBackendType::MetalRayTracing;

    // Viewport 3D line gizmo & frustum visualizers (cameras, lights, bounds)
    bool show_visualizers = true;
    bool show_light_visualizers = true;
    bool show_camera_frustums = true;
    bool show_selection_bounds = true;
    bool show_collider_wireframes = true;

    // Play Mode (M22) state
    enum class PlayState : u8 {
        Edit = 0,
        Playing,
        Paused
    };
    PlayState play_state = PlayState::Edit;
    bool request_play  = false;
    bool request_pause = false;
    bool request_step  = false;
    bool request_stop  = false;

    // Modals & Panels
    bool show_manual_modal        = false;
    bool show_preferences_window  = false;

    // Preferences & Tooltip config
    bool enable_ui_animations     = true;
    f32  animation_speed          = 1.0f;
    f32  camera_fly_speed         = 4.0f;
    f32  camera_sprint_multiplier = 2.5f;
    bool show_tooltips            = true;
};

class EditorUI {
public:
    void Init();
    void Shutdown();
    static void ApplyTheme();

    CommandStack& Commands() { return m_commands; }

    void Build(World& world, SceneAssets& assets, UiState& ui, RenderSettings& settings,
               const RenderStats& stats, CameraController& camera, const FrameTime& time,
               void* viewport_texture = nullptr, f32 viewport_aspect = 16.0f / 9.0f,
               IRenderBackend* renderer = nullptr);

    // Modular Editor Panels
    void DrawMenuBar(World& world, SceneAssets& assets, UiState& ui);
    void DrawPlayToolbar(UiState& ui);
    void DrawViewport(World& world, UiState& ui, void* texture, f32 aspect,
                      const CameraController& camera, const SceneAssets& assets,
                      const RenderStats& stats, const RenderSettings& settings, const FrameTime& time);
    void DrawHierarchy(World& world, UiState& ui, SceneAssets& assets);
    void DrawSceneGraph(World& world, UiState& ui, Entity root_or_null);
    void DrawSceneGraphInternal(World& world, UiState& ui, Entity current_parent, int depth, std::vector<bool>& is_last_stack);
    void DrawInspector(World& world, UiState& ui, SceneAssets& assets, CameraController& camera,
                       IRenderBackend* renderer = nullptr);
    void DrawMeshModeling(World& world, UiState& ui, SceneAssets& assets, IRenderBackend* renderer = nullptr);
    void DrawTextureBrowser(UiState& ui, SceneAssets& assets);
    void DrawContentBrowser(World& world, UiState& ui, SceneAssets& assets);
    void DrawConsole(UiState& ui);
    void DrawStatsPanel(World& world, const SceneAssets& assets, const RenderStats& stats,
                        const FrameTime& time, const RenderSettings& settings, const UiState& ui);
    void DrawStatsOverlay(World& world, const SceneAssets& assets, const RenderStats& stats,
                         const FrameTime& time, const RenderSettings& settings, const UiState& ui,
                         const ImVec2& image_min, const ImVec2& image_size);
    void DrawEngineDiagnostics(World& world, UiState& ui, const RenderStats& stats);
    void DrawGameplayDebugger(World& world, UiState& ui);
    void DrawGraphicsSettings(UiState& ui, SceneAssets& assets, RenderSettings& settings, CameraController& camera);
    void DrawPreferencesWindow(UiState& ui, CameraController& camera, RenderSettings& settings);
    void DrawManualModal(UiState& ui);

    // Viewport Helpers
    void DrawGizmo(World& world, UiState& ui, CameraController& camera, f32 aspect,
                   const ImVec2& image_min, const ImVec2& image_size);
    void DrawViewportVisualizers(World& world, const UiState& ui, const CameraController& camera, f32 aspect,
                                 const ImVec2& image_min, const ImVec2& image_size);

    // Edit Tracking for Undo/Redo
    void TrackEdit(Registry& registry, Entity entity, const LocalTransform& current, const char* name);
    void TrackShapeEdit(Registry& registry, Entity entity, const PrimitiveShape& current, const char* name);
    void TrackMaterialEdit(SceneAssets& assets, i32 mat_index, const GPUMaterial& current, const char* name);

    CommandStack m_commands;
    LocalTransform m_drag_start;
    bool m_dragging = false;

    PrimitiveShape m_shape_drag_start{};
    bool m_shape_dragging = false;

    GPUMaterial m_mat_drag_start{};
    bool m_mat_dragging = false;

    f32  m_fps_ema = 60.0f;
    bool m_reset_layout = false;

private:
    void BuildDefaultLayout(unsigned dockspace_id);
};

using DebugUI = EditorUI;

} // namespace lucida
