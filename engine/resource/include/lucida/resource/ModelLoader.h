// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Resource loading (GEA 6.2): file on disk -> MeshData the GPU can take.
// Assimp handles the formats, bvh v2 builds the acceleration structure; this
// module does the packing, the texture arrays and the SoA split.

#include "lucida/render/MeshData.h"

#include <string>

namespace lucida {

// Loads model from disk. Centres horizontally and seats on y=0.
// If target_size > 0 (default 2.0f), scales the mesh to target_size.
// If target_size <= 0, preserves 1:1 scale while centering.
MeshData LoadModel(const std::string& path, float target_size = 2.0f);

void BuildBVH(std::vector<GPUTriangle>& tris,
              std::vector<GPUBVHNode>&  nodes,
              int start, int count, int depth = 0);

} // namespace lucida
