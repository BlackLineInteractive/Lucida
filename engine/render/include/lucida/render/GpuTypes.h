// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// GPU-side data layout. Every struct here has a mirror in the shaders, so the
// padding is load-bearing: rows are 16 bytes and the static_asserts fail the
// build the moment a field is added without fixing the shader.
//
// DOD ch.8: hot and cold fields are separated, not grouped by "what an object
// is". See GPUTriPos / GPUTriAttr below.

#include "lucida/core/math/Math.h"

namespace lucida {

enum MaterialType { DIFFUSE = 0, METAL = 1, GLASS = 2, EMISSIVE = 3, CHECKERBOARD = 4, WATER = 5, PBR = 6 };

// Evaluated in the shader from the hit position: one material slot can vary
// albedo, roughness and metallic across a surface with no texture data.
enum ProceduralPattern {
    PROC_NONE = 0,
    PROC_MARBLE,
    PROC_WOOD,
    PROC_RUST,
    PROC_TILES,
    PROC_BRUSHED,
    PROC_HEX,
    PROC_ROUGH_RAMP,
    PROC_PATINA,
    PROC_CONCRETE,
    PROC_COUNT
};

enum MaterialFlags {
    MATFLAG_HAS_BASECOLOR_TEX = 1 << 0,
    MATFLAG_HAS_ORM_TEX       = 1 << 1,
    MATFLAG_ALPHA_BLEND       = 1 << 2,
    MATFLAG_HAS_NORMAL_TEX    = 1 << 3,  // tangent-space normal map present
    MATFLAG_THIN_WALLED       = 1 << 4,  // Thin-walled glass (windows/windshields) vs solid refractive glass
};

// CPU-side material, used when authoring the built-in scenes. Converted to
// GPUMaterial on upload; doubles here because scene setup is not a hot path.
struct Material {
    Vec3   albedo, emission, albedo2;
    double roughness, metallic, refractive_index;
    MaterialType type;
    int    flags = 0;

    // PBR Texture Maps & Channels
    std::string albedo_map;
    std::string normal_map;
    std::string roughness_map;
    std::string metallic_map;
    std::string ao_map;
    std::string emissive_map;

    Vec2  uv_scale{1.0f, 1.0f};
    Vec2  uv_offset{0.0f, 0.0f};
    float normal_scale = 1.0f;
    float ao = 1.0f;
    float emissive_intensity = 1.0f;

    Material(MaterialType t = DIFFUSE, Vec3 alb = Vec3(0.8f), Vec3 emiss = Vec3(0.0f),
             double rough = 0.5, double metal = 0.0, double ri = 1.5, Vec3 alb2 = Vec3(0.1f))
        : albedo(alb), emission(emiss), albedo2(alb2), roughness(rough), metallic(metal),
          refractive_index(ri), type(t), flags(0) {}
};

struct GPUMaterial {
    float albedo[3];       float roughness;
    float emission[3];     float metallic;
    float albedo2[3];      float refractive_index;
    int   type;            int flags; int proc_id; int pad3;
};

struct GPUSphere { float center[3]; float radius;   int mat_index; int pad1, pad2, pad3; };
struct GPUPlane  { float normal[3]; float d_offset; int mat_index; int pad1, pad2, pad3; };
struct GPUCube   { float center[3]; float pad1; float half_size[3]; int mat_index; };
struct GPUCylinder { float center[3]; float radius; float height; int mat_index; int pad1, pad2; };
struct GPUCone     { float center[3]; float radius; float height; int mat_index; int pad1, pad2; };
struct GPUTorus    { float center[3]; float radius; float inner_radius; int mat_index; int pad1, pad2; };
struct GPUDisk     { float center[3]; float radius; float normal[3]; int mat_index; int pad1; };
struct GPULight  { float position[3]; float intensity; float color[3]; float radius; };

// Build-time triangle. Split into the two structs below before upload.
struct GPUTriangle {
    float v0[3], pad0;
    float v1[3], pad1;
    float v2[3], pad2;
    float n0[3], pad3;
    float n1[3], pad4;
    float n2[3], pad5;
    float uv0[2], uv1[2];
    float uv2[2]; int mat_index; float pad6;
};

// Traversal-only data. The BVH leaf loop reads nothing else, and at 5.7M
// triangles the 128-byte version streamed 735 MB through the hottest loop to
// use 37% of it. Edges are pre-subtracted for Moller-Trumbore.
struct GPUTriPos {
    float v0[3]; float pad0;
    float e1[3]; float pad1;
    float e2[3]; float pad2;
};

// Read exactly once, after the closest hit is known.
struct GPUTriAttr {
    float n0[3]; float pad0;
    float n1[3]; float pad1;
    float n2[3]; float pad2;
    float uv0[2], uv1[2];
    float uv2[2]; int mat_index; float pad3;
};
static_assert(sizeof(GPUTriPos)  == 48);
static_assert(sizeof(GPUTriAttr) == 80);

// One BLAS instance. Nodes and triangles of every mesh live in shared buffers;
// an instance names a range plus its own transform, so moving an object costs
// a matrix write instead of a BVH rebuild.
// Transforms are 3x4 affine, row-major: rows 0..2 are axes, column 3 translation.
struct GPUInstance {
    float world_to_local[12];   //   0
    float local_to_world[12];   //  48
    float aabb_min[3]; int node_base;   //  96
    float aabb_max[3]; int tri_base;    // 112
    int   node_count; int mat_base; int flags; int pad0;   // 128

