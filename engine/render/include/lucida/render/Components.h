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
// WorldTransform, so moving the entity moves the instance — nothing else has to
// remember to keep the two in step.
struct MeshInstance {
    MeshHandle     mesh;
    InstanceHandle instance;
};

// Marks the entity the viewport camera follows. Zero or one per world.
struct CameraTag {};

} // namespace lucida
