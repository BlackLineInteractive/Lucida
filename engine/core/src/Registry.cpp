// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/core/ecs/Registry.h"

#include "lucida/core/diag/Log.h"

namespace lucida {
namespace {

// A hierarchy deeper than this is a cycle in practice. Bounded so a malformed
// scene reports itself instead of hanging the frame.
constexpr u32 kMaxHierarchyDepth = 64;

} // namespace

Mat4 ComputeWorldTransform(Registry& registry, Entity entity) {
    const LocalTransform* local = registry.Get<LocalTransform>(entity);
    if (!local) return Mat4(1.0f);

    Mat4 result = local->ToMatrix();

    Entity current = entity;
    for (u32 depth = 0; depth < kMaxHierarchyDepth; ++depth) {
        const Parent* parent = registry.Get<Parent>(current);
        if (!parent || parent->entity == kNullEntity) return result;
        if (!registry.Valid(parent->entity)) {
            // The parent was destroyed without reparenting its children.
            LUCIDA_WARN(Core, "entity has a dangling parent; treating it as a root");
            return result;
        }
        const LocalTransform* parent_local = registry.Get<LocalTransform>(parent->entity);
        if (parent_local) result = parent_local->ToMatrix() * result;
        current = parent->entity;
    }

    LUCIDA_ERROR(Core, "hierarchy deeper than %u levels, or a parent cycle",
                 kMaxHierarchyDepth);
    return result;
}

void UpdateWorldTransforms(Registry& registry) {
    for (auto [entity, world] : registry.View<WorldTransform>().each()) {
        world.matrix = ComputeWorldTransform(registry, entity);
    }
}

} // namespace lucida
