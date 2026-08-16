// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/Terrain.h"

#include "lucida/core/diag/Log.h"
#include "lucida/resource/ModelLoader.h"

#include <cmath>
#include <algorithm>
#include <vector>

namespace lucida {
namespace {

// 2D Hash for pseudo-random gradient noise
inline f32 Hash2D(i32 x, i32 z, u32 seed) {
    u32 n = static_cast<u32>(x * 374761393 + z * 668265263) ^ seed;
    n = (n ^ (n >> 13)) * 1274126177;
    return static_cast<f32>(n & 0x7fffffff) / static_cast<f32>(0x7fffffff);
}

// Smooth cubic Hermite interpolation
inline f32 Smooth(f32 t) {
    return t * t * (3.0f - 2.0f * t);
}

// 2D Value / Gradient noise
f32 Noise2D(f32 x, f32 z, u32 seed) {
    i32 ix = static_cast<i32>(std::floor(x));
    i32 iz = static_cast<i32>(std::floor(z));
    f32 fx = x - static_cast<f32>(ix);
    f32 fz = z - static_cast<f32>(iz);

    f32 sfx = Smooth(fx);
    f32 sfz = Smooth(fz);

    f32 h00 = Hash2D(ix,     iz,     seed);
    f32 h10 = Hash2D(ix + 1, iz,     seed);
    f32 h01 = Hash2D(ix,     iz + 1, seed);
    f32 h11 = Hash2D(ix + 1, iz + 1, seed);

    f32 nx0 = h00 + sfx * (h10 - h00);
    f32 nx1 = h01 + sfx * (h11 - h01);

    return nx0 + sfz * (nx1 - nx0);
}

} // namespace

f32 SampleTerrainHeight(f32 x, f32 z, const TerrainComponent& config) {
    f32 total = 0.0f;
    f32 amplitude = 1.0f;
    f32 max_amp = 0.0f;
    f32 freq = config.frequency;

    for (i32 o = 0; o < std::max(1, config.octaves); ++o) {
        total += Noise2D(x * freq, z * freq, config.seed + o * 1013) * amplitude;
        max_amp += amplitude;
        amplitude *= config.persistence;
        freq *= config.lacunarity;
    }

    if (max_amp > 0.0001f) total /= max_amp;

    // Center elevation around ground level with valleys and ridges
    total = (total * 2.0f - 1.0f);
    return total * config.max_height;
}

MeshData GenerateTerrainMeshData(const TerrainComponent& config, i32 material_index) {
    MeshData result{};
    const i32 res = std::clamp(config.resolution, 4, 256);
    const f32 half_size = config.size * 0.5f;
    const f32 dx = config.size / static_cast<f32>(res - 1);
    const f32 dz = config.size / static_cast<f32>(res - 1);

    // 1. Generate height grid and normal buffer
    std::vector<Vec3> positions(res * res);
    std::vector<Vec2> uvs(res * res);

    for (i32 z = 0; z < res; ++z) {
        for (i32 x = 0; x < res; ++x) {
            const f32 wx = -half_size + x * dx;
            const f32 wz = -half_size + z * dz;
            const f32 wy = SampleTerrainHeight(wx, wz, config);
            const i32 idx = z * res + x;
            positions[idx] = Vec3(wx, wy, wz);
            uvs[idx] = Vec2(static_cast<f32>(x) / (res - 1) * 8.0f,
                            static_cast<f32>(z) / (res - 1) * 8.0f);
        }
    }

    // 2. Compute smooth vertex normals
    std::vector<Vec3> normals(res * res, Vec3(0.0f, 1.0f, 0.0f));
    for (i32 z = 0; z < res; ++z) {
        for (i32 x = 0; x < res; ++x) {
            const f32 left  = (x > 0) ? positions[z * res + (x - 1)].y : positions[z * res + x].y;
            const f32 right = (x < res - 1) ? positions[z * res + (x + 1)].y : positions[z * res + x].y;
            const f32 down  = (z > 0) ? positions[(z - 1) * res + x].y : positions[z * res + x].y;
            const f32 up    = (z < res - 1) ? positions[(z + 1) * res + x].y : positions[z * res + x].y;

            Vec3 n(-(right - left) / (2.0f * dx), 1.0f, -(up - down) / (2.0f * dz));
            normals[z * res + x] = glm::normalize(n);
        }
    }

    // 3. Build triangles
    result.triangles.reserve((res - 1) * (res - 1) * 2);
    for (i32 z = 0; z < res - 1; ++z) {
        for (i32 x = 0; x < res - 1; ++x) {
            i32 i00 = z * res + x;
            i32 i10 = z * res + (x + 1);
            i32 i01 = (z + 1) * res + x;
            i32 i11 = (z + 1) * res + (x + 1);

            // Triangle 1 (i00, i01, i10)
            GPUTriangle t1{};
            t1.v0[0] = positions[i00].x; t1.v0[1] = positions[i00].y; t1.v0[2] = positions[i00].z;
            t1.v1[0] = positions[i01].x; t1.v1[1] = positions[i01].y; t1.v1[2] = positions[i01].z;
            t1.v2[0] = positions[i10].x; t1.v2[1] = positions[i10].y; t1.v2[2] = positions[i10].z;
            t1.n0[0] = normals[i00].x;   t1.n0[1] = normals[i00].y;   t1.n0[2] = normals[i00].z;
            t1.n1[0] = normals[i01].x;   t1.n1[1] = normals[i01].y;   t1.n1[2] = normals[i01].z;
            t1.n2[0] = normals[i10].x;   t1.n2[1] = normals[i10].y;   t1.n2[2] = normals[i10].z;
            t1.uv0[0] = uvs[i00].x; t1.uv0[1] = uvs[i00].y;
            t1.uv1[0] = uvs[i01].x; t1.uv1[1] = uvs[i01].y;
            t1.uv2[0] = uvs[i10].x; t1.uv2[1] = uvs[i10].y;
            t1.mat_index = material_index;
            result.triangles.push_back(t1);

            // Triangle 2 (i10, i01, i11)
            GPUTriangle t2{};
            t2.v0[0] = positions[i10].x; t2.v0[1] = positions[i10].y; t2.v0[2] = positions[i10].z;
            t2.v1[0] = positions[i01].x; t2.v1[1] = positions[i01].y; t2.v1[2] = positions[i01].z;
            t2.v2[0] = positions[i11].x; t2.v2[1] = positions[i11].y; t2.v2[2] = positions[i11].z;
            t2.n0[0] = normals[i10].x;   t2.n0[1] = normals[i10].y;   t2.n0[2] = normals[i10].z;
            t2.n1[0] = normals[i01].x;   t2.n1[1] = normals[i01].y;   t2.n1[2] = normals[i01].z;
            t2.n2[0] = normals[i11].x;   t2.n2[1] = normals[i11].y;   t2.n2[2] = normals[i11].z;
            t2.uv0[0] = uvs[i10].x; t2.uv0[1] = uvs[i10].y;
            t2.uv1[0] = uvs[i01].x; t2.uv1[1] = uvs[i01].y;
            t2.uv2[0] = uvs[i11].x; t2.uv2[1] = uvs[i11].y;
            t2.mat_index = material_index;
            result.triangles.push_back(t2);
        }
    }

    // 4. Compute AABB bounds
    float min_y = 1e20f, max_y = -1e20f;
    for (const auto& p : positions) {
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    }
    result.aabb_min = Vec3(-half_size, min_y, -half_size);
    result.aabb_max = Vec3( half_size, max_y,  half_size);

    // 5. Build BVH
    BuildBVH(result.triangles, result.bvh_nodes, 0, static_cast<int>(result.triangles.size()), 0);

    // 6. Split into GPUTriPos and GPUTriAttr
    result.tri_pos.resize(result.triangles.size());
    result.tri_attr.resize(result.triangles.size());
    for (size_t i = 0; i < result.triangles.size(); ++i) {
        const GPUTriangle& t = result.triangles[i];
        GPUTriPos& p = result.tri_pos[i];
        GPUTriAttr& a = result.tri_attr[i];

        p.v0[0] = t.v0[0]; p.v0[1] = t.v0[1]; p.v0[2] = t.v0[2];
        p.e1[0] = t.v1[0] - t.v0[0]; p.e1[1] = t.v1[1] - t.v0[1]; p.e1[2] = t.v1[2] - t.v0[2];
        p.e2[0] = t.v2[0] - t.v0[0]; p.e2[1] = t.v2[1] - t.v0[1]; p.e2[2] = t.v2[2] - t.v0[2];

        a.n0[0] = t.n0[0]; a.n0[1] = t.n0[1]; a.n0[2] = t.n0[2];
        a.n1[0] = t.n1[0]; a.n1[1] = t.n1[1]; a.n1[2] = t.n1[2];
        a.n2[0] = t.n2[0]; a.n2[1] = t.n2[1]; a.n2[2] = t.n2[2];
        a.uv0[0] = t.uv0[0]; a.uv0[1] = t.uv0[1];
        a.uv1[0] = t.uv1[0]; a.uv1[1] = t.uv1[1];
        a.uv2[0] = t.uv2[0]; a.uv2[1] = t.uv2[1];
        a.mat_index = t.mat_index;
    }

    result.valid = true;
    return result;
}

} // namespace lucida
