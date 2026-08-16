// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Clicking in the viewport to select something.
//
// Deliberately above the backend and on the CPU: picking is a question about
// entities, not about pixels, so it works the same whichever renderer is
// running and needs no readback, no pick buffer and no extra GPU pass.
//
// It tests bounding boxes, not triangles. For selecting an object that is the
// right answer - a click near a chair's silhouette means the chair - and it
// costs a slab test per entity instead of a BVH traversal.

#include "lucida/core/ecs/Registry.h"
#include "lucida/render/Camera.h"

namespace lucida {

struct PickResult {
    Entity entity = kNullEntity;
    f32    distance = kInfinity;

    bool Hit() const { return entity != kNullEntity; }
};

// ndc is in [-1, 1] with y up, as the viewport reports it.
Ray RayThroughViewport(const CameraState& camera, f32 aspect, Vec2 ndc);

// Nearest entity whose LocalBounds the ray enters. Invisible entities are
// skipped: if you cannot see it, clicking where it would be should not select it.
PickResult PickEntity(Registry& registry, const Ray& ray);

} // namespace lucida
