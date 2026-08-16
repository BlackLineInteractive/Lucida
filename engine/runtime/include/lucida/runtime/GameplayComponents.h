// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Comprehensive Game Engine Component Definitions for all 107 gameplay archetypes across 17 subsystems.

#include "lucida/core/container/Handle.h"
#include "lucida/core/math/Math.h"

#include <functional>
#include <string>
#include <vector>

namespace lucida {

// =========================================================================
// 1. Core & Spatial Hierarchy
// =========================================================================

struct PivotComponent {
    Vec3 pivot_offset{0.0f};
    bool lock_rotation = false;
};

struct RootComponent {
    std::string scene_name = "MainScene";
    bool is_prefab_root = false;
};

// =========================================================================
// 2. Geometry, Meshes, Decals & 2D-in-3D
// =========================================================================

struct InstancedMeshComponent {
    std::string mesh_path;
    std::vector<Mat4> instance_transforms;
    bool casts_shadow = true;
};

struct ProceduralMeshComponent {
    std::string generator_type = "PerlinTerrain";
    u32 resolution = 32;
    bool auto_rebuild = true;
};

struct DynamicMeshComponent {
    bool enable_slicing = true;
    bool enable_deformations = true;
    f32 elasticity = 0.5f;
};

enum class CSGOperation : u8 {
    Union = 0,
    Difference,
    Intersection
};

struct CSGComponent {
    CSGOperation operation = CSGOperation::Difference;
    std::string target_primitive = "Cube";
};

struct DecalComponent {
    std::string texture_path;
    Vec3        size{1.0f, 1.0f, 0.5f};
    f32         fade_distance = 50.0f;
    f32         opacity = 1.0f;
};

struct BillboardComponent {
    std::string texture_path;
    Vec2        size{1.0f, 1.0f};
    bool        lock_y_axis = false; // Cylindrical vs Spherical billboard
};

struct Text3DComponent {
    std::string text = "Text3D";
    f32         font_size = 24.0f;
    Vec4        color{1.0f, 1.0f, 1.0f, 1.0f};
    bool        face_camera = false;
};

// =========================================================================
// 3. Environment, Atmosphere & Volumetrics
// =========================================================================

struct SkyboxComponent {
    std::string cubemap_path = "assets/textures/skybox.hdr";
    f32 exposure = 1.0f;
    f32 rotation_deg = 0.0f;
};

struct VolumetricCloudComponent {
    f32  coverage = 0.5f;
    f32  cloud_height = 800.0f;
    f32  cloud_thickness = 400.0f;
    f32  wind_speed = 5.0f;
    Vec3 cloud_color{0.95f, 0.95f, 0.98f};
};

struct FogVolumeComponent {
    Vec3 bounds_size{20.0f, 10.0f, 20.0f};
    f32  density = 0.05f;
    Vec3 color{0.8f, 0.85f, 0.9f};
    f32  falloff = 1.0f;
};

struct PostProcessVolumeComponent {
    bool is_global = true;
    Vec3 bounds_size{10.0f};
    f32  bloom_intensity = 0.8f;
    f32  bloom_threshold = 1.0f;
    f32  exposure = 1.0f;
    f32  vignette = 0.2f;
    f32  saturation = 1.0f;
};

struct ReflectionProbeComponent {
    f32  radius = 15.0f;
    f32  intensity = 1.0f;
    bool is_box_projection = true;
    Vec3 box_extents{10.0f};
};

struct LightProbeComponent {
    Vec3 grid_spacing{2.0f};
    i32  sh_bands = 3; // Spherical Harmonics 3 bands (9 coefficients)
};

// =========================================================================
// 4. Cameras & Cinematics
// =========================================================================

struct CinematicCameraComponent {
    f32 focal_length_mm = 50.0f; // 35mm, 50mm, 85mm portrait
    f32 f_stop          = 2.8f;  // Aperture (DoF blur depth)
    f32 focus_distance  = 5.0f;  // Focus plane in meters
    f32 sensor_width_mm = 36.0f; // Full-frame 35mm sensor
};

struct SpringArmComponent {
    f32  target_arm_length = 4.5f;
    Vec3 socket_offset{0.0f, 1.2f, 0.0f};
    bool enable_collision_test = true;
    f32  probe_radius = 0.2f;
};

struct DollyTrackComponent {
    std::vector<Vec3> waypoints;
    f32  speed = 2.0f;
    bool loop = true;
    f32  current_progress = 0.0f;
};

// =========================================================================
// 5. World, Nature & Water
// =========================================================================

struct WaterBodyComponent {
    Vec2 plane_size{100.0f, 100.0f};
    f32  wave_amplitude = 0.35f;
    f32  wave_frequency = 0.2f;
    f32  wave_speed = 1.0f;
    Vec3 shallow_color{0.1f, 0.45f, 0.6f};
    Vec3 deep_color{0.02f, 0.08f, 0.25f};
    f32  clarity = 0.75f;
};

struct RiverSplineComponent {
    std::vector<Vec3> control_points;
    f32 river_width = 4.0f;
    f32 flow_speed  = 2.5f;
};

struct FoliageInstancerComponent {
    std::string mesh_path;
    i32         instance_count = 1000;
    f32         density_radius = 50.0f;
    Vec2        scale_range{0.8f, 1.3f};
    bool        align_to_surface_normal = true;
};

struct WindSourceComponent {
    Vec3 direction{1.0f, 0.0f, 0.0f};
    f32  speed = 5.0f;
    f32  gust_frequency = 0.5f;
    f32  turbulence = 0.3f;
};

struct VoxelTerrainComponent {
    i32 chunk_size = 32;
    f32 voxel_scale = 0.5f;
    bool enable_caves = true;
};

// =========================================================================
// 6. Physics, Constraints & Sensors
// =========================================================================

struct PhysicalMaterialComponent {
    f32 dynamic_friction = 0.6f;
    f32 static_friction  = 0.7f;
    f32 restitution      = 0.1f; // Bounciness
    f32 density_kg_m3    = 1000.0f;
};

struct RaycastSensorComponent {
    Vec3 direction{0.0f, -1.0f, 0.0f};
    f32  max_distance = 10.0f;
    bool has_hit = false;
    f32  hit_distance = 0.0f;
    Vec3 hit_point{0.0f};
    Vec3 hit_normal{0.0f, 1.0f, 0.0f};
};

struct ShapeCastComponent {
    f32  cast_radius = 0.5f;
    Vec3 direction{0.0f, 0.0f, 1.0f};
    f32  max_distance = 5.0f;
    bool has_hit = false;
};

enum class JointType : u8 {
    Hinge = 0,
    BallSocket,
    Slider,
    SixDOF
};

struct PhysicsJointComponent {
    JointType joint_type = JointType::Hinge;
    Vec3      anchor{0.0f};
    Vec3      axis{0.0f, 1.0f, 0.0f};
    Vec2      limits{-kPi, kPi};
    bool      enable_motor = false;
    f32       motor_target_velocity = 0.0f;
    f32       max_motor_force = 100.0f;
};

struct BuoyancyComponent {
    f32 water_level = 0.0f;
    f32 volume = 1.0f;
    f32 fluid_density = 1000.0f; // kg/m^3 (water)
    f32 linear_drag = 2.0f;
    f32 angular_drag = 1.5f;
};

struct RopeComponent {
    i32  segments = 12;
    f32  total_length = 5.0f;
    f32  stiffness = 0.9f;
};

struct ClothComponent {
    Vec2 grid_size{2.0f, 2.0f};
    i32  resolution = 16;
    f32  stiffness = 0.8f;
    f32  wind_influence = 1.0f;
};

struct SoftBodyComponent {
    f32 volume_stiffness = 0.9f;
    f32 pressure = 100.0f;
    f32 damping = 0.05f;
};

struct DestructibleMeshComponent {
    i32 voronoi_cell_count = 16;
    f32 fracture_force_threshold = 500.0f;
    bool is_fractured = false;
};

// =========================================================================
// 7. Vehicles
// =========================================================================

struct WheeledVehicleComponent {
    f32 engine_peak_torque = 500.0f; // Nm
    f32 max_rpm = 7000.0f;
    i32 gear_count = 6;
    i32 current_gear = 1;
};

struct VehicleWheelComponent {
    f32 suspension_rest_length = 0.5f;
    f32 suspension_stiffness = 35000.0f;
    f32 suspension_damping = 4000.0f;
    f32 wheel_radius = 0.35f;
    f32 steer_angle = 0.0f;
    bool is_powered = true;
};

struct TrackedVehicleComponent {
    f32 left_track_speed = 0.0f;
    f32 right_track_speed = 0.0f;
    f32 track_friction = 1.2f;
};

struct AircraftComponent {
    f32 wing_area_m2 = 18.0f;
    f32 lift_coefficient = 0.45f;
    f32 thrust_newtons = 25000.0f;
};

struct WatercraftComponent {
    f32 rudder_angle = 0.0f;
    f32 propeller_thrust = 15000.0f;
    f32 hull_drag = 1.5f;
};

// =========================================================================
// 8. Characters, Controllers & Pawns
// =========================================================================

struct CharacterBodyComponent {
    f32  capsule_radius = 0.4f;
    f32  capsule_height = 1.8f;
    f32  step_height = 0.35f;
    f32  max_slope_angle_deg = 45.0f;
    bool is_grounded = true;
};

struct CharacterMovementComponent {
    f32  walk_speed = 4.5f;
    f32  run_speed  = 8.0f;
    f32  jump_force = 6.5f;
    f32  air_control = 0.3f;
    Vec3 velocity{0.0f};
};

struct PlayerControllerComponent {
    bool is_active = true;
    f32  mouse_sensitivity = 1.0f;
    bool invert_y = false;
};

struct PlayerInputComponent {
    Vec2 move_axis{0.0f};
    Vec2 look_axis{0.0f};
    bool jump_pressed = false;
    bool sprint_pressed = false;
    bool fire_pressed = false;
};

struct AIControllerComponent {
    std::string current_state = "Idle";
    f32         perception_radius = 15.0f;
    f32         attack_range = 2.0f;
    Vec3        target_position{0.0f};
};

struct RagdollComponent {
    bool is_ragdoll_active = false;
    f32  total_mass = 75.0f;
};

// =========================================================================
// 9. Animation Extensions
// =========================================================================

struct BoneNodeComponent {
    std::string bone_name = "Spine_01";
    i32         bone_index = 0;
    Mat4        inverse_bind_pose{1.0f};
};

struct BoneAttachmentComponent {
    std::string joint_name = "Hand_R";
    Vec3        offset{0.0f};
    Quat        rotation_offset{1.0f, 0.0f, 0.0f, 0.0f};
};

struct AnimationTreeComponent {
    f32 blend_parameter = 0.0f; // e.g. 0.0 (Idle) -> 1.0 (Run)
    std::string clip_a = "Idle";
    std::string clip_b = "Run";
};

struct IKSolverComponent {
    std::string target_joint = "Foot_L";
    Vec3        ik_target_pos{0.0f};
    f32         weight = 1.0f;
};

struct MorphTargetComponent {
    std::vector<std::string> target_names;
    std::vector<f32>         weights;
};

// =========================================================================
// 10. AI & Navigation
// =========================================================================

struct NavMeshBoundsComponent {
    Vec3 size{50.0f, 10.0f, 50.0f};
    f32  cell_size = 0.3f;
    f32  agent_height = 1.8f;
    f32  agent_radius = 0.4f;
    f32  agent_max_climb = 0.4f;
    f32  agent_max_slope = 45.0f;
};

struct NavMeshObstacleComponent {
    Vec3 size{1.0f, 2.0f, 1.0f};
    bool carve_navmesh = true;
};

struct NavMeshLinkComponent {
    Vec3 start_point{0.0f};
    Vec3 end_point{0.0f, 2.0f, 3.0f};
    bool is_bidirectional = true;
};

struct NavigationAgentComponent {
    Vec3 destination{0.0f};
    f32  speed = 3.5f;
    f32  stopping_distance = 0.5f;
    bool path_pending = false;
    std::vector<Vec3> path_points;
};

struct BehaviorTreeComponent {
    std::string tree_name = "PatrolAndChase";
    bool        is_running = true;
};

struct FSMComponent {
    std::string current_state = "Patrol";
    f32         time_in_state = 0.0f;
};

struct PerceptionSensorComponent {
    f32 sight_radius = 20.0f;
    f32 sight_fov_deg = 90.0f;
    f32 hearing_radius = 12.0f;
    bool can_see_player = false;
};

struct BlackboardComponent {
    std::string target_actor_name;
    Vec3        last_seen_position{0.0f};
    f32         alert_level = 0.0f; // 0.0 (Calm) to 1.0 (Combat)
};

struct SplinePathComponent {
    std::vector<Vec3> points;
    bool closed_loop = true;
    f32  path_length = 0.0f;
};

// =========================================================================
// 11. Visual Effects (VFX)
// =========================================================================

struct VFXGraphComponent {
    std::string graph_asset = "assets/vfx/fire_sparks.vfx";
    i32 max_particles = 10000;
    bool is_playing = true;
};

struct TrailComponent {
    f32  lifetime = 0.5f;
    f32  min_vertex_distance = 0.1f;
    Vec4 start_color{1.0f, 0.8f, 0.2f, 1.0f};
    Vec4 end_color{1.0f, 0.1f, 0.0f, 0.0f};
    f32  width = 0.2f;
};

struct BeamEmitterComponent {
    Vec3 target_point{0.0f, 0.0f, 10.0f};
    f32  beam_width = 0.15f;
    Vec4 beam_color{0.2f, 0.8f, 1.0f, 1.0f};
    f32  noise_amplitude = 0.1f;
};

// =========================================================================
// 12. Audio & Acoustics
// =========================================================================

struct AudioReverbZoneComponent {
    f32  radius = 20.0f;
    f32  reverb_decay_time = 1.5f;
    f32  wet_mix = 0.4f;
    f32  dry_mix = 0.6f;
};

struct MusicTrackComponent {
    std::string track_path = "assets/audio/combat_theme.ogg";
    f32 volume = 0.8f;
    bool loop = true;
    f32 crossfade_duration_sec = 2.0f;
};

// =========================================================================
// 13. Gameplay, Stats & RPG Systems
// =========================================================================

struct HealthComponent {
    f32 current_health = 100.0f;
    f32 max_health     = 100.0f;
    f32 shield         = 0.0f;
    f32 max_shield     = 50.0f;
    bool is_dead       = false;

