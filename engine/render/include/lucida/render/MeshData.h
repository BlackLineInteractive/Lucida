// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Mesh as the GPU wants it: already split, already packed, already accelerated.
// Produced by lucida::resource, consumed by a render backend.

#include "lucida/render/GpuTypes.h"

#include <cstdint>
#include <vector>

namespace lucida {

// Upper bound on a slice of the base-colour array. The real size follows the
// largest image in the file, rounded to a power of two, so a 512-texel model
// does not get blown up to 2048.
inline constexpr int kMeshTexSizeMax = 2048;
// Roughness and metallic are low frequency; half the side, a quarter the VRAM.
inline constexpr int kMeshOrmSizeMax = 1024;

struct MeshData {
    std::vector<GPUTriangle> triangles;   // build-time only, freed after the split
    std::vector<GPUTriPos>   tri_pos;
    std::vector<GPUTriAttr>  tri_attr;
    std::vector<GPUMaterial> materials;
    std::vector<GPUBVHNode>  bvh_nodes;

    std::vector<uint8_t> texture_array_data;  // base colour, sRGB
    int tex_size = 0;
    std::vector<uint8_t> orm_array_data;      // occlusion/roughness/metallic, linear
    int orm_size = 0;

    Vec3 origin = Vec3(0.0f);
    bool valid  = false;

    // Bounds after centring and scaling, used to seat the model on the floor.
    Vec3 aabb_min = Vec3(0.0f);
    Vec3 aabb_max = Vec3(0.0f);
};

} // namespace lucida
