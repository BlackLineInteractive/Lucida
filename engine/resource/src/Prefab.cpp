// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/Prefab.h"
#include "lucida/audio/Components.h"
#include "lucida/resource/Terrain.h"
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

Entity Prefab::CreateSpatialNode(World& world, const Vec3& pos, const std::string& name) {
    return MakeBaseEntity(world, pos, name, "Spatial", 0);
}

Entity Prefab::CreateStaticMeshNode(World& world, MeshHandle mesh, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "StaticMesh", 0);
    MeshInstance mi{};
    mi.mesh = mesh;
    world.Entities().Add<MeshInstance>(e, mi);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
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

    HealthComponent hp{};
    world.Entities().Add<HealthComponent>(e, hp);

    InventoryComponent inv{};
    world.Entities().Add<InventoryComponent>(e, inv);

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

Entity Prefab::CreateFoliageNode(World& world, const std::string& mesh_path, i32 count, const std::string& name) {
    Entity e = MakeBaseEntity(world, Vec3(0.0f), name, "Foliage", 0);
    FoliageInstancerComponent fol{};
    fol.mesh_path = mesh_path;
    fol.instance_count = count;
    world.Entities().Add<FoliageInstancerComponent>(e, fol);
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

Entity Prefab::CreatePhysicsJointNode(World& world, JointType type, const Vec3& anchor, const std::string& name) {
    Entity e = MakeBaseEntity(world, anchor, name, "PhysicsJoint", 0);
    PhysicsJointComponent joint{};
    joint.joint_type = type;
    joint.anchor = anchor;
    world.Entities().Add<PhysicsJointComponent>(e, joint);
    return e;
}

Entity Prefab::CreateBuoyancyNode(World& world, const Vec3& pos, f32 water_level, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "BuoyancyActor", 1);
    BuoyancyComponent buo{};
    buo.water_level = water_level;
    world.Entities().Add<BuoyancyComponent>(e, buo);
    return e;
}

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

Entity Prefab::CreateAIEnemyNode(World& world, const Vec3& pos, const std::string& name) {
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

Entity Prefab::CreateNavMeshBoundsNode(World& world, const Vec3& pos, const Vec3& size, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "NavMeshBounds", 0);
    NavMeshBoundsComponent nav{};
    nav.size = size;
    world.Entities().Add<NavMeshBoundsComponent>(e, nav);
    world.Entities().Add<LocalBounds>(e, LocalBounds{-size * 0.5f, size * 0.5f});
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

Entity Prefab::CreateTrailNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "TrailEffect", 0);
    TrailComponent tr{};
    world.Entities().Add<TrailComponent>(e, tr);
    return e;
}

Entity Prefab::CreateBeamEmitterNode(World& world, const Vec3& start, const Vec3& target, const std::string& name) {
    Entity e = MakeBaseEntity(world, start, name, "BeamEmitter", 0);
    BeamEmitterComponent beam{};
    beam.target_point = target;
    world.Entities().Add<BeamEmitterComponent>(e, beam);
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

Entity Prefab::CreateInteractableNode(World& world, const Vec3& pos, const std::string& prompt, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "Interactable", 0);
    InteractableComponent inter{};
    inter.prompt_text = prompt;
    world.Entities().Add<InteractableComponent>(e, inter);
    world.Entities().Add<LocalBounds>(e, LocalBounds{Vec3(-inter.interaction_radius), Vec3(inter.interaction_radius)});
    return e;
}

Entity Prefab::CreateItemSpawnerNode(World& world, const Vec3& pos, const std::string& item, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "ItemSpawner", 0);
    SpawnerComponent spawner{};
    spawner.prefab_name = item;
    world.Entities().Add<SpawnerComponent>(e, spawner);
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

Entity Prefab::CreateWorldSpaceUINode(World& world, const Vec3& pos, const std::string& title, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "WorldSpaceUI", 0);
    WorldSpaceUIComponent ui{};
    ui.title = title;
    world.Entities().Add<WorldSpaceUIComponent>(e, ui);
    return e;
}

Entity Prefab::CreateLODGroupNode(World& world, const Vec3& pos, const std::string& name) {
    Entity e = MakeBaseEntity(world, pos, name, "LODGroup", 0);
    LODGroupComponent lod{};
    world.Entities().Add<LODGroupComponent>(e, lod);
    return e;
}

} // namespace lucida