    void TakeDamage(f32 amount) {
        if (is_dead) return;
        if (shield > 0.0f) {
            if (shield >= amount) { shield -= amount; return; }
            amount -= shield;
            shield = 0.0f;
        }
        current_health = glm::max(0.0f, current_health - amount);
        if (current_health <= 0.0f) is_dead = true;
    }

    void Heal(f32 amount) {
        if (is_dead) return;
        current_health = glm::min(max_health, current_health + amount);
    }
};

struct DamageReceiverComponent {
    f32 armor_multiplier = 1.0f; // 0.8 = 20% damage reduction
    f32 fire_resistance  = 0.0f;
};

struct HitboxComponent {
    Vec3 box_half_size{0.5f};
    f32  damage = 25.0f;
    f32  knockback = 5.0f;
    bool is_active = false;
};

struct HurtboxComponent {
    Vec3 box_half_size{0.5f};
    f32  vulnerability_multiplier = 1.0f; // 2.0 = headshot
};

struct InventoryComponent {
    i32 max_slots = 20;
    f32 max_weight_kg = 50.0f;
    std::vector<std::string> item_names;
};

struct EquipmentComponent {
    std::string main_hand_item = "Broadsword";
    std::string off_hand_item  = "WoodenShield";
    std::string armor_chest    = "IronPlate";
};

struct InteractableComponent {
    std::string prompt_text = "Press [E] to Interact";
    f32         interaction_radius = 2.5f;
    bool        is_interactable = true;
};

struct AbilityComponent {
    std::string ability_name = "Fireball";
    f32 mana_cost = 25.0f;
    f32 cooldown_sec = 4.0f;
    f32 current_cooldown = 0.0f;
};

struct QuestTriggerComponent {
    std::string quest_id = "MainQuest_01";
    std::string objective_name = "Enter Ancient Ruins";
    bool        trigger_once = true;
    bool        is_completed = false;
};

struct SavePointComponent {
    std::string checkpoint_name = "Bonfire_01";
    bool is_activated = false;
};

struct SpawnerComponent {
    std::string prefab_name = "EnemyGoblin";
    f32         spawn_interval = 5.0f;
    i32         max_spawn_count = 10;
    i32         current_spawned = 0;
    f32         timer = 0.0f;
};

// =========================================================================
// 14. Networking & Replication
// =========================================================================

struct NetworkIdentityComponent {
    u32 net_id = 0;
    bool is_server_authority = true;
    bool is_locally_owned = true;
};

struct NetworkTransformComponent {
    Vec3 synced_position{0.0f};
    Quat synced_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    f32  interpolation_speed = 15.0f;
};

struct NetworkAnimatorComponent {
    i32 synced_anim_state = 0;
    f32 synced_clip_time = 0.0f;
};

struct ReplicationManagerComponent {
    u32 tick_rate_hz = 30;
    f32 send_rate_sec = 1.0f / 30.0f;
};

struct RPCComponent {
    std::vector<std::string> pending_rpc_names;
};

// =========================================================================
// 15. User Interface (UI & HUD)
// =========================================================================

struct CanvasComponent {
    i32 sorting_order = 0;
    bool match_viewport_aspect = true;
};

struct UIPanelComponent {
    Vec2 position{0.0f};
    Vec2 size{200.0f, 150.0f};
    Vec4 background_color{0.1f, 0.1f, 0.12f, 0.85f};
};

struct UIContainerComponent {
    enum class LayoutDirection : u8 { Vertical = 0, Horizontal, Grid };
    LayoutDirection direction = LayoutDirection::Vertical;
    f32 spacing = 8.0f;
};

struct UIButtonComponent {
    std::string label = "Button";
    bool is_pressed = false;
};

struct UILabelComponent {
    std::string text = "Label";
    f32 font_size = 18.0f;
    Vec4 text_color{1.0f};
};

struct UIImageComponent {
    std::string texture_path;
    Vec2 size{64.0f, 64.0f};
};

struct WorldSpaceUIComponent {
    std::string title = "Health Bar";
    Vec2        size{1.0f, 0.15f};
    Vec3        offset{0.0f, 2.2f, 0.0f};
    f32         fill_percent = 1.0f;
    Vec4        bar_color{0.2f, 0.9f, 0.3f, 1.0f};
};

struct MiniMapComponent {
    std::string icon_name = "PlayerIcon";
    Vec4        icon_color{0.2f, 0.6f, 1.0f, 1.0f};
    bool        show_on_radar = true;
};

// =========================================================================
// 16. Scene Management, Optimization & Diagnostics
// =========================================================================

struct LODGroupComponent {
    f32 lod1_distance = 15.0f;
    f32 lod2_distance = 35.0f;
    f32 cull_distance = 75.0f;
    i32 active_lod    = 0;
};

struct HLODComponent {
    std::string proxy_mesh_path;
    f32 transition_distance = 100.0f;
    bool is_active = false;
};

struct OcclusionPortalComponent {
    Vec2 portal_dimensions{2.0f, 3.0f};
    bool is_open = true;
};

struct WorldPartitionComponent {
    Vec3 cell_index{0.0f};
    f32  cell_size = 256.0f;
    bool is_loaded = true;
};

struct DebugDrawComponent {
    bool draw_wireframe_bounds = true;
    bool draw_velocity_vector  = true;
    bool draw_sensor_rays      = true;
};

struct SignalBusComponent {
    std::vector<std::string> registered_signals;
};

} // namespace lucida
