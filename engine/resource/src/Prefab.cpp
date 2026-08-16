// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/Prefab.h"
#include "lucida/audio/Components.h"
#include "lucida/core/math/Tween.h"
#include "lucida/resource/Terrain.h"
#include "lucida/runtime/Particles.h"
#include "lucida/runtime/World.h"

namespace lucida {

namespace {
inline Entity MakeBaseEntity(World& world, const Vec3& pos, const std::string& name, const std::string& tag, u32 layer = 0) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());
    LocalTransform lt{};
    lt.position = pos;
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);
    entities.Add<TagComponent>(e, TagComponent{tag, layer});
    return e;
}
} // namespace

// =========================================================================
// 1. Core & Spatial Hierarchy (4)
// =========================================================================

Entity Prefab::CreateSpatialNode(World& world, const Vec3& pos, const std::string& name) {
    return MakeBaseEntity(world, pos, name, "Spatial", 0);
}

Entity Prefab::CreateTransformNode(World& world, const Vec3& pos, const std::string& name) {
    return MakeBaseEntity(world, pos, name, "Transform", 0);
}

Entity Prefab::CreatePivotNode(World& world, const Vec3& pos, const Vec3& pivot, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Pivot", 0);
    world.Entities().Add<PivotComponent>(e, PivotComponent{pivot, false});
    return e;
}

Entity Prefab::CreateRootNode(World& world, const std::string& scene_name, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Root", 0);
    world.Entities().Add<RootComponent>(e, RootComponent{scene_name, true});
    return e;
}

// =========================================================================
// 2. Geometry & Meshes (9)
// =========================================================================

Entity Prefab::CreateStaticMeshNode(World& world, MeshHandle mesh, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "StaticMesh", 0);
    MeshInstance mi{};
    mi.mesh = mesh;
    world.Entities().Add<MeshInstance>(e, mi);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
    return e;
}

Entity Prefab::CreateSkinnedMeshNode(World& world, MeshHandle mesh, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "SkinnedMesh", 0);
    MeshInstance mi{};
    mi.mesh = mesh;
    world.Entities().Add<MeshInstance>(e, mi);
    world.Entities().Add<BoneAttachmentComponent>(e, BoneAttachmentComponent{});
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
    return e;
}

Entity Prefab::CreateInstancedMeshNode(World& world, const std::string& path, i32 count, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "InstancedMesh", 0);
    InstancedMeshComponent imc{};
    imc.mesh_path = path;
    imc.instance_transforms.resize(count, Mat4(1.0f));
    world.Entities().Add<InstancedMeshComponent>(e, imc);
    return e;
}

Entity Prefab::CreateProceduralMeshNode(World& world, const std::string& type, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "ProceduralMesh", 0);
    ProceduralMeshComponent pmc{};
    pmc.generator_type = type;
    world.Entities().Add<ProceduralMeshComponent>(e, pmc);
    return e;
}

Entity Prefab::CreateDynamicMeshNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "DynamicMesh", 0);
    world.Entities().Add<DynamicMeshComponent>(e, DynamicMeshComponent{});
    return e;
}

Entity Prefab::CreateCSGNode(World& world, CSGOperation op, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "CSGNode", 0);
    CSGComponent csg{};
    csg.operation = op;
    world.Entities().Add<CSGComponent>(e, csg);
    return e;
}

Entity Prefab::CreateDecalNode(World& world, const std::string& tex, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Decal", 0);
    DecalComponent dec{};
    dec.texture_path = tex;
    world.Entities().Add<DecalComponent>(e, dec);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-dec.size * 0.5f, dec.size * 0.5f});
    return e;
}

Entity Prefab::CreateBillboardNode(World& world, const std::string& tex, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Billboard", 0);
    BillboardComponent bb{};
    bb.texture_path = tex;
    world.Entities().Add<BillboardComponent>(e, bb);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-bb.size.x * 0.5f, -bb.size.y * 0.5f, -0.05f),
                                                     Vec3( bb.size.x * 0.5f,  bb.size.y * 0.5f,  0.05f)});
    return e;
}

Entity Prefab::CreateText3DNode(World& world, const std::string& text, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Text3D", 0);
    Text3DComponent t{};
    t.text = text;
    world.Entities().Add<Text3DComponent>(e, t);
    return e;
}

// =========================================================================
// 3. Lighting, Atmosphere & Post-Processing (10)
// =========================================================================

