// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Render-side components.
//
// They hold handles, not data: the mesh lives in the backend's BLAS pool and
// the instance lives in its TLAS. An entity only says which of them it is.

#include "lucida/render/RenderBackend.h"

namespace lucida {

// An entity drawn as a mesh instance. The transform comes from the entity's
// WorldTransform, so moving the entity moves the instance - nothing else has to
// remember to keep the two in step.
struct MeshInstance {
    MeshHandle     mesh;
    InstanceHandle instance;
};

// The mesh's bounding box in the entity's own space. Kept on the entity rather
// than looked up from the backend so picking, culling and framing a selection
// are all answerable without asking the renderer anything.
struct LocalBounds {
    Vec3 min{-0.5f};
    Vec3 max{ 0.5f};
};

// An analytic primitive: geometry described by a handful of numbers rather than
// by triangles. This is where the engine's cheapness comes from - a sphere is
// twelve floats and an exact intersection, not a mesh to build a BVH over.
enum class PrimitiveType : u8 { Sphere = 0, Box, Plane, Cylinder, Cone, Torus, Disk, Count };

struct PrimitiveShape {
    PrimitiveType type = PrimitiveType::Sphere;

    // Sphere: radius in x. Box: half extents. Plane: normal, with `offset` as
    // the distance along it. One struct rather than three because the editor
    // wants to switch a primitive's type without destroying the entity.
    Vec3 size{0.5f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    f32  offset = 0.0f;
    f32  cylinder_height = 1.0f;
    f32  inner_radius = 0.25f; // For torus

    Vec3 HalfExtents() const {
        switch (type) {
        case PrimitiveType::Sphere: return Vec3(size.x);
        case PrimitiveType::Box:    return size;
        case PrimitiveType::Cylinder: return Vec3(size.x, cylinder_height * 0.5f, size.x);
        case PrimitiveType::Cone:   return Vec3(size.x, cylinder_height * 0.5f, size.x);
        case PrimitiveType::Torus:  return Vec3(size.x + inner_radius, inner_radius, size.x + inner_radius);
        case PrimitiveType::Disk:   return Vec3(size.x, 0.02f, size.x);
        // A plane is unbounded; give picking something finite to hit that still
        // reads as "the ground" rather than a wall in front of the camera.
        case PrimitiveType::Plane:  return Vec3(50.0f, 0.02f, 50.0f);
        default:                    return Vec3(0.5f);
        }
    }
};

// Which material in the scene palette this entity uses.
struct MaterialRef {
    i32 index = 0;
};

enum class LightType : u8 {
    Point = 0,
    Directional,
    Spot,
    Area
};

// A light, as an entity, so it can be selected and moved like anything else.
struct LightSource {
    LightType type = LightType::Point;
    Vec3 color{1.0f, 0.95f, 0.9f};
    f32  intensity = 50.0f;
    f32  radius = 1.0f;
    Vec3 direction{0.0f, -1.0f, 0.0f};
    f32  inner_angle = 25.0f;
    f32  outer_angle = 45.0f;
    bool cast_shadows = true;
};

// Procedural Terrain configuration component
struct TerrainComponent {
    i32 resolution   = 64;       // Grid vertices per side (64x64)
    f32 size         = 60.0f;    // World X/Z size in meters
    f32 max_height   = 8.0f;     // Maximum peak elevation
    f32 frequency    = 0.04f;    // Base noise frequency
    i32 octaves      = 4;        // Fractal octaves
    f32 persistence  = 0.5f;     // Roughness multiplier per octave
    f32 lacunarity   = 2.0f;     // Frequency multiplier per octave
    u32 seed         = 1337;     // Random seed
    bool dirty       = false;    // Marks mesh needs regeneration
};

// Marks the entity the viewport camera follows. Zero or one per world.
struct CameraTag {};

// Tag component to indicate this entity should appear in the Scene Graph hierarchy UI.
// Parent/child relations themselves are handled by core's `Parent` component.
struct SceneGraphNode {};

} // namespace lucida
