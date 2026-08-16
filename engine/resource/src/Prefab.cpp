// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/Prefab.h"
#include "lucida/audio/Components.h"
#include "lucida/resource/Terrain.h"
#include "lucida/runtime/World.h"

namespace lucida {

Entity Prefab::CreateTerrainNode(World& world, const TerrainComponent& config,
                                 i32 material_index, const std::string& name) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());

    LocalTransform lt{};
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);

    entities.Add<TerrainComponent>(e, config);

    PrimitiveShape ps{};
    ps.type = PrimitiveType::Plane;
    ps.normal = Vec3(0.0f, 1.0f, 0.0f);
    ps.offset = 0.0f;
    entities.Add<PrimitiveShape>(e, ps);

    entities.Add<MaterialRef>(e, MaterialRef{material_index});

    RigidBody rb{};
    rb.type = BodyType::Static;
    rb.shape = ShapeType::Plane;
    entities.Add<RigidBody>(e, rb);

    const f32 half_sz = config.size * 0.5f;
    entities.Add<LocalBounds>(e, LocalBounds{Vec3(-half_sz, -10.0f, -half_sz),
                                             Vec3( half_sz,  config.max_height,  half_sz)});
    entities.Add<TagComponent>(e, TagComponent{"Terrain", 0});

    return e;
}

Entity Prefab::CreateVehicleNode(World& world, const Vec3& pos,
                                 i32 material_index, const std::string& name) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());

    LocalTransform lt{};
    lt.position = pos;
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);

    Vehicle veh{};
    entities.Add<Vehicle>(e, veh);

    const Vec3 box_half = Vec3(1.0f, 0.5f, 2.2f);

    RigidBody rb{};
    rb.type = BodyType::Dynamic;
    rb.shape = ShapeType::Box;
    rb.mass = 1500.0f;
    rb.friction = 0.8f;
    entities.Add<RigidBody>(e, rb);

    PrimitiveShape ps{};
    ps.type = PrimitiveType::Box;
    ps.size = box_half;
    entities.Add<PrimitiveShape>(e, ps);

    entities.Add<MaterialRef>(e, MaterialRef{material_index});

    entities.Add<LocalBounds>(e, LocalBounds{-box_half, box_half});
    entities.Add<TagComponent>(e, TagComponent{"Vehicle", 2});

    return e;
}

Entity Prefab::CreatePhysicsActorNode(World& world, PrimitiveType shape,
                                      BodyType body_type, const Vec3& pos,
                                      i32 material_index, const std::string& name) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());

    LocalTransform lt{};
    lt.position = pos;
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);

    PrimitiveShape ps{};
    ps.type = shape;
    if (shape == PrimitiveType::Sphere) {
        ps.size = Vec3(0.5f);
    } else if (shape == PrimitiveType::Box) {
        ps.size = Vec3(0.5f);
    }
    entities.Add<PrimitiveShape>(e, ps);

    RigidBody rb{};
    rb.type = body_type;
    if (shape == PrimitiveType::Sphere) {
        rb.shape = ShapeType::Sphere;
    } else if (shape == PrimitiveType::Plane) {
        rb.shape = ShapeType::Plane;
        rb.type = BodyType::Static;
    } else {
        rb.shape = ShapeType::Box;
    }
    entities.Add<RigidBody>(e, rb);

    entities.Add<MaterialRef>(e, MaterialRef{material_index});

    const Vec3 half = ps.HalfExtents();
    entities.Add<LocalBounds>(e, LocalBounds{-half, half});
    entities.Add<TagComponent>(e, TagComponent{"PhysicsActor", 1});

    return e;
}

Entity Prefab::CreatePawnNode(World& world, const Vec3& pos, const std::string& name) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());

    LocalTransform lt{};
    lt.position = pos;
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);

    CameraComponent cam{};
    cam.fov = 70.0f;
    cam.is_primary = true;
    entities.Add<CameraComponent>(e, cam);

    AudioListenerComponent listener{};
    listener.is_active = true;
    listener.master_volume = 1.0f;
    entities.Add<AudioListenerComponent>(e, listener);

    entities.Add<TagComponent>(e, TagComponent{"Player", 1});

    return e;
}

Entity Prefab::CreateStaticMeshNode(World& world, MeshHandle mesh,
                                    const Vec3& pos, const std::string& name) {
    Registry& entities = world.Entities();
    Entity e = entities.Create(name.c_str());

    LocalTransform lt{};
    lt.position = pos;
    entities.Add<LocalTransform>(e, lt);
    entities.Add<WorldTransform>(e);

    MeshInstance mi{};
    mi.mesh = mesh;
    entities.Add<MeshInstance>(e, mi);

    entities.Add<LocalBounds>(e, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
    entities.Add<TagComponent>(e, TagComponent{"StaticMesh", 0});

    return e;
}

} // namespace lucida