Entity Prefab::CreateDirectionalLightNode(World& world, const Vec3& dir, const Vec3& col, f32 intensity, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f, 10.0f, 0.0f), name, "Light", 0);
    LightSource light{};
    light.type = LightType::Directional;
    light.direction = glm::normalize(dir);
    light.color = col;
    light.intensity = intensity;
    world.Entities().Add<LightSource>(e, light);
    return e;
}

Entity Prefab::CreatePointLightNode(World& world, const Vec3& pos, const Vec3& col, f32 intensity, f32 radius, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Light", 0);
    LightSource light{};
    light.type = LightType::Point;
    light.color = col;
    light.intensity = intensity;
    light.radius = radius;
    world.Entities().Add<LightSource>(e, light);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-radius), Vec3(radius)});
    return e;
}

Entity Prefab::CreateSpotLightNode(World& world, const Vec3& pos, const Vec3& dir, f32 inner_deg, f32 outer_deg, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Light", 0);
    LightSource light{};
    light.type = LightType::Spot;
    light.direction = glm::normalize(dir);
    light.inner_angle = inner_deg;
    light.outer_angle = outer_deg;
    world.Entities().Add<LightSource>(e, light);
    return e;
}

Entity Prefab::CreateAreaLightNode(World& world, const Vec3& pos, const Vec3& col, f32 intensity, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Light", 0);
    LightSource light{};
    light.type = LightType::Area;
    light.color = col;
    light.intensity = intensity;
    light.radius = 2.0f;
    world.Entities().Add<LightSource>(e, light);
    return e;
}

Entity Prefab::CreateSkyboxNode(World& world, const std::string& cubemap, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Skybox", 0);
    SkyboxComponent sky{};
    sky.cubemap_path = cubemap;
    world.Entities().Add<SkyboxComponent>(e, sky);
    return e;
}

Entity Prefab::CreateVolumetricCloudNode(World& world, f32 height, f32 thickness, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f, height, 0.0f), name, "Clouds", 0);
    VolumetricCloudComponent vc{};
    vc.cloud_height = height;
    vc.cloud_thickness = thickness;
    world.Entities().Add<VolumetricCloudComponent>(e, vc);
    return e;
}

Entity Prefab::CreateFogVolumeNode(World& world, const Vec3& pos, const Vec3& size, f32 density, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "FogVolume", 0);
    FogVolumeComponent fog{};
    fog.bounds_size = size;
    fog.density = density;
    world.Entities().Add<FogVolumeComponent>(e, fog);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-size * 0.5f, size * 0.5f});
    return e;
}

Entity Prefab::CreatePostProcessVolumeNode(World& world, const Vec3& pos, const Vec3& size, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "PostProcess", 0);
    PostProcessVolumeComponent pp{};
    pp.bounds_size = size;
    world.Entities().Add<PostProcessVolumeComponent>(e, pp);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-size * 0.5f, size * 0.5f});
    return e;
}

Entity Prefab::CreateReflectionProbeNode(World& world, const Vec3& pos, f32 radius, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "ReflectionProbe", 0);
    ReflectionProbeComponent rp{};
    rp.radius = radius;
    world.Entities().Add<ReflectionProbeComponent>(e, rp);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-radius), Vec3(radius)});
    return e;
}

Entity Prefab::CreateLightProbeNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "LightProbe", 0);
    world.Entities().Add<LightProbeComponent>(e, LightProbeComponent{});
    return e;
}

// =========================================================================
// 4. Cameras & Cinematics (4)
// =========================================================================

Entity Prefab::CreateCameraNode(World& world, const Vec3& pos, f32 fov, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Camera", 0);
    CameraComponent cam{};
    cam.fov = fov;
    world.Entities().Add<CameraComponent>(e, cam);
    return e;
}

Entity Prefab::CreateCinematicCameraNode(World& world, const Vec3& pos, f32 focal_length_mm, f32 f_stop, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "CinematicCamera", 0);
    CameraComponent cam{};
    cam.fov = 2.0f * glm::degrees(std::atan(36.0f / (2.0f * focal_length_mm)));
    world.Entities().Add<CameraComponent>(e, cam);

    CinematicCameraComponent cine{};
    cine.focal_length_mm = focal_length_mm;
    cine.f_stop = f_stop;
    world.Entities().Add<CinematicCameraComponent>(e, cine);
    return e;
}

Entity Prefab::CreateSpringArmNode(World& world, const Vec3& pos, f32 arm_length, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "SpringArm", 0);
    SpringArmComponent sa{};
    sa.target_arm_length = arm_length;
    world.Entities().Add<SpringArmComponent>(e, sa);
    return e;
}

