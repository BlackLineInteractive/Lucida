// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/Picking.h"

#include "lucida/render/Components.h"

namespace lucida {
namespace {

// Slab test in the box's own space. Returns the entry distance, or a miss.
bool IntersectAABB(const Ray& ray, const Vec3& box_min, const Vec3& box_max, f32& out_t) {
    // Division by a zero component gives an infinity, and the min/max below
    // resolve it correctly - no need to special-case an axis-aligned ray.
    const Vec3 inv = 1.0f / ray.direction;
    const Vec3 t0 = (box_min - ray.origin) * inv;
    const Vec3 t1 = (box_max - ray.origin) * inv;

    const Vec3 near_t = glm::min(t0, t1);
    const Vec3 far_t  = glm::max(t0, t1);

    const f32 enter = Max(Max(near_t.x, near_t.y), near_t.z);
    const f32 exit  = Min(Min(far_t.x, far_t.y), far_t.z);

    if (exit < 0.0f || enter > exit) return false;

    // Inside the box counts as a hit at zero distance rather than a miss.
    out_t = enter > 0.0f ? enter : 0.0f;
    return true;
}

} // namespace

Ray RayThroughViewport(const CameraState& camera, f32 aspect, Vec2 ndc) {
    // Matches the ray generation in the tracing kernel: the same basis, the same
    // half-angle. If these ever disagree, a click lands somewhere other than
    // where the user pointed.
    const f32 tan_half = std::tan(camera.fov_y * 0.5f);

    Ray ray;
    ray.origin = camera.position;
    ray.direction = glm::normalize(camera.Forward() +
                                   camera.Right() * (ndc.x * aspect * tan_half) +
                                   camera.Up() * (ndc.y * tan_half));
    return ray;
}

PickResult PickEntity(Registry& registry, const Ray& ray) {
    PickResult best;

    for (auto [entity, bounds, world, visibility] :
         registry.View<LocalBounds, WorldTransform, Visibility>().each()) {
        if (!visibility.visible) continue;

        // Take the ray into the entity's space rather than transforming eight
        // corners into the world: one inverse and two transforms beats rebuilding
        // a box that would also be looser than the original after rotation.
        const Mat4 to_local = glm::inverse(world.matrix);

        Ray local;
        local.origin    = Vec3(to_local * Vec4(ray.origin, 1.0f));
        local.direction = Vec3(to_local * Vec4(ray.direction, 0.0f));

        const f32 length = glm::length(local.direction);
        if (length < kEpsilon) continue;
        local.direction /= length;

        f32 t = 0.0f;
        if (!IntersectAABB(local, bounds.min, bounds.max, t)) continue;

        // Back to world distance: the local ray was normalised after a transform
        // that may have scaled it.
        const f32 world_t = t / length;
        if (world_t < best.distance) {
            best.entity = entity;
            best.distance = world_t;
        }
    }

    return best;
}

} // namespace lucida
