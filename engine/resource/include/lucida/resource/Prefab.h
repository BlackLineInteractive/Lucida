// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Complete Game Engine Node Archetypes & Prefabs ecosystem (GEA ch.14 Gameplay Foundations).

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
    // 1. Core Spatial & Scene Nodes
    static Entity CreateSpatialNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "SpatialNode");
    static Entity CreateStaticMeshNode(World& world, MeshHandle mesh, const Vec3& pos = Vec3(0.0f), const std::string& name = "StaticMesh");

    // 2. Geometry, Decals & 2D in 3D
    static Entity CreateDecalNode(World& world, const std::string& tex, const Vec3& pos = Vec3(0.0f), const std::string& name = "Decal");
    static Entity CreateBillboardNode(World& world, const std::string& tex, const Vec3& pos = Vec3(0.0f), const std::string& name = "Billboard");
    static Entity CreateText3DNode(World& world, const std::string& text, const Vec3& pos = Vec3(0.0f), const std::string& name = "Text3D");

    // 3. Lighting & Atmosphere
    static Entity CreateDirectionalLightNode(World& world, const Vec3& dir = Vec3(0.0f, -1.0f, 0.2f),
                                            const Vec3& col = Vec3(1.0f, 0.98f, 0.95f), f32 intensity = 10.0f,
                                            const std::string& name = "DirectionalLight");
    static Entity CreatePointLightNode(World& world, const Vec3& pos = Vec3(0.0f, 3.0f, 0.0f),
                                       const Vec3& col = Vec3(1.0f, 0.8f, 0.5f), f32 intensity = 50.0f, f32 radius = 8.0f,
                                       const std::string& name = "PointLight");
    static Entity CreateSpotLightNode(World& world, const Vec3& pos = Vec3(0.0f, 4.0f, 0.0f),
                                      const Vec3& dir = Vec3(0.0f, -1.0f, 0.0f), f32 inner_deg = 20.0f, f32 outer_deg = 40.0f,
                                      const std::string& name = "SpotLight");
    static Entity CreateFogVolumeNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(20.0f, 10.0f, 20.0f),
                                      f32 density = 0.05f, const std::string& name = "FogVolume");
    static Entity CreatePostProcessVolumeNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(10.0f),
                                             const std::string& name = "PostProcessVolume");
    static Entity CreateReflectionProbeNode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), f32 radius = 15.0f,
                                           const std::string& name = "ReflectionProbe");

    // 4. Cameras & Cinematics
    static Entity CreatePawnNode(World& world, const Vec3& pos = Vec3(0.0f, 1.8f, 5.0f), const std::string& name = "PlayerPawn");
    static Entity CreateCinematicCameraNode(World& world, const Vec3& pos = Vec3(0.0f, 1.8f, 5.0f),
                                           f32 focal_length_mm = 50.0f, f32 f_stop = 2.8f,
                                           const std::string& name = "CinematicCamera");
    static Entity CreateSpringArmNode(World& world, const Vec3& pos = Vec3(0.0f, 1.5f, 0.0f), f32 arm_length = 4.5f,
                                      const std::string& name = "SpringArm");
    static Entity CreateDollyTrackNode(World& world, const std::vector<Vec3>& waypoints, f32 speed = 2.0f,
                                       const std::string& name = "CameraDollyTrack");

    // 5. World, Nature, Terrain & Water
    static Entity CreateTerrainNode(World& world, const TerrainComponent& config = TerrainComponent{},
                                    i32 material_index = 0, const std::string& name = "Terrain");
    static Entity CreateWaterBodyNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec2& size = Vec2(100.0f, 100.0f),
                                      const std::string& name = "WaterBody");
    static Entity CreateRiverNode(World& world, const std::vector<Vec3>& points, f32 width = 4.0f,
                                  const std::string& name = "River");
    static Entity CreateFoliageNode(World& world, const std::string& mesh_path, i32 count = 500,
                                    const std::string& name = "FoliageInstancer");
    static Entity CreateWindSourceNode(World& world, const Vec3& dir = Vec3(1.0f, 0.0f, 0.0f), f32 speed = 5.0f,
                                       const std::string& name = "WindSource");

    // 6. Physics, Queries & Constraints
    static Entity CreatePhysicsActorNode(World& world, PrimitiveType shape = PrimitiveType::Box,
                                         BodyType body_type = BodyType::Dynamic, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f),
                                         i32 material_index = 0, const std::string& name = "PhysicsActor");
    static Entity CreateTriggerVolumeNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                          const Vec3& half_size = Vec3(1.0f), const std::string& name = "TriggerVolume");
    static Entity CreateRaycastSensorNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                          const Vec3& dir = Vec3(0.0f, -1.0f, 0.0f), f32 max_dist = 10.0f,
                                          const std::string& name = "RaycastSensor");
    static Entity CreatePhysicsJointNode(World& world, JointType type = JointType::Hinge, const Vec3& anchor = Vec3(0.0f),
                                         const std::string& name = "PhysicsJoint");
    static Entity CreateBuoyancyNode(World& world, const Vec3& pos = Vec3(0.0f), f32 water_level = 0.0f,
                                     const std::string& name = "BuoyancyActor");

    // 7. Vehicles
    static Entity CreateVehicleNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                    i32 material_index = 0, const std::string& name = "Vehicle");

    // 8. Characters & Controllers
    static Entity CreateCharacterBodyNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                          const std::string& name = "CharacterBody");
    static Entity CreateAIEnemyNode(World& world, const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                    const std::string& name = "AIEnemy");

    // 9. AI & Navigation
    static Entity CreateNavMeshBoundsNode(World& world, const Vec3& pos = Vec3(0.0f), const Vec3& size = Vec3(50.0f, 10.0f, 50.0f),
                                          const std::string& name = "NavMeshBounds");
    static Entity CreatePatrolPathNode(World& world, const std::vector<Vec3>& points,
                                       const std::string& name = "PatrolPath");

    // 10. Visual Effects (VFX)
    static Entity CreateTrailNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "TrailEffect");
    static Entity CreateBeamEmitterNode(World& world, const Vec3& start = Vec3(0.0f), const Vec3& target = Vec3(0.0f, 0.0f, 10.0f),
                                        const std::string& name = "LaserBeam");

    // 11. Spatial Audio
    static Entity CreateAudioReverbZoneNode(World& world, const Vec3& pos = Vec3(0.0f), f32 radius = 20.0f,
                                           const std::string& name = "AudioReverbZone");

    // 12. Gameplay, RPG & Interaction
    static Entity CreateInteractableNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& prompt = "Press [E] to Open",
                                         const std::string& name = "InteractableChest");
    static Entity CreateItemSpawnerNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& item = "HealthPotion",
                                        const std::string& name = "ItemSpawner");
    static Entity CreateQuestTriggerNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& quest_id = "Quest_01",
                                         const std::string& name = "QuestTrigger");

    // 13. UI & Optimization
    static Entity CreateWorldSpaceUINode(World& world, const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f), const std::string& title = "Boss Health",
                                         const std::string& name = "WorldSpaceUI");
    static Entity CreateLODGroupNode(World& world, const Vec3& pos = Vec3(0.0f), const std::string& name = "LODGroup");
};

} // namespace lucida