Entity Prefab::CreateDollyTrackNode(World& world, const std::vector<Vec3>& waypoints, f32 speed, const std::string& name) {
    Vec3 start_pos = waypoints.empty() ? Vec3(0.0f) : waypoints[0];
    Entity e = MakeBaseEntity(world, start_pos, name, "DollyTrack", 0);
    DollyTrackComponent dt{};
    dt.waypoints = waypoints;
    dt.speed = speed;
    world.Entities().Add<DollyTrackComponent>(e, dt);
    return e;
}

// =========================================================================
// 5. World, Terrain & Water (6)
// =========================================================================

Entity Prefab::CreateTerrainNode(World& world, const TerrainComponent& config, i32 material_index, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Terrain", 0);
    world.Entities().Add<TerrainComponent>(e, config);

    PrimitiveShape ps{};
    ps.type = PrimitiveType::Plane;
    ps.normal = Vec3(0.0f, 1.0f, 0.0f);
    ps.offset = 0.0f;
    world.Entities().Add<PrimitiveShape>(e, ps);
    world.Entities().Add<MaterialRef>(e, MaterialRef{material_index});

    RigidBody rb{};
    rb.type = BodyType::Static;
    rb.shape = ShapeType::Plane;
    world.Entities().Add<RigidBody>(e, rb);

    const f32 half_sz = config.size * 0.5f;
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-half_sz, -10.0f, -half_sz),
                                                     Vec3( half_sz,  config.max_height,  half_sz)});
    return e;
}

Entity Prefab::CreateVoxelTerrainNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "VoxelTerrain", 0);
    world.Entities().Add<VoxelTerrainComponent>(e, VoxelTerrainComponent{});
    return e;
}

Entity Prefab::CreateFoliageNode(World& world, const std::string& mesh_path, i32 count, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Foliage", 0);
    FoliageInstancerComponent fol{};
    fol.mesh_path = mesh_path;
    fol.instance_count = count;
    world.Entities().Add<FoliageInstancerComponent>(e, fol);
    return e;
}

Entity Prefab::CreateWaterBodyNode(World& world, const Vec3& pos, const Vec2& size, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Water", 0);
    WaterBodyComponent water{};
    water.plane_size = size;
    world.Entities().Add<WaterBodyComponent>(e, water);

    PrimitiveShape ps{};
    ps.type = PrimitiveType::Plane;
    ps.normal = Vec3(0.0f, 1.0f, 0.0f);
    world.Entities().Add<PrimitiveShape>(e, ps);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-size.x * 0.5f, -1.0f, -size.y * 0.5f),
                                                     Vec3( size.x * 0.5f,  1.0f,  size.y * 0.5f)});
    return e;
}

Entity Prefab::CreateRiverNode(World& world, const std::vector<Vec3>& points, f32 width, const std::string& name) {
    Vec3 start_pos = points.empty() ? Vec3(0.0f) : points[0];
    Entity e = MakeBaseEntity(world, start_pos, name, "River", 0);
    RiverSplineComponent river{};
    river.control_points = points;
    river.river_width = width;
    world.Entities().Add<RiverSplineComponent>(e, river);
    return e;
}

Entity Prefab::CreateWindSourceNode(World& world, const Vec3& dir, f32 speed, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "WindSource", 0);
    WindSourceComponent wind{};
    wind.direction = glm::normalize(dir);
    wind.speed = speed;
    world.Entities().Add<WindSourceComponent>(e, wind);
    return e;
}

// =========================================================================
// 6. Rigid Bodies, Collisions & Queries (8)
// =========================================================================

Entity Prefab::CreatePhysicsActorNode(World& world, PrimitiveType shape, BodyType body_type,
                                      const Vec3& pos, i32 material_index, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "PhysicsActor", 1);
    PrimitiveShape ps{};
    ps.type = shape;
    ps.size = Vec3(0.5f);
    world.Entities().Add<PrimitiveShape>(e, ps);

    RigidBody rb{};
    rb.type = body_type;
    rb.shape = (shape == PrimitiveType::Sphere) ? ShapeType::Sphere :
               (shape == PrimitiveType::Plane)  ? ShapeType::Plane : ShapeType::Box;
    world.Entities().Add<RigidBody>(e, rb);
    world.Entities().Add<MaterialRef>(e, MaterialRef{material_index});

    const Vec3 half = ps.HalfExtents();
    world.Entities().Add<LocalBounds>(e, LocalBounds{-half, half});
    return e;
}

Entity Prefab::CreateStaticBodyNode(World& world, const Vec3& pos, const std::string& name) {
    return CreatePhysicsActorNode(world, PrimitiveType::Box, BodyType::Static, pos, 0, name);
}

