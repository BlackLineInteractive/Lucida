// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// The scene, as entities.
//
// This is the change that turns a renderer into an editor: the world is entities
// with components, and `RenderScene` becomes a *view* of them rebuilt for the
// backend rather than the place the world lives. Before it, primitives were raw
// arrays inside a scene struct - nothing to select, nothing to inspect, nothing
// to move.
//
// What stays outside the ECS is the material palette and the environment: they
// are scene-wide, not per entity, and giving every sphere its own copy of the
// sky would be a strange way to store one gradient.

#include "lucida/core/ecs/Registry.h"
#include "lucida/render/Components.h"
#include "lucida/render/Scene.h"

#include <string>
#include <vector>

namespace lucida {

struct SceneAssets {
    std::string name = "untitled";

    std::vector<GPUMaterial> materials;
    std::vector<std::string> material_names;

    SceneEnvironment environment;
    ShadingModel     model = ShadingModel::WhittedGI;
    CameraState      spawn;

    i32 AddMaterial(const Material& m, i32 procedural = PROC_NONE,
                    const std::string& name = {});
    i32 FindMaterial(const std::string& name) const;
};

// Rebuilds a RenderScene from the entities that describe it. Cheap enough to do
// whenever something changed: a scene is tens of primitives, not millions.
void PublishScene(Registry& registry, const SceneAssets& assets, RenderScene& out);

// A fingerprint of everything PublishScene reads. Comparing it each frame is far
// cheaper than rebuilding and re-uploading buffers, and it cannot miss an edit
// the way a manually maintained dirty flag can.
u64 SceneFingerprint(Registry& registry, const SceneAssets& assets);

// --- authoring -------------------------------------------------------------

Entity CreatePrimitive(Registry& registry, PrimitiveType type, const Vec3& position,
                       i32 material, const std::string& name = {});
Entity CreateLight(Registry& registry, LightType type, const Vec3& position, const Vec3& color,
                   f32 intensity, f32 radius, const Vec3& direction = Vec3(0.0f, -1.0f, 0.0f),
                   const std::string& name = {});
inline Entity CreateLight(Registry& registry, const Vec3& position, const Vec3& color,
                          f32 intensity, f32 radius, const std::string& name = {}) {
    return CreateLight(registry, LightType::Point, position, color, intensity, radius, Vec3(0.0f, -1.0f, 0.0f), name);
}

Entity CreateTerrain(Registry& registry, SceneAssets& assets, const TerrainComponent& config,
                     i32 material, const std::string& name = {});

const char* PrimitiveTypeName(PrimitiveType type);
const char* LightTypeName(LightType type);

} // namespace lucida
