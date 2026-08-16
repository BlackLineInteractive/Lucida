// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Engine Node Archetypes & Prefabs for gameplay creation (GEA ch.14 Gameplay Foundations).

#include "lucida/core/ecs/Registry.h"
#include "lucida/core/math/Math.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/render/GpuTypes.h"

#include <string>

namespace lucida {

class World;

class Prefab {
public:
    // 1. Procedural Terrain Node
    static Entity CreateTerrainNode(World& world,
                                    const TerrainComponent& config = TerrainComponent{},
                                    i32 material_index = 0,
                                    const std::string& name = "Terrain");

    // 2. Interactive Vehicle Actor Node
    static Entity CreateVehicleNode(World& world,
                                    const Vec3& pos = Vec3(0.0f, 1.0f, 0.0f),
                                    i32 material_index = 0,
                                    const std::string& name = "Vehicle");

    // 3. Physics RigidBody Actor Node
    static Entity CreatePhysicsActorNode(World& world,
                                         PrimitiveType shape = PrimitiveType::Box,
                                         BodyType body_type = BodyType::Dynamic,
                                         const Vec3& pos = Vec3(0.0f, 2.0f, 0.0f),
                                         i32 material_index = 0,
                                         const std::string& name = "PhysicsActor");

    // 4. Controllable Player Pawn Node (Camera + Listener + Physics/Collider)
    static Entity CreatePawnNode(World& world,
                                 const Vec3& pos = Vec3(0.0f, 1.8f, 5.0f),
                                 const std::string& name = "PlayerPawn");

    // 5. Static Mesh Scene Node
    static Entity CreateStaticMeshNode(World& world,
                                       MeshHandle mesh, const Vec3& pos = Vec3(0.0f),
                                       const std::string& name = "StaticMesh");
};

} // namespace lucida