Entity Prefab::CreateKinematicBodyNode(World& world, const Vec3& pos, const std::string& name) {
    return CreatePhysicsActorNode(world, PrimitiveType::Box, BodyType::Kinematic, pos, 0, name);
}

Entity Prefab::CreateCollisionShapeNode(World& world, ShapeType shape, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "CollisionShape", 1);
    RigidBody rb{};
    rb.type = BodyType::Static;
    rb.shape = shape;
    world.Entities().Add<RigidBody>(e, rb);
    return e;
}

Entity Prefab::CreateTriggerVolumeNode(World& world, const Vec3& pos, const Vec3& half_size, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "TriggerVolume", 0);
    PrimitiveShape ps{};
    ps.type = PrimitiveType::Box;
    ps.size = half_size;
    world.Entities().Add<PrimitiveShape>(e, ps);

    RigidBody rb{};
    rb.type = BodyType::Static;
    rb.shape = ShapeType::Box;
    rb.is_trigger = true;
    world.Entities().Add<RigidBody>(e, rb);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-half_size, half_size});
    return e;
}

Entity Prefab::CreateRaycastSensorNode(World& world, const Vec3& pos, const Vec3& dir, f32 max_dist, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "RaycastSensor", 0);
    RaycastSensorComponent ray{};
    ray.direction = glm::normalize(dir);
    ray.max_distance = max_dist;
    world.Entities().Add<RaycastSensorComponent>(e, ray);
    return e;
}

Entity Prefab::CreateShapeCastNode(World& world, const Vec3& pos, f32 radius, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "ShapeCast", 0);
    ShapeCastComponent sc{};
    sc.cast_radius = radius;
    world.Entities().Add<ShapeCastComponent>(e, sc);
    return e;
}

Entity Prefab::CreatePhysicalMaterialNode(World& world, f32 friction, f32 restitution, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "PhysicalMaterial", 0);
    PhysicalMaterialComponent pm{};
    pm.dynamic_friction = friction;
    pm.restitution = restitution;
    world.Entities().Add<PhysicalMaterialComponent>(e, pm);
    return e;
}

// =========================================================================
// 7. Constraints, Soft Bodies & Destruction (6)
// =========================================================================

Entity Prefab::CreatePhysicsJointNode(World& world, JointType type, const Vec3& anchor, const std::string& name) {
    Entity e = MakeBaseEntity(world, anchor, name, "PhysicsJoint", 0);
    PhysicsJointComponent joint{};
    joint.joint_type = type;
    joint.anchor = anchor;
    world.Entities().Add<PhysicsJointComponent>(e, joint);
    return e;
}

Entity Prefab::CreateRopeNode(World& world, const Vec3& start, f32 length, const std::string& name) {
    Entity e = MakeBaseEntity(world, start, name, "Rope", 1);
    RopeComponent r{};
    r.total_length = length;
    world.Entities().Add<RopeComponent>(e, r);
    return e;
}

Entity Prefab::CreateClothNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Cloth", 1);
    world.Entities().Add<ClothComponent>(e, ClothComponent{});
    return e;
}

Entity Prefab::CreateSoftBodyNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "SoftBody", 1);
    world.Entities().Add<SoftBodyComponent>(e, SoftBodyComponent{});
    return e;
}

Entity Prefab::CreateBuoyancyNode(World& world, const Vec3& pos, f32 water_level, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "BuoyancyActor", 1);
    BuoyancyComponent buo{};
    buo.water_level = water_level;
    world.Entities().Add<BuoyancyComponent>(e, buo);
    return e;
}

Entity Prefab::CreateDestructibleMeshNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "DestructibleMesh", 1);
    world.Entities().Add<DestructibleMeshComponent>(e, DestructibleMeshComponent{});
    return e;
}

// =========================================================================
// 8. Vehicles (6)
// =========================================================================

Entity Prefab::CreateVehicleNode(World& world, const Vec3& pos, i32 material_index, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Vehicle", 2);
    Vehicle veh{};
    world.Entities().Add<Vehicle>(e, veh);

    const Vec3 box_half = Vec3(1.0f, 0.5f, 2.2f);
    RigidBody rb{};
    rb.type = BodyType::Dynamic;
    rb.shape = ShapeType::Box;
    rb.mass = 1500.0f;
    rb.friction = 0.8f;
    world.Entities().Add<RigidBody>(e, rb);

    PrimitiveShape ps{};
    ps.type = PrimitiveType::Box;
    ps.size = box_half;
    world.Entities().Add<PrimitiveShape>(e, ps);
    world.Entities().Add<MaterialRef>(e, MaterialRef{material_index});
    world.Entities().Add<LocalBounds>(e, LocalBounds{-box_half, box_half});
    return e;
}

