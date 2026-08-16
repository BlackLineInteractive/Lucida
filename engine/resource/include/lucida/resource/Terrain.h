// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/render/Components.h"
#include "lucida/render/MeshData.h"

namespace lucida {

// Samples procedural height for coordinate (x, z)
f32 SampleTerrainHeight(f32 x, f32 z, const TerrainComponent& config);

// Builds complete GPU mesh data with BVH for terrain
MeshData GenerateTerrainMeshData(const TerrainComponent& config, i32 material_index);

} // namespace lucida
