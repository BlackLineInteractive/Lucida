// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/SceneAssets.h"
#include "lucida/physics/Components.h"

#include <functional>

namespace lucida {
namespace {

// Cheap, order-dependent mixing. Not a cryptographic hash and does not need to
// be: it only has to change when the scene changes.
inline void Mix(u64& seed, u64 value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

inline void MixFloat(u64& seed, f32 value) {
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Mix(seed, bits);
}

inline void MixVec3(u64& seed, const Vec3& v) {
    MixFloat(seed, v.x); MixFloat(seed, v.y); MixFloat(seed, v.z);
}

} // namespace

i32 SceneAssets::AddMaterial(const Material& m, i32 procedural, const std::string& name) {
    GPUMaterial gm{};
    SetVec3(gm.albedo, m.albedo);
    SetVec3(gm.emission, m.emission);
    SetVec3(gm.albedo2, m.albedo2);
    gm.roughness        = f32(m.roughness);
    gm.metallic         = f32(m.metallic);
    gm.refractive_index = f32(m.refractive_index);
    gm.type             = i32(m.type);
    gm.proc_id          = procedural;

    materials.push_back(gm);
    material_names.push_back(name.empty() ? "material_" + std::to_string(materials.size() - 1)
                                          : name);
    return i32(materials.size()) - 1;
}

i32 SceneAssets::FindMaterial(const std::string& name) const {
    for (usize i = 0; i < material_names.size(); ++i) {
        if (material_names[i] == name) return i32(i);
    }
    return -1;
}

const char* PrimitiveTypeName(PrimitiveType type) {
    switch (type) {
    case PrimitiveType::Sphere: return "Sphere";
    case PrimitiveType::Box:    return "Box";
    case PrimitiveType::Plane:  return "Plane";
    case PrimitiveType::Cylinder: return "Cylinder";
    case PrimitiveType::Cone:   return "Cone";
    case PrimitiveType::Torus:  return "Torus";
    case PrimitiveType::Disk:   return "Disk";
    default:                    return "?";
    }
}

void PublishScene(Registry& registry, const SceneAssets& assets, RenderScene& out) {
    out.Clear();
    out.name           = assets.name;
    out.materials      = assets.materials;
    out.material_names = assets.material_names;
    out.environment    = assets.environment;
    out.model          = assets.model;
    out.spawn          = assets.spawn;

    for (auto [entity, shape, material, world, visibility] :
         registry.View<PrimitiveShape, MaterialRef, WorldTransform, Visibility>().each()) {
        if (!visibility.visible) continue;

        // World transform, not local: a primitive parented to something else has
        // to follow it, and the hierarchy has already been resolved this frame.
        const Vec3 position(world.matrix[3]);
        const Vec3 scale_3d(glm::length(Vec3(world.matrix[0])),
                            glm::length(Vec3(world.matrix[1])),
                            glm::length(Vec3(world.matrix[2])));
        const f32  scale = scale_3d.x;

        switch (shape.type) {
        case PrimitiveType::Sphere:
            out.AddSphere(position, shape.size.x * scale, material.index);
            break;
        case PrimitiveType::Box:
            out.AddCube(position, shape.size * scale_3d, material.index);
            break;
        case PrimitiveType::Cylinder:
            out.AddCylinder(position, shape.size.x * scale, shape.cylinder_height * scale_3d.y, material.index);
            break;
        case PrimitiveType::Cone:
            out.AddCone(position, shape.size.x * scale, shape.cylinder_height * scale_3d.y, material.index);
            break;
        case PrimitiveType::Torus:
            out.AddTorus(position, shape.size.x * scale, shape.inner_radius * scale, material.index);
            break;
        case PrimitiveType::Disk: {
            const Vec3 normal = glm::normalize(Vec3(world.matrix * Vec4(shape.normal, 0.0f)));
            out.AddDisk(position, shape.size.x * scale, normal, material.index);
            break;
        }
        case PrimitiveType::Plane: {
            // The plane's normal follows the entity's rotation; its offset is
            // the distance from the origin along that normal.
            const Vec3 normal = glm::normalize(Vec3(world.matrix * Vec4(shape.normal, 0.0f)));
            out.AddPlane(normal, glm::dot(normal, position), material.index);
            break;
        }
        default:
            break;
        }
    }

    for (auto [entity, light, world, visibility] :
         registry.View<LightSource, WorldTransform, Visibility>().each()) {
        if (!visibility.visible) continue;
        out.AddLight(Vec3(world.matrix[3]), light.intensity, light.color, light.radius);
    }
}

u64 SceneFingerprint(Registry& registry, const SceneAssets& assets) {
    u64 seed = 0xcbf29ce484222325ull;

    Mix(seed, assets.materials.size());
    for (const GPUMaterial& m : assets.materials) {
        MixFloat(seed, m.albedo[0]); MixFloat(seed, m.albedo[1]); MixFloat(seed, m.albedo[2]);
        MixFloat(seed, m.roughness); MixFloat(seed, m.metallic);
        MixFloat(seed, m.refractive_index);
        Mix(seed, u64(m.type)); Mix(seed, u64(m.proc_id));
        MixFloat(seed, m.emission[0]); MixFloat(seed, m.emission[1]); MixFloat(seed, m.emission[2]);
    }

    MixVec3(seed, assets.environment.ambient);
    MixFloat(seed, assets.environment.fog_density);
    Mix(seed, u64(assets.environment.fog_enabled));
    Mix(seed, u64(assets.environment.fog_steps));
    Mix(seed, u64(assets.model));

    for (auto [entity, shape, material, world, visibility] :
         registry.View<PrimitiveShape, MaterialRef, WorldTransform, Visibility>().each()) {
        Mix(seed, u64(entt::to_integral(entity)));
        Mix(seed, u64(shape.type));
        MixVec3(seed, shape.size);
        MixVec3(seed, shape.normal);
        MixFloat(seed, shape.offset);
        MixFloat(seed, shape.cylinder_height);
        MixFloat(seed, shape.inner_radius);
        Mix(seed, u64(material.index));
        Mix(seed, u64(visibility.visible));
        for (int c = 0; c < 4; ++c) MixVec3(seed, Vec3(world.matrix[c]));
    }

    for (auto [entity, light, world, visibility] :
         registry.View<LightSource, WorldTransform, Visibility>().each()) {
        Mix(seed, u64(entt::to_integral(entity)));
        MixVec3(seed, light.color);
        MixFloat(seed, light.intensity);
        MixFloat(seed, light.radius);
        Mix(seed, u64(visibility.visible));
        MixVec3(seed, Vec3(world.matrix[3]));
    }

    return seed;
}

Entity CreatePrimitive(Registry& registry, PrimitiveType type, const Vec3& position,
                       i32 material, const std::string& name) {
    const Entity entity = registry.Create(name.empty() ? PrimitiveTypeName(type) : name);
    registry.Get<LocalTransform>(entity)->position = position;

    PrimitiveShape shape;
    shape.type = type;
    if (type == PrimitiveType::Sphere) shape.size = Vec3(1.0f);
    registry.Add<PrimitiveShape>(entity, shape);
    registry.Add<MaterialRef>(entity, MaterialRef{material});
    registry.Raw().emplace_or_replace<SceneGraphNode>(entity);

    // Bounds are what makes it clickable, so they are set here rather than left
    // to whoever remembers.
    const Vec3 half = shape.HalfExtents();
    registry.Add<LocalBounds>(entity, LocalBounds{-half, half});
    return entity;
}

Entity CreateLight(Registry& registry, LightType type, const Vec3& position, const Vec3& color,
                   f32 intensity, f32 radius, const Vec3& direction, const std::string& name) {
    std::string default_name = "Light";
    switch (type) {
    case LightType::Point:       default_name = "Point Light"; break;
    case LightType::Directional: default_name = "Directional Light"; break;
    case LightType::Spot:        default_name = "Spot Light"; break;
    case LightType::Area:        default_name = "Area Light"; break;
    }

    const Entity entity = registry.Create(name.empty() ? default_name : name);
    registry.Get<LocalTransform>(entity)->position = position;
    LightSource ls{};
    ls.type = type;
    ls.color = color;
    ls.intensity = intensity;
    ls.radius = radius;
    ls.direction = direction;
    registry.Add<LightSource>(entity, ls);
    registry.Raw().emplace_or_replace<SceneGraphNode>(entity);
    registry.Add<LocalBounds>(entity, LocalBounds{Vec3(-0.25f), Vec3(0.25f)});
    return entity;
}

const char* LightTypeName(LightType type) {
    switch (type) {
    case LightType::Point:       return "Point Light";
    case LightType::Directional: return "Directional Light";
    case LightType::Spot:        return "Spot Light";
    case LightType::Area:        return "Area Light";
    default:                     return "Light";
    }
}

Entity CreateTerrain(Registry& registry, SceneAssets& assets, const TerrainComponent& config,
                     i32 material, const std::string& name) {
    (void)assets;
    const Entity entity = registry.Create(name.empty() ? "Terrain" : name);
    registry.Get<LocalTransform>(entity)->position = Vec3(0.0f);
    registry.Add<TerrainComponent>(entity, config);
    registry.Add<MaterialRef>(entity, MaterialRef{material});
    registry.Add<Visibility>(entity, Visibility{true});
    registry.Raw().emplace_or_replace<SceneGraphNode>(entity);

    const f32 half = config.size * 0.5f;
    registry.Add<LocalBounds>(entity, LocalBounds{
        Vec3(-half, -config.max_height, -half),
        Vec3( half,  config.max_height,  half)
    });

    RigidBody rb{};
    rb.type = BodyType::Static;
    rb.shape = ShapeType::Plane;
    registry.Add<RigidBody>(entity, rb);

    return entity;
}

Entity CreateCamera(Registry& registry, const Vec3& position, f32 fov,
                    ProjectionType proj, const std::string& name) {
    const Entity entity = registry.Create(name.empty() ? (proj == ProjectionType::Perspective ? "Main Camera" : "Ortho Camera") : name);
    registry.Get<LocalTransform>(entity)->position = position;
    CameraComponent cam{};
    cam.fov = fov;
    cam.projection = proj;
    cam.is_primary = true;
    registry.Add<CameraComponent>(entity, cam);
    registry.Add<Visibility>(entity, Visibility{true});
    registry.Raw().emplace_or_replace<SceneGraphNode>(entity);
    registry.Add<LocalBounds>(entity, LocalBounds{Vec3(-0.3f), Vec3(0.3f)});
    return entity;
}

} // namespace lucida