Entity Prefab::CreateWheeledVehicleNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = CreateVehicleNode(world, pos, 0, name);
    world.Entities().Add<WheeledVehicleComponent>(e, WheeledVehicleComponent{});
    return e;
}

Entity Prefab::CreateVehicleWheelNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "VehicleWheel", 2);
    world.Entities().Add<VehicleWheelComponent>(e, VehicleWheelComponent{});
    return e;
}

Entity Prefab::CreateTrackedVehicleNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "TrackedVehicle", 2);
    world.Entities().Add<TrackedVehicleComponent>(e, TrackedVehicleComponent{});
    return e;
}

Entity Prefab::CreateAircraftNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Aircraft", 2);
    world.Entities().Add<AircraftComponent>(e, AircraftComponent{});
    return e;
}

Entity Prefab::CreateWatercraftNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Watercraft", 2);
    world.Entities().Add<WatercraftComponent>(e, WatercraftComponent{});
    return e;
}

// =========================================================================
// 9. Pawns & Controllers (7)
// =========================================================================

Entity Prefab::CreatePawnNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Player", 1);
    CameraComponent cam{};
    cam.fov = 70.0f;
    cam.is_primary = true;
    world.Entities().Add<CameraComponent>(e, cam);

    AudioListenerComponent listener{};
    listener.is_active = true;
    world.Entities().Add<AudioListenerComponent>(e, listener);

    PlayerControllerComponent pc{};
    world.Entities().Add<PlayerControllerComponent>(e, pc);

    PlayerInputComponent pi{};
    world.Entities().Add<PlayerInputComponent>(e, pi);

    HealthComponent hp{};
    world.Entities().Add<HealthComponent>(e, hp);

    InventoryComponent inv{};
    world.Entities().Add<InventoryComponent>(e, inv);
    return e;
}

Entity Prefab::CreateCharacterBodyNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Character", 1);
    CharacterBodyComponent cb{};
    world.Entities().Add<CharacterBodyComponent>(e, cb);

    CharacterMovementComponent cm{};
    world.Entities().Add<CharacterMovementComponent>(e, cm);

    HealthComponent hp{};
    world.Entities().Add<HealthComponent>(e, hp);

    HurtboxComponent hurt{};
    hurt.box_half_size = Vec3(cb.capsule_radius, cb.capsule_height * 0.5f, cb.capsule_radius);
    world.Entities().Add<HurtboxComponent>(e, hurt);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-hurt.box_half_size, hurt.box_half_size});
    return e;
}

Entity Prefab::CreateCharacterMovementNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Movement", 0);
    world.Entities().Add<CharacterMovementComponent>(e, CharacterMovementComponent{});
    return e;
}

Entity Prefab::CreatePlayerControllerNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Controller", 0);
    world.Entities().Add<PlayerControllerComponent>(e, PlayerControllerComponent{});
    return e;
}

Entity Prefab::CreateAIControllerNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Enemy", 1);
    AIControllerComponent ai{};
    world.Entities().Add<AIControllerComponent>(e, ai);

    BehaviorTreeComponent bt{};
    world.Entities().Add<BehaviorTreeComponent>(e, bt);

    PerceptionSensorComponent perc{};
    world.Entities().Add<PerceptionSensorComponent>(e, perc);

    HealthComponent hp{};
    hp.max_health = 150.0f;
    hp.current_health = 150.0f;
    world.Entities().Add<HealthComponent>(e, hp);

    HitboxComponent hit{};
    world.Entities().Add<HitboxComponent>(e, hit);
    return e;
}

Entity Prefab::CreatePlayerInputNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Input", 0);
    world.Entities().Add<PlayerInputComponent>(e, PlayerInputComponent{});
    return e;
}

Entity Prefab::CreateRagdollNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Ragdoll", 1);
    world.Entities().Add<RagdollComponent>(e, RagdollComponent{});
    return e;
}

// =========================================================================
// 10. Animation & Skeletal Hierarchy (7)
// =========================================================================

Entity Prefab::CreateSkeletonNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Skeleton", 0);
    return e;
}

Entity Prefab::CreateBoneNode(World& world, const std::string& bone_name, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Bone", 0);
    BoneNodeComponent bn{};
    bn.bone_name = bone_name;
    world.Entities().Add<BoneNodeComponent>(e, bn);
    return e;
}

Entity Prefab::CreateSocketNode(World& world, const std::string& joint_name, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Socket", 0);
    BoneAttachmentComponent ba{};
    ba.joint_name = joint_name;
    world.Entities().Add<BoneAttachmentComponent>(e, ba);
    return e;
}

