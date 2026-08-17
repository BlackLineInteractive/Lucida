// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/core/math/Math.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/render/GpuTypes.h"
#include "lucida/runtime/GameplayComponents.h"

#include <string>
#include <vector>

namespace lucida {

class World;

class Prefab {
public:
    // Core & Spatial Hierarchy
    static Entity CreateSpatialNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "SpatialNode");
    static Entity CreateTransformNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "TransformNode");
    static Entity CreatePivotNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& pivot = Vec3(0.0f), const std::string& name = "PivotNode");
    static Entity CreateRootNode(World& world, const std::string& scene_name = "MainScene", const std::string& name = "RootNode");

    // Geometry & Meshes
    static Entity CreateStaticMeshNode(World& world, MeshHandle mesh, const Vec3& pos = Vec3(0.0f), const std::string& name = "StaticMesh");
    static Entity CreateSkinnedMeshNode(World& world, MeshHandle mesh, const Vec3& pos = Vec3(0.0f), const std::string& name = "SkinnedMesh");
    static Entity CreateInstancedMeshNode(World& world, const std::string& path, i32 count = 100, const std::string& name = "InstancedMesh");
    static Entity CreateProceduralMeshNode(World& world, const std::string& type = "PerlinTerrain", const std::string& name = "ProceduralMesh");
    static Entity CreateDynamicMeshNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "DynamicMesh");
    static Entity CreateCSGNode(World& world, CSGOperation op = CSGOperation::Difference, const std::string& name = "CSGNode");
    static Entity CreateDecalNode(World& world, const std::string& tex, const Vec3& pos = Vec3(0.0f), const std::string& name = "Decal");
    static Entity CreateBillboardNode(World& world, const std::string& tex, const Vec3& pos = Vec3(0.0f), const std::string& name = "Billboard");
    static Entity CreateText3DNode(World& world, const std::string& text, const Vec3& pos = Vec3(0.0f), const std::string& name = "Text3D");

    // Lighting, Atmosphere & Post-Processing
    static Entity CreateDirectionalLightNode(World& world, const Vec3& dir = Vec3(0.0f, -1.0f, 0.2f),
                                            const Vec3& col = Vec3(1.0f, 0.98f, 0.95f), f32 intensity = 10.0f, const std::string& name = "DirectionalLight");
    static Entity CreatePointLightNode(World& world, const Vec3& pos = Vec3(0.0f, 3.0f, 0.0f),
                                       const Vec3& col = Vec3(1.0f, 0.8f, 0.5f), f32 intensity = 50.0f, f32 radius = 8.0f, const std::string& name = "PointLight");
    static Entity CreateSpotLightNode(World& world, const Vec3& pos = Vec3(0.0f, 4.0f, 0.0f),
                                      const Vec3& dir = Vec3(0.0f, -1.0f, 0.0f), f32 inner_deg = 20.0f, f32 outer_deg = 40.0f, const std::string& name = "SpotLight");
    static Entity CreateAreaLightNode(World& world, const Vec3& pos = Vec3(0.0f, 3.0f, 0.0f), const Vec3& col = Vec3(1.0f), f32 intensity = 60.0f, const std::string& name = "AreaLight");
    static Entity CreateSkyboxNode(World& world, const std::string& cubemap = "skybox.hdr", const std::string& name = "Skybox");
    static Entity CreateVolumetricCloudNode(World& world, f32 height = 800.0f, f32 thickness = 400.0f, const std::string& name = "VolumetricCloud");
    static Entity CreateFogVolumeNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(20.0f, 10.0f, 20.0f), f32 density = 0.05f, const std::string& name = "FogVolume");
    static Entity CreatePostProcessVolumeNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(10.0f), const std::string& name = "PostProcessVolume");
    static Entity CreateReflectionProbeNode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), f32 radius = 15.0f, const std::string& name = "ReflectionProbe");
    static Entity CreateLightProbeNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "LightProbe");

    // Cameras & Cinematics
    static Entity CreateCameraNode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 6.0f), f32 fov = 60.0f, const std::string& name = "Camera");
    static Entity CreateCinematicCameraNode(World& world, const Vec3& pos = Vec3(0.0f, 1.8f, 5.0f), f32 focal_length_mm = 50.0f, f32 f_stop = 2.8f, const std::string& name = "CinematicCamera");
    static Entity CreateSpringArmNode(World& world, const Vec3& pos = Vec3(0.0f, 1.5f, 0.0f), f32 arm_length = 4.5f, const std::string& name = "SpringArm");
    static Entity CreateDollyTrackNode(World& world, const std::vector<Vec3>& waypoints, f32 speed = 2.0f, const std::string& name = "CameraDollyTrack");

    // World, Terrain & Water
    static Entity CreateTerrainNode(World& world, const TerrainComponent& config = TerrainComponent{}, i32 material_index = 0, const std::string& name = "Terrain");
    static Entity CreateVoxelTerrainNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "VoxelTerrain");
    static Entity CreateFoliageNode(World& world, const std::string& mesh_path, i32 count = 500, const std::string& name = "FoliageInstancer");
    static Entity CreateWaterBodyNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec2& size = Vec2(100.0f, 100.0f), const std::string& name = "WaterBody");
    static Entity CreateRiverNode(World& world, const std::vector<Vec3>& points, f32 width = 4.0f, const std::string& name = "River");
    static Entity CreateWindSourceNode(World& world, const Vec3& dir = Vec3(1.0f, 0.0f, 0.0f), f32 speed = 5.0f, const std::string& name = "WindSource");

    // Rigid Bodies, Collisions & Queries
    static Entity CreatePhysicsActorNode(World& world, PrimitiveType shape = PrimitiveType::Box, BodyType body_type = BodyType::Dynamic, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), i32 material_index = 0, const std::string& name = "PhysicsActor");
    static Entity CreateStaticBodyNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "StaticBody");
    static Entity CreateKinematicBodyNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "KinematicBody");
    static Entity CreateCollisionShapeNode(World& world, ShapeType shape = ShapeType::Box, const Vec3& pos = Vec3(0.0f), const std::string& name = "CollisionShape");
    static Entity CreateTriggerVolumeNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const Vec3& half_size = Vec3(1.0f), const std::string& name = "TriggerVolume");
    static Entity CreateRaycastSensorNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const Vec3& dir = Vec3(0.0f, -1.0f, 0.0f), f32 max_dist = 10.0f, const std::string& name = "RaycastSensor");
    static Entity CreateShapeCastNode(World& world, const Vec3& pos = Vec3(0.0f), f32 radius = 0.5f, const std::string& name = "ShapeCast");
    static Entity CreatePhysicalMaterialNode(World& world, f32 friction = 0.6f, f32 restitution = 0.1f, const std::string& name = "PhysicalMaterial");

    // Constraints, Soft Bodies & Destruction
    static Entity CreatePhysicsJointNode(World& world, JointType type = JointType::Hinge, const Vec3& anchor = Vec3(0.0f), const std::string& name = "PhysicsJoint");
    static Entity CreateRopeNode(World& world, const Vec3& start = Vec3(0.0f), f32 length = 5.0f, const std::string& name = "Rope");
    static Entity CreateClothNode(World& world, const Vec3& pos = Vec3(0.0f, 3.0f, 0.0f), const std::string& name = "Cloth");
    static Entity CreateSoftBodyNode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), const std::string& name = "SoftBody");
    static Entity CreateBuoyancyNode(World& world, const Vec3& pos = Vec3(0.0f), f32 water_level = 0.0f, const std::string& name = "BuoyancyActor");
    static Entity CreateDestructibleMeshNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "DestructibleMesh");

    // Vehicles
    static Entity CreateVehicleNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), i32 material_index = 0, const std::string& name = "Vehicle");
    static Entity CreateWheeledVehicleNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const std::string& name = "WheeledVehicle");
    static Entity CreateVehicleWheelNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "VehicleWheel");
    static Entity CreateTrackedVehicleNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const std::string& name = "TrackedTank");
    static Entity CreateAircraftNode(World& world, const Vec3& pos = Vec3(0.0f, 10.0f, 0.0f), const std::string& name = "Aircraft");
    static Entity CreateWatercraftNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "Watercraft");

    // Pawns & Controllers
    static Entity CreatePawnNode(World& world, const Vec3& pos = Vec3(0.0f, 1.8f, 5.0f), const std::string& name = "PlayerPawn");
    static Entity CreateCharacterBodyNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const std::string& name = "CharacterBody");
    static Entity CreateCharacterMovementNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "CharacterMovement");
    static Entity CreatePlayerControllerNode(World& world, const std::string& name = "PlayerController");
    static Entity CreateAIControllerNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f), const std::string& name = "AIEnemy");
    static Entity CreatePlayerInputNode(World& world, const std::string& name = "PlayerInput");
    static Entity CreateRagdollNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "Ragdoll");

    // Animation & Skeletal Hierarchy
    static Entity CreateSkeletonNode(World& world, const std::string& name = "Skeleton");
    static Entity CreateBoneNode(World& world, const std::string& bone_name = "Spine_01", const std::string& name = "BoneNode");
    static Entity CreateSocketNode(World& world, const std::string& joint_name = "Hand_R", const std::string& name = "SocketNode");
    static Entity CreateAnimationPlayerNode(World& world, const std::string& clip_name = "Idle", const std::string& name = "AnimationPlayer");
    static Entity CreateAnimationTreeBlendNode(World& world, const std::string& name = "AnimationTreeBlend");
    static Entity CreateIKSolverNode(World& world, const std::string& target_joint = "Foot_L", const std::string& name = "IKSolver");
    static Entity CreateMorphTargetNode(World& world, const std::string& name = "MorphTarget");

    // AI & Navigation
    static Entity CreateNavMeshBoundsNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(50.0f, 10.0f, 50.0f), const std::string& name = "NavMeshBounds");
    static Entity CreateNavMeshObstacleNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "NavMeshObstacle");
    static Entity CreateNavMeshLinkNode(World& world, const Vec3& start = Vec3(0.0f), const Vec3& end = Vec3(0.0f, 2.0f, 3.0f), const std::string& name = "NavMeshLink");
    static Entity CreateNavigationAgentNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "NavigationAgent");
    static Entity CreateBehaviorTreeNode(World& world, const std::string& tree_name = "PatrolAndChase", const std::string& name = "BehaviorTree");
    static Entity CreateFSMNode(World& world, const std::string& initial_state = "Patrol", const std::string& name = "StatechartFSM");
    static Entity CreatePerceptionSensorNode(World& world, f32 sight_radius = 20.0f, const std::string& name = "PerceptionSensor");
    static Entity CreateBlackboardNode(World& world, const std::string& name = "AIBlackboard");
    static Entity CreatePatrolPathNode(World& world, const std::vector<Vec3>& points, const std::string& name = "PatrolPath");

    // Visual Effects (VFX)
    static Entity CreateParticleEmitterNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "ParticleEmitter");
    static Entity CreateVFXGraphNode(World& world, const std::string& graph_path = "fire_sparks.vfx", const std::string& name = "VFXGraph");
    static Entity CreateTrailNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "TrailEffect");
    static Entity CreateBeamEmitterNode(World& world, const Vec3& start = Vec3(0.0f), const Vec3& target = Vec3(0.0f, 0.0f, 10.0f), const std::string& name = "LaserBeam");

    // Spatial Audio
    static Entity CreateAudioSourceNode(World& world, const std::string& sound_path = "assets/sound/sfx.wav", const Vec3& pos = Vec3(0.0f), const std::string& name = "AudioSource");
    static Entity CreateSpatialAudioNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "SpatialAudio");
    static Entity CreateAudioListenerNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "AudioListener");
    static Entity CreateAudioReverbZoneNode(World& world, const Vec3& pos = Vec3(0.0f), f32 radius = 20.0f, const std::string& name = "AudioReverbZone");
    static Entity CreateMusicTrackNode(World& world, const std::string& track_path = "assets/audio/music.ogg", const std::string& name = "MusicTrack");

    // Gameplay Systems & Stats
    static Entity CreateHealthNode(World& world, f32 max_hp = 100.0f, const std::string& name = "HealthNode");
    static Entity CreateDamageReceiverNode(World& world, const std::string& name = "DamageReceiver");
    static Entity CreateHitboxNode(World& world, f32 damage = 25.0f, const std::string& name = "Hitbox");
    static Entity CreateHurtboxNode(World& world, const std::string& name = "Hurtbox");
    static Entity CreateInventoryNode(World& world, i32 max_slots = 20, const std::string& name = "Inventory");
    static Entity CreateEquipmentNode(World& world, const std::string& name = "Equipment");
    static Entity CreateInteractableNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& prompt = "Press [E] to Open", const std::string& name = "InteractableChest");
    static Entity CreateAbilityNode(World& world, const std::string& ability = "Fireball", const std::string& name = "AbilityNode");
    static Entity CreateQuestTriggerNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& quest_id = "Quest_01", const std::string& name = "QuestTrigger");
    static Entity CreateSavePointNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "SavePoint");
    static Entity CreateItemSpawnerNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& item = "HealthPotion", const std::string& name = "ItemSpawner");

    // Networking & Replication
    static Entity CreateNetworkIdentityNode(World& world, u32 net_id = 1, const std::string& name = "NetworkIdentity");
    static Entity CreateNetworkTransformNode(World& world, const std::string& name = "NetworkTransform");
    static Entity CreateNetworkAnimatorNode(World& world, const std::string& name = "NetworkAnimator");
    static Entity CreateReplicationManagerNode(World& world, u32 tick_rate = 30, const std::string& name = "ReplicationManager");
    static Entity CreateRPCNode(World& world, const std::string& name = "RPCNode");

    // User Interface (UI & HUD)
    static Entity CreateCanvasLayerNode(World& world, i32 sort_order = 0, const std::string& name = "CanvasLayer");
    static Entity CreateUIPanelNode(World& world, const Vec2& size = Vec2(200.0f, 150.0f), const std::string& name = "UIPanel");
    static Entity CreateUIContainerNode(World& world, const std::string& name = "UIContainer");
    static Entity CreateUIButtonNode(World& world, const std::string& label = "Play", const std::string& name = "UIButton");
    static Entity CreateUILabelNode(World& world, const std::string& text = "Score: 0", const std::string& name = "UILabel");
    static Entity CreateUIImageNode(World& world, const std::string& tex = "icon.png", const std::string& name = "UIImage");
    static Entity CreateWorldSpaceUINode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), const std::string& title = "Boss Health", const std::string& name = "WorldSpaceUI");
    static Entity CreateMiniMapNode(World& world, const std::string& name = "MiniMap");

    // Scene Management & Optimization
    static Entity CreateLODGroupNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "LODGroup");
    static Entity CreateHLODNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "HLODProxy");
    static Entity CreateOcclusionPortalNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "OcclusionPortal");
    static Entity CreateWorldPartitionCellNode(World& world, const Vec3& cell = Vec3(0.0f), const std::string& name = "WorldPartitionCell");
    static Entity CreateDebugDrawNode(World& world, const std::string& name = "DebugDrawNode");
    static Entity CreateTimerNode(World& world, f32 duration = 1.0f, const std::string& name = "TimerNode");
    static Entity CreateSignalBusNode(World& world, const std::string& name = "SignalBus");
};

} // namespace lucida