    // The transform this instance had in the previously rendered frame.
    // Temporal upscaling needs to know where a surface *was*, and reprojecting
    // through the camera alone answers that only for geometry that did not move
    // - which is why moving objects used to tear. Equal to local_to_world for
    // anything static, so the shader maths collapses to the camera-only case.
    float prev_local_to_world[12];      // 144
};
static_assert(sizeof(GPUInstance) == 192);

struct GPUBVHNode {
    float aabb_min[3]; int left_or_tri;      // leaf: index into the triangle buffer
    float aabb_max[3]; int right_or_count;   // leaf: count, negative marks a leaf
};

struct GPUNeedle {
    float position[3]; float radius;
    float normal[3];   int   object_id;
    float radiance[3]; int   pad;
};

// 16-byte rows; prev_view_proj must land on offset 176 for float4x4.
struct GPUUniforms {
    int   num_spheres, num_planes, num_cubes, num_bvh_nodes;
    int   num_lights,  max_depth,  num_triangles, enable_triangles;
    float tan_half_fov, aspect_ratio, screen_width, screen_height;
    float ambient_light[3]; float pad2;
    float camera_origin[3]; float pad3;
    float camera_forward[3]; float pad4;
    float camera_right[3];   float pad5;
    float camera_up[3];      float pad6;
    float time; int enable_fog; int enable_jitter; int samples_per_pixel;
    int   debug_mode; float model_pos[3];
    float fog_width, fog_height, jitter_x, jitter_y;
    float prev_view_proj[16];
    float fog_density; int fog_steps; int frame_index;
    float mesh_tex_dim;
    float orm_tex_dim; int mesh_mat_count; int num_instances; float pad10;

    // Appended, never inserted: prev_view_proj has to stay at offset 176 for the
    // shader's float4x4 to line up, and everything before it is already spoken
    // for. New rows go on the end.
    float sky_zenith[3];  float grid_opacity;
    float sky_horizon[3]; float grid_fade;
    float sky_ground[3];  int   grid_enabled;
    float grid_color[3];  float pad11;
    float grid_axis_x[3]; float pad12;
    float grid_axis_z[3]; float pad13;
    int   num_cylinders; int num_cones; int num_tori; int num_disks;
    float grid_spacing; int grid_auto_scale; float pad15; float pad16;

    // Dynamic Lighting & Sun
    float sun_direction[3]; float sun_intensity;
    float sun_color[3];     int   sun_enabled;
    // Water simulation parameters
    float water_height; float water_speed; float water_frequency; float water_foam;
    // Post-processing & Tonemapping
    float exposure; int tonemap_mode; float gamma; float dither_strength;
    // Ambient Occlusion
    float ao_radius; float ao_intensity; int ao_samples; float pad17;
};
static_assert(sizeof(GPUUniforms) == 480);

inline void SetVec3(float* dst, const Vec3& v) { dst[0] = v.x; dst[1] = v.y; dst[2] = v.z; }

} // namespace lucida