Entity Prefab::CreateAnimationPlayerNode(World& world, const std::string& clip_name, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "AnimationPlayer", 0);
    AnimationTreeComponent at{};
    at.clip_a = clip_name;
    world.Entities().Add<AnimationTreeComponent>(e, at);
    return e;
}

Entity Prefab::CreateAnimationTreeBlendNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "AnimationTree", 0);
    world.Entities().Add<AnimationTreeComponent>(e, AnimationTreeComponent{});
    return e;
}

Entity Prefab::CreateIKSolverNode(World& world, const std::string& target_joint, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "IKSolver", 0);
    IKSolverComponent ik{};
    ik.target_joint = target_joint;
    world.Entities().Add<IKSolverComponent>(e, ik);
    return e;
}

Entity Prefab::CreateMorphTargetNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "MorphTarget", 0);
    world.Entities().Add<MorphTargetComponent>(e, MorphTargetComponent{});
    return e;
}

// =========================================================================
// 11. AI & Navigation (9)
// =========================================================================

Entity Prefab::CreateNavMeshBoundsNode(World& world, const Vec3& pos, const Vec3& size, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "NavMeshBounds", 0);
    NavMeshBoundsComponent nav{};
    nav.size = size;
    world.Entities().Add<NavMeshBoundsComponent>(e, nav);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-size * 0.5f, size * 0.5f});
    return e;
}

Entity Prefab::CreateNavMeshObstacleNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "NavMeshObstacle", 0);
    world.Entities().Add<NavMeshObstacleComponent>(e, NavMeshObstacleComponent{});
    return e;
}

Entity Prefab::CreateNavMeshLinkNode(World& world, const Vec3& start, const Vec3& end, const std::string& name) {
    Entity e = MakeBaseEntity(world, start, name, "NavMeshLink", 0);
    NavMeshLinkComponent link{};
    link.start_point = start;
    link.end_point = end;
    world.Entities().Add<NavMeshLinkComponent>(e, link);
    return e;
}

Entity Prefab::CreateNavigationAgentNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "NavAgent", 0);
    world.Entities().Add<NavigationAgentComponent>(e, NavigationAgentComponent{});
    return e;
}

Entity Prefab::CreateBehaviorTreeNode(World& world, const std::string& tree_name, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "BehaviorTree", 0);
    BehaviorTreeComponent bt{};
    bt.tree_name = tree_name;
    world.Entities().Add<BehaviorTreeComponent>(e, bt);
    return e;
}

Entity Prefab::CreateFSMNode(World& world, const std::string& initial_state, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "FSM", 0);
    FSMComponent fsm{};
    fsm.current_state = initial_state;
    world.Entities().Add<FSMComponent>(e, fsm);
    return e;
}

Entity Prefab::CreatePerceptionSensorNode(World& world, f32 sight_radius, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "PerceptionSensor", 0);
    PerceptionSensorComponent perc{};
    perc.sight_radius = sight_radius;
    world.Entities().Add<PerceptionSensorComponent>(e, perc);
    return e;
}

Entity Prefab::CreateBlackboardNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Blackboard", 0);
    world.Entities().Add<BlackboardComponent>(e, BlackboardComponent{});
    return e;
}

Entity Prefab::CreatePatrolPathNode(World& world, const std::vector<Vec3>& points, const std::string& name) {
    Vec3 start_pos = points.empty() ? Vec3(0.0f) : points[0];
    Entity e = MakeBaseEntity(world, start_pos, name, "PatrolPath", 0);
    SplinePathComponent sp{};
    sp.points = points;
    world.Entities().Add<SplinePathComponent>(e, sp);
    return e;
}

// =========================================================================
// 12. Visual Effects (VFX) (4)
// =========================================================================

Entity Prefab::CreateParticleEmitterNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "ParticleEmitter", 0);
    world.Entities().Add<ParticleEmitterComponent>(e, ParticleEmitterComponent{});
    return e;
}

Entity Prefab::CreateVFXGraphNode(World& world, const std::string& graph_path, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "VFXGraph", 0);
    VFXGraphComponent vfx{};
    vfx.graph_asset = graph_path;
    world.Entities().Add<VFXGraphComponent>(e, vfx);
    return e;
}

Entity Prefab::CreateTrailNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "TrailEffect", 0);
    world.Entities().Add<TrailComponent>(e, TrailComponent{});
    return e;
}

Entity Prefab::CreateBeamEmitterNode(World& world, const Vec3& start, const Vec3& target, const std::string& name) {
    Entity e = MakeBaseEntity(world, start, name, "BeamEmitter", 0);
    BeamEmitterComponent beam{};
    beam.target_point = target;
    world.Entities().Add<BeamEmitterComponent>(e, beam);
    return e;
}

// =========================================================================
// 13. Spatial Audio (5)
// =========================================================================

Entity Prefab::CreateAudioSourceNode(World& world, const std::string& sound_path, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "AudioSource", 0);
    AudioSourceComponent asc{};
    asc.sound_path = sound_path;
    world.Entities().Add<AudioSourceComponent>(e, asc);
    return e;
}

Entity Prefab::CreateSpatialAudioNode(World& world, const Vec3& pos, const std::string& name) {
    return CreateAudioSourceNode(world, "assets/audio/spatial_sfx.wav", pos, name);
}

Entity Prefab::CreateAudioListenerNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "AudioListener", 0);
    world.Entities().Add<AudioListenerComponent>(e, AudioListenerComponent{true});
    return e;
}

Entity Prefab::CreateAudioReverbZoneNode(World& world, const Vec3& pos, f32 radius, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "AudioReverbZone", 0);
    AudioReverbZoneComponent rev{};
    rev.radius = radius;
    world.Entities().Add<AudioReverbZoneComponent>(e, rev);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-radius), Vec3(radius)});
    return e;
}

Entity Prefab::CreateMusicTrackNode(World& world, const std::string& track_path, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "MusicTrack", 0);
    MusicTrackComponent mt{};
    mt.track_path = track_path;
    world.Entities().Add<MusicTrackComponent>(e, mt);
    return e;
}

// =========================================================================
// 14. Gameplay Systems & Stats (11)
// =========================================================================

Entity Prefab::CreateHealthNode(World& world, f32 max_hp, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Health", 0);
    HealthComponent hp{};
    hp.max_health = max_hp;
    hp.current_health = max_hp;
    world.Entities().Add<HealthComponent>(e, hp);
    return e;
}

Entity Prefab::CreateDamageReceiverNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "DamageReceiver", 0);
    world.Entities().Add<DamageReceiverComponent>(e, DamageReceiverComponent{});
    return e;
}

Entity Prefab::CreateHitboxNode(World& world, f32 damage, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Hitbox", 0);
    HitboxComponent hit{};
    hit.damage = damage;
    world.Entities().Add<HitboxComponent>(e, hit);
    return e;
}

Entity Prefab::CreateHurtboxNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Hurtbox", 0);
    world.Entities().Add<HurtboxComponent>(e, HurtboxComponent{});
    return e;
}

Entity Prefab::CreateInventoryNode(World& world, i32 max_slots, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Inventory", 0);
    InventoryComponent inv{};
    inv.max_slots = max_slots;
    world.Entities().Add<InventoryComponent>(e, inv);
    return e;
}

Entity Prefab::CreateEquipmentNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Equipment", 0);
    world.Entities().Add<EquipmentComponent>(e, EquipmentComponent{});
    return e;
}

Entity Prefab::CreateInteractableNode(World& world, const Vec3& pos, const std::string& prompt, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Interactable", 0);
    InteractableComponent inter{};
    inter.prompt_text = prompt;
    world.Entities().Add<InteractableComponent>(e, inter);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-inter.interaction_radius), Vec3(inter.interaction_radius)});
    return e;
}

Entity Prefab::CreateAbilityNode(World& world, const std::string& ability, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Ability", 0);
    AbilityComponent ab{};
    ab.ability_name = ability;
    world.Entities().Add<AbilityComponent>(e, ab);
    return e;
}

Entity Prefab::CreateQuestTriggerNode(World& world, const Vec3& pos, const std::string& quest_id, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "QuestTrigger", 0);
    QuestTriggerComponent q{};
    q.quest_id = quest_id;
    world.Entities().Add<QuestTriggerComponent>(e, q);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-2.0f), Vec3(2.0f)});
    return e;
}

Entity Prefab::CreateSavePointNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "SavePoint", 0);
    world.Entities().Add<SavePointComponent>(e, SavePointComponent{});
    return e;
}

Entity Prefab::CreateItemSpawnerNode(World& world, const Vec3& pos, const std::string& item, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "ItemSpawner", 0);
    SpawnerComponent spawner{};
    spawner.prefab_name = item;
    world.Entities().Add<SpawnerComponent>(e, spawner);
    return e;
}

// =========================================================================
// 15. Networking & Replication (5)
// =========================================================================

Entity Prefab::CreateNetworkIdentityNode(World& world, u32 net_id, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "NetIdentity", 0);
    NetworkIdentityComponent net{};
    net.net_id = net_id;
    world.Entities().Add<NetworkIdentityComponent>(e, net);
    return e;
}

Entity Prefab::CreateNetworkTransformNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "NetTransform", 0);
    world.Entities().Add<NetworkTransformComponent>(e, NetworkTransformComponent{});
    return e;
}

Entity Prefab::CreateNetworkAnimatorNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "NetAnimator", 0);
    world.Entities().Add<NetworkAnimatorComponent>(e, NetworkAnimatorComponent{});
    return e;
}

Entity Prefab::CreateReplicationManagerNode(World& world, u32 tick_rate, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "ReplicationMgr", 0);
    ReplicationManagerComponent rm{};
    rm.tick_rate_hz = tick_rate;
    world.Entities().Add<ReplicationManagerComponent>(e, rm);
    return e;
}

Entity Prefab::CreateRPCNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "RPCNode", 0);
    world.Entities().Add<RPCComponent>(e, RPCComponent{});
    return e;
}

// =========================================================================
// 16. User Interface (UI & HUD) (8)
// =========================================================================

Entity Prefab::CreateCanvasLayerNode(World& world, i32 sort_order, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "CanvasLayer", 0);
    CanvasComponent c{};
    c.sorting_order = sort_order;
    world.Entities().Add<CanvasComponent>(e, c);
    return e;
}

Entity Prefab::CreateUIPanelNode(World& world, const Vec2& size, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "UIPanel", 0);
    UIPanelComponent p{};
    p.size = size;
    world.Entities().Add<UIPanelComponent>(e, p);
    return e;
}

Entity Prefab::CreateUIContainerNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "UIContainer", 0);
    world.Entities().Add<UIContainerComponent>(e, UIContainerComponent{});
    return e;
}

Entity Prefab::CreateUIButtonNode(World& world, const std::string& label, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "UIButton", 0);
    UIButtonComponent b{};
    b.label = label;
    world.Entities().Add<UIButtonComponent>(e, b);
    return e;
}

Entity Prefab::CreateUILabelNode(World& world, const std::string& text, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "UILabel", 0);
    UILabelComponent l{};
    l.text = text;
    world.Entities().Add<UILabelComponent>(e, l);
    return e;
}

Entity Prefab::CreateUIImageNode(World& world, const std::string& tex, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "UIImage", 0);
    UIImageComponent img{};
    img.texture_path = tex;
    world.Entities().Add<UIImageComponent>(e, img);
    return e;
}

Entity Prefab::CreateWorldSpaceUINode(World& world, const Vec3& pos, const std::string& title, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "WorldSpaceUI", 0);
    WorldSpaceUIComponent ui{};
    ui.title = title;
    world.Entities().Add<WorldSpaceUIComponent>(e, ui);
    return e;
}

Entity Prefab::CreateMiniMapNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "MiniMap", 0);
    world.Entities().Add<MiniMapComponent>(e, MiniMapComponent{});
    return e;
}

// =========================================================================
// 17. Scene Management & Optimization (7)
// =========================================================================

Entity Prefab::CreateLODGroupNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "LODGroup", 0);
    world.Entities().Add<LODGroupComponent>(e, LODGroupComponent{});
    return e;
}

Entity Prefab::CreateHLODNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "HLOD", 0);
    world.Entities().Add<HLODComponent>(e, HLODComponent{});
    return e;
}

Entity Prefab::CreateOcclusionPortalNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "OcclusionPortal", 0);
    world.Entities().Add<OcclusionPortalComponent>(e, OcclusionPortalComponent{});
    return e;
}

Entity Prefab::CreateWorldPartitionCellNode(World& world, const Vec3& cell, const std::string& name) {
    Entity e = MakeBaseEntity(world, cell, name, "WorldPartitionCell", 0);
    WorldPartitionComponent wp{};
    wp.cell_index = cell;
    world.Entities().Add<WorldPartitionComponent>(e, wp);
    return e;
}

Entity Prefab::CreateDebugDrawNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "DebugDraw", 0);
    world.Entities().Add<DebugDrawComponent>(e, DebugDrawComponent{});
    return e;
}

Entity Prefab::CreateTimerNode(World& world, f32 duration, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Timer", 0);
    world.Entities().Add<Timer>(e, Timer(duration));
    return e;
}

Entity Prefab::CreateSignalBusNode(World& world, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "SignalBus", 0);
    world.Entities().Add<SignalBusComponent>(e, SignalBusComponent{});
    return e;
}

} // namespace lucida
