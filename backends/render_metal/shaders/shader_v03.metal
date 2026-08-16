// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include <metal_stdlib>
using namespace metal;

struct Material { packed_float3 albedo; float roughness; packed_float3 emission; float metallic; packed_float3 albedo2; float refractive_index; int type; int flags; int proc_id; int pad3; };

constant float kBayer4[16] = {
     0.0f/16,  8.0f/16,  2.0f/16, 10.0f/16,
    12.0f/16,  4.0f/16, 14.0f/16,  6.0f/16,
     3.0f/16, 11.0f/16,  1.0f/16,  9.0f/16,
    15.0f/16,  7.0f/16, 13.0f/16,  5.0f/16
};

constant int MATFLAG_HAS_BASECOLOR_TEX = 1 << 0;
constant int MATFLAG_HAS_ORM_TEX       = 1 << 1;
constant int MATFLAG_ALPHA_BLEND       = 1 << 2;
struct Sphere { packed_float3 center; float radius; int mat_index; int pad1, pad2, pad3; };
struct Plane { packed_float3 normal; float d_offset; int mat_index; int pad1, pad2, pad3; };
struct Cube { packed_float3 center; float pad1; packed_float3 half_size; int mat_index; };
struct Cylinder { packed_float3 center; float radius; float height; int mat_index; int pad1, pad2; };
struct Cone { packed_float3 center; float radius; float height; int mat_index; int pad1, pad2; };
struct Torus { packed_float3 center; float radius; float inner_radius; int mat_index; int pad1, pad2; };
struct Disk { packed_float3 center; float radius; packed_float3 normal; int mat_index; int pad1; };
struct Light { packed_float3 position; float intensity; packed_float3 color; float radius; };

// Traversal reads only this: 48 bytes with the edges already subtracted.
struct TriPos {
    packed_float3 v0; float pad0;
    packed_float3 e1; float pad1;
    packed_float3 e2; float pad2;
};

// Read once, after the closest hit is known.
struct TriAttr {
    packed_float3 n0; float pad0;
    packed_float3 n1; float pad1;
    packed_float3 n2; float pad2;
    float2 uv0;
    float2 uv1;
    float2 uv2;
    int mat_index; float pad3;
};

// One BLAS placed in the world. See GPUInstance in Scene.h.
struct Instance {
    float4 w2l0, w2l1, w2l2;   // world -> local, rows of a 3x4 affine
    float4 l2w0, l2w1, l2w2;   // local -> world
    packed_float3 aabb_min; int node_base;
    packed_float3 aabb_max; int tri_base;
    int node_count; int mat_base; int flags; int pad0;
    float4 p2w0, p2w1, p2w2;   // local -> world as of the previous frame
};

static inline float3 xform_point(float4 r0, float4 r1, float4 r2, float3 p) {
    return float3(dot(r0.xyz, p) + r0.w,
                  dot(r1.xyz, p) + r1.w,
                  dot(r2.xyz, p) + r2.w);
}
static inline float3 xform_dir(float4 r0, float4 r1, float4 r2, float3 d) {
    return float3(dot(r0.xyz, d), dot(r1.xyz, d), dot(r2.xyz, d));
}

// Flat BVH node
struct BVHNode {
    packed_float3 aabb_min; int left_or_tri;   // negative right_or_count => leaf
    packed_float3 aabb_max; int right_or_count;
};

struct Uniforms {
    int num_spheres, num_planes, num_cubes, num_bvh_nodes;
    int num_lights,  max_depth,  num_triangles, enable_triangles;
    float tan_half_fov, aspect_ratio, screen_width, screen_height;
    packed_float3 ambient_light; float pad2;
    packed_float3 camera_origin; float pad3;
    packed_float3 camera_forward; float pad4;
    packed_float3 camera_right;   float pad5;
    packed_float3 camera_up;      float pad6;
    float time; int enable_fog; int enable_jitter; int samples_per_pixel;
    int debug_mode; packed_float3 model_pos;
    float fog_width, fog_height, jitter_x, jitter_y;
    float4x4 prev_view_proj;
    float fog_density; int fog_steps; int frame_index; float mesh_tex_dim;
    float orm_tex_dim; int mesh_mat_count; int num_instances; float pad10;

    // Mirrors GPUUniforms. Appended, never inserted.
    packed_float3 sky_zenith;  float grid_opacity;
    packed_float3 sky_horizon; float grid_fade;
    packed_float3 sky_ground;  int   grid_enabled;
    packed_float3 grid_color;  float pad11;
    packed_float3 grid_axis_x; float pad12;
    packed_float3 grid_axis_z; float pad13;
    int num_cylinders; int num_cones; int num_tori; int num_disks;
    float grid_spacing; int grid_auto_scale; float pad15; float pad16;
};

constant float EPSILON = 1e-4;
// Guard for the Moller-Trumbore determinant. This must reject only rays that are
// parallel to the triangle plane; |det| also scales with triangle area, so using
// a coarse epsilon here culls small triangles outright.
constant float DET_EPSILON = 1e-12;
constant float INF = 1e20;
constant int MAX_STACK = 12;
constant int DIFFUSE = 0, METAL = 1, GLASS = 2, EMISSIVE = 3, CHECKERBOARD = 4, WATER = 5, PBR = 6;

struct Ray { float3 origin; float3 direction; };
struct HitInfo {
    bool hit; float t; float3 point, normal; int mat_index; float2 uv; bool is_mesh;
    // Instance this hit belongs to, or -1 for world-space analytic geometry.
    // Only the motion-vector pass reads it, but it has to be carried from the
    // traversal loop, which is the only place that knows it.
    int inst_id;
    // sqrt(uv_area / world_area) of the hit triangle: texture-space density used
    // to pick a mip level. 0 for analytic primitives, which are not textured.
    float uv_density;
    // Geometric (face) normal. Secondary-ray offsets must use this rather than
    // the interpolated shading normal: on curved geometry the two diverge, and
    // offsetting along a shading normal that leans into the surface leaves the
    // ray able to re-hit the triangle it started from. That self-intersection is
    // what shredded Sponza's drapery into black slivers while the flat stonework
    // stayed clean.
    float3 geo_normal;
    // Centre of the primitive that was hit. Procedural patterns are evaluated
    // relative to this: anchoring them to world space put the arguments in the
    // thousands for objects far from the origin, where fract() loses all
    // precision and the noise collapses to a constant.
    float3 obj_origin;
};

// Origin for a secondary ray leaving a surface. The offset scales with hit
// distance because float precision on the hit point degrades with it, so a
// fixed epsilon that works up close is far too small across a large scene.
inline float3 offset_ray(float3 p, float3 geo_n, float3 dir, float t) {
    float3 n = (dot(geo_n, dir) < 0.0) ? -geo_n : geo_n;
    return p + n * max(1e-3, t * 2e-3);
}

Ray make_ray(float3 o, float3 d) { return {o, normalize(d)}; }

// ---------------------------------------------------------------- BVH traversal
//
// The traversal keeps only (t, u, v, triangle index) while walking the tree and
// reconstructs the normal / UV / material exactly once, after the closest hit is
// known. Interpolating and normalising per candidate triangle - as the previous
// version did - costs a normalize() and a full HitInfo copy for every triangle
// tested, most of which are immediately discarded.

struct TriHit { float t; float u, v; int idx; };

constant int BVH_STACK = 28;

// Slab test. Returns whether the box is hit within [0, tmax] and reports the
// entry distance, which drives near-child-first ordering.
static inline bool slab_hit(float3 bmin, float3 bmax, float3 ro, float3 inv,
                            float tmax, thread float& tenter) {
    float3 t0 = (bmin - ro) * inv;
    float3 t1 = (bmax - ro) * inv;
    float3 mn = min(t0, t1);
    float3 mx = max(t0, t1);
    float te = max(max(mn.x, mn.y), mn.z);
    float tx = min(min(mx.x, mx.y), mx.z);
    tenter = te;
    return tx >= max(te, 0.0f) && te < tmax;
}

TriHit intersect_bvh(Ray ray, device const BVHNode* nodes,
                     device const TriPos* tris, int node_count,
                     int node_base, int tri_base) {
    TriHit best; best.t = INF; best.u = 0.0f; best.v = 0.0f; best.idx = -1;
    if (node_count == 0) return best;

    float3 ro  = ray.origin;
    float3 rd  = ray.direction;
    float3 inv = 1.0f / rd;

    int   stack_node[BVH_STACK];
    float stack_tenter[BVH_STACK];
    int   sp   = 0;
    int   node = 0;

    { // reject the whole tree up front
        float te;
        BVHNode r = nodes[node_base];
        if (!slab_hit(r.aabb_min, r.aabb_max, ro, inv, best.t, te)) return best;
    }

    for (;;) {
        BVHNode n = nodes[node_base + node];

        if (n.right_or_count <= 0) {
            int start = n.left_or_tri;
            int cnt   = -n.right_or_count;
            for (int i = 0; i < cnt; i++) {
                device const TriPos& tri = tris[tri_base + start + i];
                float3 e1 = tri.e1;
                float3 e2 = tri.e2;
                float3 h  = cross(rd, e2);
                float  a  = dot(e1, h);
                if (abs(a) < DET_EPSILON) continue;   // parallel only, not "small"

                float  f  = 1.0f / a;
                float3 s  = ro - tri.v0;
                float  u  = f * dot(s, h);
                if (u < 0.0f || u > 1.0f) continue;
                float3 q  = cross(s, e1);
                float  v  = f * dot(rd, q);
                if (v < 0.0f || u + v > 1.0f) continue;
                float  t  = f * dot(e2, q);
                if (t > EPSILON && t < best.t) {
                    best.t = t; best.u = u; best.v = v; best.idx = tri_base + start + i;
                }
            }
        } else {
            int l = n.left_or_tri;
            int r = n.right_or_count;
            BVHNode nl = nodes[node_base + l];
            BVHNode nr = nodes[node_base + r];
            float tl, tr;
            bool hl = slab_hit(nl.aabb_min, nl.aabb_max, ro, inv, best.t, tl);
            bool hr = slab_hit(nr.aabb_min, nr.aabb_max, ro, inv, best.t, tr);

            if (hl && hr) {
                if (tl > tr) { int ti = l; l = r; r = ti; float tf = tl; tl = tr; tr = tf; }
                if (sp < BVH_STACK) { stack_node[sp] = r; stack_tenter[sp] = tr; sp++; }
                node = l;
                continue;
            }
            if (hl) { node = l; continue; }
            if (hr) { node = r; continue; }
        }

        // Pop, skipping entries the closest hit has since made irrelevant.
        bool popped = false;
        while (sp > 0) {
            sp--;
            if (stack_tenter[sp] < best.t) { node = stack_node[sp]; popped = true; break; }
        }
        if (!popped) break;
    }
    return best;
}

// Resolve the interpolated shading attributes of a confirmed closest hit.
HitInfo resolve_tri_hit(Ray ray, TriHit th, device const TriPos* tris,
                        device const TriAttr* attrs) {
    HitInfo info;
    info.inst_id = -1;   // the caller overwrites this with its instance index
    device const TriPos&  tri = tris[th.idx];
    device const TriAttr& at  = attrs[th.idx];
    float w = 1.0f - th.u - th.v;
    info.hit       = true;
    info.t         = th.t;
    info.point     = ray.origin + ray.direction * th.t;
    info.normal    = normalize(w * at.n0 + th.u * at.n1 + th.v * at.n2);
    info.uv        = w * at.uv0 + th.u * at.uv1 + th.v * at.uv2;
    info.mat_index = at.mat_index;
    info.is_mesh   = true;

    // Ray-cone texture LOD (Akenine-Möller): the ratio of the triangle's area in
    // UV space to its area in world space gives how many texels a world unit
    // covers. Combined with the pixel cone footprint at distance t this yields a
    // mip level, which is what stops Sponza's brick and drapery textures from
    // shimmering into noise at distance.
    float2 duv1 = at.uv1 - at.uv0;
    float2 duv2 = at.uv2 - at.uv0;
    float  uv_area    = abs(duv1.x * duv2.y - duv1.y * duv2.x);
    float3 geo_cross  = cross(tri.e1, tri.e2);
    float  world_area = length(geo_cross);
    info.uv_density   = sqrt(uv_area / max(world_area, 1e-12f));
    info.geo_normal   = (world_area > 1e-20f) ? (geo_cross / world_area) : info.normal;
    info.obj_origin   = float3(0.0);
    return info;
}

// ---------------------------------------------------------------- BVH Shadow Any-Hit
// Any-hit: no ordering, no attribute interpolation, returns on the first blocker.
bool intersect_bvh_shadow(Ray ray, float max_t, device const BVHNode* nodes,
                          device const TriPos* tris, int node_count,
                          int node_base, int tri_base) {
    if (node_count == 0) return false;

    float3 ro  = ray.origin;
    float3 rd  = ray.direction;
    float3 inv = 1.0f / rd;

    int stack_node[BVH_STACK];
    int sp   = 0;
    int node = 0;

    {
        float te;
        BVHNode r = nodes[node_base];
        if (!slab_hit(r.aabb_min, r.aabb_max, ro, inv, max_t, te)) return false;
    }

    for (;;) {
        BVHNode n = nodes[node_base + node];

        if (n.right_or_count <= 0) {
            int start = n.left_or_tri;
            int cnt   = -n.right_or_count;
            for (int i = 0; i < cnt; i++) {
                device const TriPos& tri = tris[tri_base + start + i];
                float3 e1 = tri.e1;
                float3 e2 = tri.e2;
                float3 h  = cross(rd, e2);
                float  a  = dot(e1, h);
                if (abs(a) < DET_EPSILON) continue;   // parallel only, not "small"

                float  f  = 1.0f / a;
                float3 s  = ro - tri.v0;
                float  u  = f * dot(s, h);
                if (u < 0.0f || u > 1.0f) continue;
                float3 q  = cross(s, e1);
                float  v  = f * dot(rd, q);
                if (v < 0.0f || u + v > 1.0f) continue;
                float  t  = f * dot(e2, q);
                if (t > EPSILON && t < max_t) return true;
            }
        } else {
            int l = n.left_or_tri;
            int r = n.right_or_count;
            BVHNode nl = nodes[node_base + l];
            BVHNode nr = nodes[node_base + r];
            float tl, tr;
            bool hl = slab_hit(nl.aabb_min, nl.aabb_max, ro, inv, max_t, tl);
            bool hr = slab_hit(nr.aabb_min, nr.aabb_max, ro, inv, max_t, tr);

            if (hl && hr) {
                if (sp < BVH_STACK) stack_node[sp++] = r;
                node = l;
                continue;
            }
            if (hl) { node = l; continue; }
            if (hr) { node = r; continue; }
        }

        if (sp == 0) break;
        node = stack_node[--sp];
    }
    return false;
}

// ------------------------------------------------------- procedural materials
//
// Evaluated from the world-space hit position, so a single material slot can
// present a spatially varying surface with no texture data at all. Each pattern
// drives albedo, roughness and metallic together, because that is what makes a
// surface read as a material rather than as a painted colour: rust is not brown
// paint, it is iron that stopped being metal and got rougher.

constant int PROC_NONE = 0, PROC_MARBLE = 1, PROC_WOOD = 2, PROC_RUST = 3,
             PROC_TILES = 4, PROC_BRUSHED = 5, PROC_HEX = 6,
             PROC_ROUGH_RAMP = 7, PROC_PATINA = 8, PROC_CONCRETE = 9;

static float phash(float3 p) {
    p = fract(p * 0.3183099 + float3(0.71, 0.113, 0.419));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

static float pnoise(float3 x) {
    float3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(phash(i + float3(0,0,0)), phash(i + float3(1,0,0)), f.x),
                   mix(phash(i + float3(0,1,0)), phash(i + float3(1,1,0)), f.x), f.y),
               mix(mix(phash(i + float3(0,0,1)), phash(i + float3(1,0,1)), f.x),
                   mix(phash(i + float3(0,1,1)), phash(i + float3(1,1,1)), f.x), f.y), f.z);
}

static float fbm(float3 p, int octaves) {
    float a = 0.5, sum = 0.0;
    for (int i = 0; i < octaves; i++) { sum += a * pnoise(p); p *= 2.03; a *= 0.5; }
    return sum;
}

// Applies the pattern in place. `lp` is the position local to the object, which
// keeps the pattern anchored to the surface instead of swimming through it.
static void apply_procedural(int id, float3 lp, thread float3& albedo,
                             thread float& roughness, thread float& metallic) {
    if (id == PROC_MARBLE) {
        float veins = fbm(lp * 3.0, 4);
        float m = abs(sin((lp.x + lp.y * 0.6) * 6.0 + veins * 7.0));
        m = pow(m, 0.35);
        albedo = mix(float3(0.06, 0.07, 0.09), float3(0.86, 0.85, 0.82), m);
        roughness = mix(0.05, 0.18, m);   // polished stone, veins slightly duller
        metallic = 0.0;
    } else if (id == PROC_WOOD) {
        float r = length(lp.xz * float2(1.0, 0.55)) * 14.0 + fbm(lp * 5.0, 3) * 2.2;
        float rings = fract(r);
        rings = smoothstep(0.15, 0.55, rings);
        albedo = mix(float3(0.23, 0.11, 0.045), float3(0.51, 0.30, 0.14), rings);
        roughness = mix(0.30, 0.62, rings);
        metallic = 0.0;
    } else if (id == PROC_RUST) {
        float r = fbm(lp * 4.5, 5);
        float rust = smoothstep(0.42, 0.62, r);
        albedo = mix(float3(0.56, 0.57, 0.58), float3(0.35, 0.13, 0.05), rust);
        metallic = mix(1.0, 0.0, rust);     // oxide is no longer a conductor
        roughness = mix(0.18, 0.88, rust);
    } else if (id == PROC_TILES) {
        float2 t = lp.xz * 6.0;
        float2 c = fract(t) - 0.5;
        float grout = min(smoothstep(0.42, 0.48, abs(c.x)), 1.0) *
                      min(smoothstep(0.42, 0.48, abs(c.y)), 1.0);
        grout = 1.0 - max(smoothstep(0.42, 0.49, abs(c.x)), smoothstep(0.42, 0.49, abs(c.y)));
        float tint = phash(float3(floor(t), 0.0));
        albedo = mix(float3(0.19, 0.18, 0.17),
                     mix(float3(0.13, 0.35, 0.46), float3(0.20, 0.52, 0.60), tint), grout);
        roughness = mix(0.85, 0.06, grout);  // glazed face, matte grout
        metallic = 0.0;
    } else if (id == PROC_BRUSHED) {
        float scratch = fbm(float3(lp.x * 220.0, lp.y * 2.0, lp.z * 220.0), 2);
        albedo = float3(0.91, 0.92, 0.94);
        metallic = 1.0;
        roughness = clamp(0.16 + scratch * 0.34, 0.04, 0.7);
    } else if (id == PROC_HEX) {
        float2 p = lp.xz * 7.0;
        p.y *= 0.8660254;
        float2 g = float2(floor(p.x + 0.5 * fract(p.y)), floor(p.y));
        float cell = phash(float3(g, 0.0));
        float2 f = float2(fract(p.x + 0.5 * fract(p.y)) - 0.5, fract(p.y) - 0.5);
        float edge = smoothstep(0.34, 0.46, max(abs(f.x), abs(f.y)));
        albedo = mix(mix(float3(0.75, 0.62, 0.18), float3(0.30, 0.30, 0.33), cell),
                     float3(0.03), edge);
        metallic = 1.0 - edge;
        roughness = mix(0.12 + cell * 0.35, 0.9, edge);
    } else if (id == PROC_ROUGH_RAMP) {
        // The point of this one is to read the whole roughness axis at a glance.
        float k = clamp(lp.y * 0.5 + 0.5, 0.0, 1.0);
        albedo = float3(0.94, 0.78, 0.38);
        metallic = 1.0;
        roughness = clamp(k, 0.02, 1.0);
    } else if (id == PROC_PATINA) {
        float cav = fbm(lp * 5.5, 4);
        float green = smoothstep(0.45, 0.72, cav);
        albedo = mix(float3(0.72, 0.36, 0.18), float3(0.24, 0.55, 0.44), green);
        metallic = mix(1.0, 0.05, green);
        roughness = mix(0.14, 0.75, green);
    } else if (id == PROC_CONCRETE) {
        float agg = fbm(lp * 9.0, 4);
        float pit = smoothstep(0.62, 0.72, fbm(lp * 22.0, 2));
        albedo = float3(0.52, 0.51, 0.49) * (0.72 + agg * 0.45) - pit * 0.16;
        roughness = clamp(0.72 + pit * 0.2, 0.0, 1.0);
        metallic = 0.0;
    }
}

// Any-hit across every instance. Shadow, AO and sun-visibility rays all go
// through here so they see moving geometry exactly like the primary ray does.
bool any_hit_instances(Ray ray, float max_t,
                       device const BVHNode* nodes, device const TriPos* tris,
                       device const Instance* instances, constant Uniforms& u) {
    if (u.enable_triangles == 0 || u.num_instances == 0) return false;
    float3 inv = 1.0f / ray.direction;
    for (int ii = 0; ii < u.num_instances; ii++) {
        device const Instance& inst = instances[ii];
        float te;
        if (!slab_hit(inst.aabb_min, inst.aabb_max, ray.origin, inv, max_t, te)) continue;
        Ray lr;
        lr.origin    = xform_point(inst.w2l0, inst.w2l1, inst.w2l2, ray.origin);
        lr.direction = xform_dir(inst.w2l0, inst.w2l1, inst.w2l2, ray.direction);
        if (intersect_bvh_shadow(lr, max_t, nodes, tris, inst.node_count,
                                 inst.node_base, inst.tri_base))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------- hash / noise for jitter
float hash21(float2 p) {
    p = fract(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}


HitInfo intersect_sphere(Ray ray, device const Sphere& s) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float3 oc = ray.origin - s.center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(oc, ray.direction);
    float c = dot(oc, oc) - s.radius*s.radius;
    float disc = b*b - 4.0*a*c;
    if (disc >= 0.0) {
        float sd = sqrt(disc);
        float t1 = (-b - sd) / (2.0*a);
        float t2 = (-b + sd) / (2.0*a);
        float t = INF;
        if (t1 > EPSILON) t = t1; else if (t2 > EPSILON) t = t2;
        if (t < INF) {
            info.hit = true; info.t = t;
            info.point = ray.origin + ray.direction * t;
            info.normal = normalize(info.point - s.center); info.geo_normal = info.normal;
            info.obj_origin = s.center;
            info.mat_index = s.mat_index;
            info.is_mesh = false;
            float3 pl = (info.point - s.center) / s.radius;
            info.uv = float2((atan2(pl.z, pl.x) + 3.14159) / (2.0 * 3.14159), (asin(clamp(pl.y, -1.0, 1.0)) + 1.5707) / 3.14159);
        }
    }
    return info;
}

HitInfo intersect_plane(Ray ray, device const Plane& p) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float denom = dot(p.normal, ray.direction);
    if (abs(denom) > EPSILON) {
        float t = (p.d_offset - dot(ray.origin, p.normal)) / denom;
        if (t > EPSILON) {
            info.hit = true; info.t = t;
            info.point = ray.origin + ray.direction * t;
            info.normal = p.normal; info.geo_normal = info.normal;
            info.obj_origin = float3(0.0);
            info.mat_index = p.mat_index;
            info.is_mesh = false;
            float3 u_axis = abs(p.normal.y) > 0.9 ? float3(1,0,0) : normalize(cross(float3(0,1,0), p.normal));
            float3 v_axis = normalize(cross(p.normal, u_axis));
            info.uv = float2(dot(info.point, u_axis)*0.1, dot(info.point, v_axis)*0.1);
        }
    }
    return info;
}

HitInfo intersect_cube(Ray ray, device const Cube& c) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float3 inv_dir = 1.0 / ray.direction;
    float3 t1 = (c.center - c.half_size - ray.origin) * inv_dir;
    float3 t2 = (c.center + c.half_size - ray.origin) * inv_dir;
    float3 tminv = min(t1, t2); float3 tmaxv = max(t1, t2);
    float t_enter = max(max(tminv.x, tminv.y), tminv.z);
    float t_exit  = min(min(tmaxv.x, tmaxv.y), tmaxv.z);
    if (t_exit < EPSILON || t_enter > t_exit) return info;
    float t = (t_enter > EPSILON) ? t_enter : t_exit;
    if (t > EPSILON) {
        info.hit = true; info.t = t;
        info.point = ray.origin + ray.direction * t;
        info.mat_index = c.mat_index;
        info.is_mesh = false;
        float3 hr = info.point - c.center;
        float3 n = float3(0.0);
        if (abs(abs(hr.x) - c.half_size.x) < EPSILON*10.0) n.x = sign(hr.x);
        else if (abs(abs(hr.y) - c.half_size.y) < EPSILON*10.0) n.y = sign(hr.y);
        else if (abs(abs(hr.z) - c.half_size.z) < EPSILON*10.0) n.z = sign(hr.z);
        info.normal = normalize(n); info.geo_normal = info.normal;
        info.obj_origin = c.center;
        info.uv = float2(hr.x / c.half_size.x, hr.z / c.half_size.z);
    }
    return info;
}

HitInfo intersect_cylinder(Ray ray, device const Cylinder& c) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float3 oc = ray.origin - c.center;
    float a = ray.direction.x*ray.direction.x + ray.direction.z*ray.direction.z;
    float b = 2.0 * (oc.x*ray.direction.x + oc.z*ray.direction.z);
    float c_val = oc.x*oc.x + oc.z*oc.z - c.radius*c.radius;
    float disc = b*b - 4.0*a*c_val;
    if (disc < 0.0) return info;
    float sqrtd = sqrt(disc);
    float t0 = (-b - sqrtd) / (2.0*a);
    float t1 = (-b + sqrtd) / (2.0*a);
    float t = t0;
    float y = oc.y + t*ray.direction.y;
    if (t < EPSILON || abs(y) > c.height) {
        t = t1;
        y = oc.y + t*ray.direction.y;
        if (t < EPSILON || abs(y) > c.height) return info;
    }
    info.hit = true; info.t = t;
    info.point = ray.origin + ray.direction * t;
    info.normal = normalize(float3(info.point.x - c.center.x, 0.0, info.point.z - c.center.z));
    info.geo_normal = info.normal;
    info.mat_index = c.mat_index;
    info.obj_origin = c.center;
    info.is_mesh = false;
    info.uv = float2((atan2(info.normal.z, info.normal.x) + 3.14159)/(2.0*3.14159), y / (2.0*c.height) + 0.5);
    return info;
}

HitInfo intersect_cone(Ray ray, device const Cone& c) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float3 oc = ray.origin - c.center;
    float k = c.radius / c.height; k = k*k;
    float dy = oc.y - c.height;
    float a = ray.direction.x*ray.direction.x + ray.direction.z*ray.direction.z - k*ray.direction.y*ray.direction.y;
    float b = 2.0 * (oc.x*ray.direction.x + oc.z*ray.direction.z - k*dy*ray.direction.y);
    float c_val = oc.x*oc.x + oc.z*oc.z - k*dy*dy;
    float disc = b*b - 4.0*a*c_val;
    if (disc < 0.0) return info;
    float sqrtd = sqrt(disc);
    float t0 = (-b - sqrtd) / (2.0*a);
    float t1 = (-b + sqrtd) / (2.0*a);
    float t = t0;
    float y = oc.y + t*ray.direction.y;
    if (t < EPSILON || y < -c.height || y > c.height) {
        t = t1;
        y = oc.y + t*ray.direction.y;
        if (t < EPSILON || y < -c.height || y > c.height) return info;
    }
    info.hit = true; info.t = t;
    info.point = ray.origin + ray.direction * t;
    float r = sqrt((info.point.x - c.center.x)*(info.point.x - c.center.x) + (info.point.z - c.center.z)*(info.point.z - c.center.z));
    info.normal = normalize(float3(info.point.x - c.center.x, r * (c.radius/c.height), info.point.z - c.center.z));
    info.geo_normal = info.normal;
    info.mat_index = c.mat_index;
    info.obj_origin = c.center;
    info.is_mesh = false;
    info.uv = float2((atan2(info.normal.z, info.normal.x) + 3.14159)/(2.0*3.14159), (y+c.height) / (2.0*c.height));
    return info;
}

HitInfo intersect_torus(Ray ray, device const Torus& tor) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float t = 0.0;
    for (int i=0; i<64; i++) {
        float3 p = ray.origin + ray.direction * t - tor.center;
        float2 q = float2(length(p.xz) - tor.radius, p.y);
        float d = length(q) - tor.inner_radius;
        if (d < 0.001) {
            info.hit = true; info.t = t;
            info.point = ray.origin + ray.direction * t;
            float3 center_ring = tor.center + normalize(float3(p.x, 0.0, p.z)) * tor.radius;
            info.normal = normalize(info.point - center_ring);
            info.geo_normal = info.normal;
            info.mat_index = tor.mat_index;
            info.obj_origin = tor.center;
            info.is_mesh = false;
            info.uv = float2((atan2(p.z, p.x) + 3.14159)/(2.0*3.14159), (atan2(p.y, length(p.xz)-tor.radius) + 3.14159)/(2.0*3.14159));
            return info;
        }
        t += d;
        if (t > 100.0) break;
    }
    return info;
}

HitInfo intersect_disk(Ray ray, device const Disk& d) {
    HitInfo info; info.hit = false; info.t = INF; info.inst_id = -1;
    float denom = dot(d.normal, ray.direction);
    if (abs(denom) > EPSILON) {
        float t = dot(d.center - ray.origin, d.normal) / denom;
        if (t > EPSILON) {
            float3 p = ray.origin + ray.direction * t;
            float3 v = p - d.center;
            if (dot(v, v) <= d.radius * d.radius) {
                info.hit = true; info.t = t;
                info.point = p;
                info.normal = d.normal; info.geo_normal = d.normal;
                info.obj_origin = d.center;
                info.mat_index = d.mat_index;
                info.is_mesh = false;
                info.uv = float2(0.5) + float2(v.x, v.z) / (2.0 * d.radius);
            }
        }
    }
    return info;
}

// Walks every instance: reject by its world AABB, then transform the ray into
// the instance's local space and traverse that BLAS.
//
// The local direction is deliberately left un-normalised. Under a rigid or
// uniformly scaled transform that keeps the parameter t identical to world
// space, so hits from different instances are directly comparable and the
// closest-so-far distance can prune later instances without any conversion.
HitInfo find_closest(Ray ray,
                     device const Sphere*   spheres,
                     device const Plane*    planes,
                     device const Cube*     cubes,
                     device const Cylinder* cylinders,
                     device const Cone*     cones,
                     device const Torus*    tori,
                     device const Disk*     disks,
                     device const BVHNode*  bvh_nodes,
                     device const TriPos*   triangles,
                     device const TriAttr*  tri_attrs,
                     device const Instance* instances,
                     constant Uniforms&     u) {
    HitInfo closest; closest.hit = false; closest.t = INF; closest.inst_id = -1;

    for (int i = 0; i < u.num_spheres; i++) { HitInfo h = intersect_sphere(ray, spheres[i]); if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_planes;  i++) { HitInfo h = intersect_plane(ray, planes[i]);   if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_cubes;   i++) { HitInfo h = intersect_cube(ray, cubes[i]);     if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_cylinders; i++) { HitInfo h = intersect_cylinder(ray, cylinders[i]); if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_cones;   i++) { HitInfo h = intersect_cone(ray, cones[i]);     if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_tori;    i++) { HitInfo h = intersect_torus(ray, tori[i]);     if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_disks;   i++) { HitInfo h = intersect_disk(ray, disks[i]);     if (h.hit && h.t < closest.t) closest = h; }

    if (u.enable_triangles != 0) {
        float3 inv = 1.0f / ray.direction;
        for (int ii = 0; ii < u.num_instances; ii++) {
            device const Instance& inst = instances[ii];

            float te;
            if (!slab_hit(inst.aabb_min, inst.aabb_max, ray.origin, inv, closest.t, te))
                continue;

            Ray lr;
            lr.origin    = xform_point(inst.w2l0, inst.w2l1, inst.w2l2, ray.origin);
            lr.direction = xform_dir(inst.w2l0, inst.w2l1, inst.w2l2, ray.direction);

            TriHit th = intersect_bvh(lr, bvh_nodes, triangles, inst.node_count,
                                      inst.node_base, inst.tri_base);
            if (th.idx < 0 || th.t >= closest.t) continue;

            HitInfo h = resolve_tri_hit(lr, th, triangles, tri_attrs);
            h.inst_id    = ii;
            h.point      = xform_point(inst.l2w0, inst.l2w1, inst.l2w2, h.point);
            h.normal     = normalize(xform_dir(inst.l2w0, inst.l2w1, inst.l2w2, h.normal));
            h.geo_normal = normalize(xform_dir(inst.l2w0, inst.l2w1, inst.l2w2, h.geo_normal));
            h.obj_origin = xform_point(inst.l2w0, inst.l2w1, inst.l2w2, h.obj_origin);
            h.mat_index += inst.mat_base;
            closest = h;
        }
    }

    // The floor plane is world-space and belongs to no instance.
    for (int i = 0; i < u.num_planes; i++) {
        HitInfo h = intersect_plane(ray, planes[i]);
        if (h.hit && h.t < closest.t) closest = h;
    }
    return closest;
}

float cone_sphere_occlusion(float3 cone_o, float3 cone_d, float cone_angle, float3 s_center, float s_radius) {
    float3 L = s_center - cone_o;
    float dist = length(L);
    if (dist < s_radius) return 1.0; 
    L /= dist;
    float obj_angle = asin(s_radius / dist);
    float cos_diff = dot(cone_d, L);
    float angle_diff = acos(clamp(cos_diff, -1.0f, 1.0f));
    if (angle_diff > cone_angle + obj_angle) return 0.0;
    if (angle_diff + cone_angle <= obj_angle) return 1.0;
    float overlap = (cone_angle + obj_angle - angle_diff) / (2.0 * cone_angle);
    return smoothstep(0.0, 1.0, clamp(overlap, 0.0, 1.0));
}

bool point_in_cube(float3 p, device const Cube& c) {
    float3 d = abs(p - c.center);
    return d.x <= c.half_size.x && d.y <= c.half_size.y && d.z <= c.half_size.z;
}

float calc_analytic_shadow(float3 p, float3 n, float3 geo_n, float hit_t, float3 lpos, float lrad, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, device const Cylinder* cylinders, device const Cone* cones, device const Torus* tori, device const Disk* disks, device const BVHNode* bvh_nodes, device const TriPos* triangles,
                 device const TriAttr* tri_attrs,
                 device const Instance* instances, constant Uniforms& u) {
    float3 L = lpos - p;
    float actual_dist = length(L);
    L /= actual_dist;
    float light_angle = atan(lrad / actual_dist);
    float occlusion = 0.0;
    float3 ro = offset_ray(p, geo_n, L, hit_t);

    for(int i = 0; i < u.num_spheres; i++) {
        if (distance(spheres[i].center, lpos) < 1e-2) continue;
        occlusion += cone_sphere_occlusion(ro, L, light_angle, spheres[i].center, spheres[i].radius);
    }
    for(int i = 0; i < u.num_cubes; i++) {
        if (point_in_cube(ro, cubes[i])) continue;
        Ray shadow_ray = make_ray(ro, L);
        HitInfo h = intersect_cube(shadow_ray, cubes[i]);
        if (h.hit && h.t > 0.05 && h.t < actual_dist - 0.05) {
            occlusion = 1.0;
            break;
        }
    }
    
    // Mesh shadows (Any-Hit BVH)
    if (u.enable_triangles > 0 && occlusion < 1.0) {
        Ray shadow_ray = make_ray(ro, L);
        if (any_hit_instances(shadow_ray, actual_dist - 0.05, bvh_nodes, triangles, instances, u)) {
            occlusion = 1.0;
        }
    }
    
    return 1.0 - clamp(occlusion, 0.0, 1.0);
}

float calc_analytic_ao(float3 p, float3 n, device const Sphere* spheres, device const Cube* cubes, constant Uniforms& u) {
    float occlusion = 0.0;
    float3 ro = p + n * 0.05;
    float ao_cone_angle = 0.8;
    for(int i = 0; i < u.num_spheres; i++) {
        float3 L = spheres[i].center - ro;
        float dist = length(L);
        if(dist > 0.01 && dist < 3.0) {
            float occ = cone_sphere_occlusion(ro, n, ao_cone_angle, spheres[i].center, spheres[i].radius);
            occlusion += occ * (1.0 - dist / 3.0);
        }
    }
    for(int i = 0; i < u.num_cubes; i++) {
        float3 L = cubes[i].center - ro;
        float dist = length(L);
        if(dist > 0.01 && dist < 3.0) {
            float occ = cone_sphere_occlusion(ro, n, ao_cone_angle, cubes[i].center, length(cubes[i].half_size)) * 0.5;
            occlusion += occ * (1.0 - dist / 3.0);
        }
    }
    return 1.0 - clamp(occlusion, 0.0, 1.0);
}

float3 schlick(float cos_theta, float n1, float n2) {
    float r0 = (n1 - n2) / (n1 + n2); r0 *= r0;
    float x = 1.0 - cos_theta;
    return float3(r0 + (1.0 - r0) * pow(x, 5.0));
}

float3 sky_color(float3 dir, constant Uniforms& u) {
    // A gradient the scene owns, not a constant baked into the shader: an empty
    // project needs to look like somewhere, and loading an HDRI to get a sky is
    // exactly the cost this engine is trying not to pay.
    float3 zenith  = float3(u.sky_zenith);
    float3 horizon = float3(u.sky_horizon);
    float3 ground  = float3(u.sky_ground);

    float up = dir.y;
    if (up >= 0.0) {
        float t = pow(saturate(up), 0.55);
        return mix(horizon, zenith, t);
    }
    float t = pow(saturate(-up * 2.0), 0.8);
    return mix(horizon, ground, t);
}

// ------------------------------------------------------- mesh lighting model
//
// The analytic terms below (calc_analytic_ao / calc_gi) only ever loop over
// spheres, planes and cubes, so a loaded mesh received *no* occlusion and *no*
// bounce light - calc_analytic_ao returns a flat 1.0 and calc_gi contributes
// nothing. Sponza was therefore lit by a single point light meant for a scene of
// three spheres, which is why it read as flat and why anything the point light
// could not reach went black.
//
// A mesh instead gets: a directional sun (shadowed against the BVH), sky
// irradiance over the hemisphere, and ray-traced ambient occlusion.

// Pre-normalised: MSL forbids global constructors, so normalize() cannot be
// called at file scope. This is normalize(0.5, 0.4, 0.7), matching sky_color.
constant float3 kSunDir       = {0.527046f, 0.421637f, 0.737865f};
constant float3 kSunColor     = {1.0f, 0.94f, 0.82f};
constant float  kSunIntensity = 5.0;
constant int    kAORays       = 6;

// Ambient occlusion by short any-hit rays over the cosine hemisphere. The
// directions come from a golden-angle spiral: fixed, identical for every pixel,
// so the result is completely deterministic - no noise to filter back out.
float trace_ao(float3 p, float3 N, float3 geo_n, float t, float radius,
               device const BVHNode* nodes, device const TriPos* tris,
               device const Instance* instances,
               constant Uniforms& u) {
    if (u.enable_triangles == 0 || u.num_bvh_nodes == 0) return 1.0;

    float3 T = normalize(abs(N.z) < 0.9 ? cross(float3(0.0, 0.0, 1.0), N)
                                        : cross(float3(1.0, 0.0, 0.0), N));
    float3 B = cross(N, T);
    float3 ro = offset_ray(p, geo_n, N, t) - u.model_pos;

    float occ = 0.0;
    for (int i = 0; i < kAORays; i++) {
        float r  = sqrt((float(i) + 0.5) / float(kAORays));
        float th = float(i) * 2.3999632;              // golden angle
        float3 d = T * (r * cos(th)) + B * (r * sin(th))
                 + N * sqrt(max(0.0, 1.0 - r * r));
        if (any_hit_instances(make_ray(ro, d), radius, nodes, tris, instances, u))
            occ += 1.0;
    }
    return 1.0 - occ / float(kAORays);
}

// One bounce of indirect diffuse, evaluated over the same cosine hemisphere the
// AO rays already walked. Counting those rays as a binary hit/miss threw away
// everything that makes an interior read as lit: a ray that escapes carries sky
// radiance from the direction it escaped in, and a ray that lands carries the
// sunlit colour of whatever it landed on. That bounce is why Sponza's shadowed
// walls pick up warm light from the floor instead of sitting flat and dead.
//
// Rays are bounded: only nearby geometry contributes meaningfully to a single
// bounce, and a short ray leaves the BVH early.
struct GIResult { float3 bent_normal; float ao; };

GIResult trace_gi(float3 p, float3 N, float3 geo_n, float t, float radius,
                  device const BVHNode* nodes, device const TriPos* tris,
                  device const Instance* instances,
                  device const Material* mesh_mats,
                  texture2d_array<float, access::sample> mesh_textures,
                  constant Uniforms& u) {
    GIResult r; r.bent_normal = N; r.ao = 1.0;
    if (u.enable_triangles == 0 || u.num_bvh_nodes == 0) return r;

    float3 T = normalize(abs(N.z) < 0.9 ? cross(float3(0.0, 0.0, 1.0), N)
                                        : cross(float3(1.0, 0.0, 0.0), N));
    float3 B = cross(N, T);

    float3 bent = float3(0.0);
    float  occ  = 0.0;
    for (int i = 0; i < kAORays; i++) {
        float rr = sqrt((float(i) + 0.5) / float(kAORays));
        float th = float(i) * 2.3999632;              // golden angle
        float3 d = T * (rr * cos(th)) + B * (rr * sin(th))
                 + N * sqrt(max(0.0, 1.0 - rr * rr));

        float3 ro = offset_ray(p, geo_n, d, t);
        if (any_hit_instances(make_ray(ro, d), radius, nodes, tris, instances, u))
            occ += 1.0;
        else
            bent += d;      // this direction sees sky
    }

    r.ao = 1.0 - occ / float(kAORays);
    r.bent_normal = (length(bent) > 1e-4) ? normalize(bent) : N;
    return r;
}

// Hemisphere-integrated sky, rather than a single sky_color(, u) lookup along the
// normal. Surfaces facing up see the zenith, surfaces facing down see the
// ground bounce, and everything between blends.
float3 sky_irradiance(float3 N) {
    // Open-air atrium: the sky is the dominant light source for everything the
    // sun cannot reach, so it has to carry real energy rather than act as a
    // token fill. Warmed slightly toward the ground bounce off the stone.
    float3 zenith = float3(0.62, 0.80, 1.15);
    float3 ground = float3(0.52, 0.44, 0.34);
    return mix(ground, zenith, N.y * 0.5 + 0.5) * 1.35;
}

// Sun visibility for a mesh surface: one long any-hit ray. This is what carves
// the light shafts down the arcade and gives the scene direction.
float sun_shadow(float3 p, float3 geo_n, float t,
                 device const BVHNode* nodes, device const TriPos* tris,
                 device const Instance* instances,
                 constant Uniforms& u) {
    if (u.enable_triangles == 0 || u.num_bvh_nodes == 0) return 1.0;
    float3 ro = offset_ray(p, geo_n, kSunDir, t);
    return any_hit_instances(make_ray(ro, kSunDir), 200.0, nodes, tris, instances, u)
           ? 0.0 : 1.0;
}

float3 calc_gi(float3 p, float3 n, device const Material* materials, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, device const Cylinder* cylinders, device const Cone* cones, device const Torus* tori, device const Disk* disks, constant Uniforms& u) {
    float3 gi = float3(0.0);
    for (int i = 0; i < u.num_planes; i++) {
        if (planes[i].normal.y > 0.9) {
            Material fm = materials[planes[i].mat_index];
            float3 fcol = fm.albedo;
            if (fm.type == CHECKERBOARD) {
                int cs = 4;
                bool white = (int(floor(p.x * 0.1 * float(cs))) + int(floor(p.z * 0.1 * float(cs)))) % 2 == 0;
                fcol = white ? fm.albedo : fm.albedo2;
            }
            gi += fcol * max(0.0, n.y) * 0.5;
        }
    }
    for (int i = 0; i < u.num_spheres; i++) {
        float3 to_obj = spheres[i].center - p;
        float dist = length(to_obj);
        if (dist < 0.1 || dist > 6.0) continue;
        float3 L = to_obj / dist;
        float NdotL = max(0.0, dot(n, L));
        if (NdotL <= 0.0) continue;
        Material om = materials[spheres[i].mat_index];
        if (om.type == EMISSIVE || om.type == GLASS) continue;
        float atten = 1.0 / (dist * dist + 0.1);
        gi += om.albedo * NdotL * atten * 1.5;
    }
    for (int i = 0; i < u.num_cubes; i++) {
        float3 to_obj = cubes[i].center - p;
        float dist = length(to_obj);
        if (dist < 0.1 || dist > 6.0) continue;
        float3 L = to_obj / dist;
        float NdotL = max(0.0, dot(n, L));
        if (NdotL <= 0.0) continue;
        Material om = materials[cubes[i].mat_index];
        if (om.type == EMISSIVE || om.type == GLASS) continue;
        float atten = 1.0 / (dist * dist + 0.1);
        gi += om.albedo * NdotL * atten * 1.5;
    }
    return gi;
}

// Depth- and normal-aware upsample of the half-resolution AO. Weighting each tap
// by how well its depth and orientation match this pixel keeps occlusion from
// bleeding across silhouettes, and blending four taps turns AO's six discrete
// levels into a continuous gradient.
float4 upsample_ao(texture2d<float, access::sample> aoTex,
                  texture2d<float, access::sample> giNormTex,
                  texture2d<float, access::sample> fogDepthTex,
                  float2 uv, float center_dist, float3 center_n,
                  constant Uniforms& u) {
    constexpr sampler pt(coord::normalized, filter::nearest, address::clamp_to_edge);

    float2 dim   = float2(u.fog_width, u.fog_height);
    float2 texel = 1.0 / dim;
    float2 fpx   = uv * dim - 0.5;
    float2 base  = floor(fpx);
    float2 frac  = fpx - base;

    float4 sum = float4(0.0); float wsum = 0.0;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 2; i++) {
            float2 tuv = (base + float2(i, j) + 0.5) * texel;
            float  bw  = (i == 0 ? 1.0 - frac.x : frac.x) *
                         (j == 0 ? 1.0 - frac.y : frac.y);
            float4 tap = aoTex.sample(pt, tuv);
            float3 tn  = giNormTex.sample(pt, tuv).xyz;
            float  d   = fogDepthTex.sample(pt, tuv).x;
            float  dw  = exp(-abs(d - center_dist) / max(0.03 * center_dist, 0.03));
            float  nw  = max(0.0, dot(tn, center_n));
            nw = nw * nw * nw;
            float  w   = bw * dw * nw;
            sum  += tap * w;
            wsum += w;
        }
    }
    return (wsum < 1e-4) ? float4(0.0, 0.0, 0.0, 1.0) : sum / wsum;
}

float3 trace_ray(Ray ray, device const Material* materials, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, device const Cylinder* cylinders, device const Cone* cones, device const Torus* tori, device const Disk* disks, device const Light* lights,
                 device const BVHNode* bvh_nodes, device const TriPos* triangles,
                 device const TriAttr* tri_attrs,
                 device const Instance* instances,
                 device const Material* mesh_mats,
                 texture2d_array<float, access::sample> mesh_textures,
                 texture2d_array<float, access::sample> mesh_orm,
                 texture2d<float, access::sample> aoTex,
                 texture2d<float, access::sample> giNormTex,
                 texture2d<float, access::sample> fogDepthTex,
                 float2 screen_uv,
                 constant Uniforms& u, thread float& first_dist,
                 thread int& first_inst) {

    // The primary hit distance (used by the fog term) is taken from the first
    // iteration of the loop below rather than from a second, identical
    // find_closest() call - that duplicate doubled the cost of every primary ray.
    first_dist = 60.0;
    first_inst = -1;
    bool first_iteration = true;

    float3 result = float3(0.0);
    Ray stack_ray[MAX_STACK];
    float3 stack_contrib[MAX_STACK];
    int stack_depth[MAX_STACK];
    int sp = 0;

    stack_ray[0] = ray;
    stack_contrib[0] = float3(1.0);
    stack_depth[0] = u.max_depth;
    sp = 1;

    while (sp > 0) {
        sp--;
        Ray cur = stack_ray[sp];
        float3 contrib = stack_contrib[sp];
        int depth = stack_depth[sp];
        if (depth <= 0) continue;

        // Russian roulette: probabilistically terminate low-energy rays instead
        // of evaluating them. Max channel luminance is the survival probability.
        // The primary ray (depth == u.max_depth) is never killed.
        if (depth < u.max_depth) {
            float survive = clamp(max(max(contrib.x, contrib.y), contrib.z), 0.0f, 1.0f);
            if (survive < 0.05f) continue;   // kill rays that contribute < 5%
        }

        HitInfo hit = find_closest(cur, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);

        if (first_iteration) {
            first_iteration = false;
            if (hit.hit) { first_dist = hit.t; first_inst = hit.inst_id; }
        }

        if (!hit.hit) {
            result += contrib * sky_color(cur.direction, u);
            continue;
        }

        Material mat;
        if (hit.is_mesh) {
            mat = mesh_mats[hit.mat_index];
            if (mat.type != EMISSIVE) {
                // Trilinear + mip. The old path point-sampled level 0 through
                // access::read, which aliased violently on any surface not
                // facing the camera head-on.
                constexpr sampler mesh_sampler(coord::normalized,
                                               filter::linear,
                                               mip_filter::linear,
                                               address::repeat,
                                               max_anisotropy(4));

                // Pixel cone footprint at this hit, widened at grazing angles.
                float cone_w  = hit.t * (2.0f * u.tan_half_fov / max(u.screen_height, 1.0f));
                float grazing = max(abs(dot(hit.normal, cur.direction)), 0.1f);
                float lod_base = log2(max(cone_w * hit.uv_density / grazing, 1e-9f));

                float2 uv = float2(hit.uv.x, hit.uv.y);
                float4 tex_color = mesh_textures.sample(mesh_sampler, uv, hit.mat_index,
                                                        level(max(lod_base + log2(u.mesh_tex_dim), 0.0f)));
                mat.albedo = tex_color.xyz;

                // Alpha. Sponza's grime and banner decals are separate polygons
                // laid over the walls, ~70% transparent, meant to blend. Ignoring
                // alpha drew them as opaque grey slabs that hid the stonework
                // behind - the flat polygonal patches on the walls. Continuing
                // the ray with the complementary weight composites them properly;
                // the existing ray stack already does exactly this job.
                // Alpha, but only for materials the loader confirmed actually
                // carry transparency. Applying this to every mesh material made
                // solid walls see-through wherever their alpha channel happened
                // to hold zeros.
                if (mat.flags & MATFLAG_ALPHA_BLEND) {
                    float alpha = tex_color.w;
                    float3 cont_o = hit.point + cur.direction * max(1e-3f, hit.t * 2e-3f);

                    // A decal is not a bounce, so compositing through it must not
                    // spend the reflection budget. Charging it one meant the depth
                    // ran out on stacked grime and the ray was dropped, which read
                    // as hard black patches. The stack size bounds the recursion
                    // instead, and the ray advances past the hit every time, so it
                    // always terminates.
                    if (alpha < 0.995f) {
                        if (sp < MAX_STACK) {
                            stack_ray[sp]     = make_ray(cont_o, cur.direction);
                            stack_contrib[sp] = contrib * (1.0f - alpha);
                            stack_depth[sp]   = depth;
                            sp++;
                        }
                        if (alpha < 0.02f) continue;   // nothing visible here
                        contrib *= alpha;
                    }
                }

                // Per-texel roughness/metallic. Without this the glTF factors
                // (1,1 by default) made every surface a rough mirror.
                if (mat.flags & MATFLAG_HAS_ORM_TEX) {
                    float4 orm = mesh_orm.sample(mesh_sampler, uv, hit.mat_index,
                                                 level(max(lod_base + log2(u.orm_tex_dim), 0.0f)));
                    mat.roughness = orm.y;
                    mat.metallic  = orm.z;
                }
            }
        } else {
            mat = materials[hit.mat_index];
        }
        
        result += contrib * mat.emission;
        if (mat.type == EMISSIVE) continue;

        // Procedural surfaces resolve here, before any shading reads albedo /
        // roughness / metallic, so the pattern drives the BRDF rather than being
        // painted on top of it.
        if (mat.proc_id != PROC_NONE) {
            float3 lp = hit.point - hit.obj_origin;
            float3 pa = mat.albedo;
            float  pr = mat.roughness, pm = mat.metallic;
            apply_procedural(mat.proc_id, lp, pa, pr, pm);
            mat.albedo = pa; mat.roughness = pr; mat.metallic = pm;
        }

        float3 N = hit.normal;
        float3 V = normalize(cur.origin - hit.point);
        if (dot(N, V) < 0.0) N = -N; // Flip normal if hitting backface
        float3 alb = mat.albedo;

        if (mat.type == CHECKERBOARD) {
            int cs = 4;
            bool white = (int(floor(hit.uv.x * float(cs))) + int(floor(hit.uv.y * float(cs)))) % 2 == 0;
            alb = white ? mat.albedo : mat.albedo2;
        }

        if (mat.type == WATER) {
            float3 wp = hit.point;
            float tm = u.time;

            float w1 = sin(wp.x * 8.0 + tm * 2.0) * 0.012;
            float w2 = cos(wp.z * 7.0 + tm * 2.5) * 0.012;
            float w3 = sin((wp.x * 0.7 + wp.z * 0.9) * 14.0 + tm * 3.5) * 0.006;
            float w4 = cos(wp.x * 3.0 - wp.z * 4.0 + tm * 1.8) * 0.008;
            float wave_h = w1 + w2 + w3 + w4;

            float dx = 8.0 * cos(wp.x * 8.0 + tm * 2.0) * 0.012
                     + 14.0 * 0.7 * cos((wp.x * 0.7 + wp.z * 0.9) * 14.0 + tm * 3.5) * 0.006
                     + 3.0 * -sin(wp.x * 3.0 - wp.z * 4.0 + tm * 1.8) * 0.008;
            float dz = 7.0 * -sin(wp.z * 7.0 + tm * 2.5) * 0.012
                     + 14.0 * 0.9 * cos((wp.x * 0.7 + wp.z * 0.9) * 14.0 + tm * 3.5) * 0.006
                     + 4.0 * sin(wp.x * 3.0 - wp.z * 4.0 + tm * 1.8) * 0.008;

            float3 water_n = normalize(float3(-dx, 1.0, -dz));

            float cos_theta = max(0.0, dot(water_n, V));
            float R0 = 0.02;
            float fresnel = R0 + (1.0 - R0) * pow(1.0 - cos_theta, 5.0);
            fresnel = clamp(fresnel, 0.0, 1.0);

            if (fresnel > EPSILON && sp < MAX_STACK) {
                float3 R = reflect(cur.direction, water_n);
                stack_ray[sp] = make_ray(hit.point + water_n * 0.02, R);
                stack_contrib[sp] = contrib * fresnel;
                stack_depth[sp] = depth - 1;
                sp++;
            }

            if ((1.0 - fresnel) > EPSILON && sp < MAX_STACK) {
                float n1 = 1.0, n2 = 1.33;
                float3 Nf = water_n; 
                float cos_i = -dot(cur.direction, Nf);
                if (cos_i < 0.0) { float tmp = n1; n1 = n2; n2 = tmp; Nf = -Nf; cos_i = -dot(cur.direction, Nf); }
                float eta = n1 / n2;
                float k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
                if (k >= 0.0) {
                    float3 T = eta * cur.direction + (eta * cos_i - sqrt(k)) * Nf;
                    float view_depth = 1.0 / max(dot(Nf, -T), 0.15);
                    float3 water_absorb = float3(0.25, 0.18, 0.08); 
                    float3 absorption = exp(-water_absorb * view_depth * 1.2);

                    stack_ray[sp] = make_ray(hit.point - Nf * 0.05, T);
                    stack_contrib[sp] = contrib * (1.0 - fresnel) * absorption;
                    stack_depth[sp] = depth - 1;
                    sp++;
                }
            }

            float3 spec = float3(0.0);
            for (int i = 0; i < u.num_lights; i++) {
                float sh = calc_analytic_shadow(hit.point, water_n, hit.geo_normal, hit.t, lights[i].position, lights[i].radius, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);
                float3 to_light = lights[i].position - hit.point;
                float dist = length(to_light);
                float3 L = to_light / dist;
                float3 H = normalize(L + V);
                float NdotH = max(0.0, dot(water_n, H));
                spec += lights[i].color * pow(NdotH, 800.0) * lights[i].intensity * sh * fresnel / (dist * dist + 0.1);
            }
            result += contrib * spec;

            float foam = smoothstep(0.025, 0.032, wave_h) * 0.08;
            float3 foam_col = float3(0.97, 0.98, 1.0);
            result += contrib * foam_col * foam * (1.0 - fresnel);

            continue;
        }

        if (mat.type != GLASS) {
            // Metallic workflow: dielectrics keep a 4% specular and their full
            // albedo as diffuse; metals tint the specular with albedo and have
            // no diffuse at all. Driving both from `metallic` replaces the old
            // hard DIFFUSE/METAL branch, which had no diffuse term for anything
            // classified as metal - the reason textured Sponza read as grey.
            float  metallic  = clamp(mat.metallic, 0.0f, 1.0f);
            float  roughness = clamp(mat.roughness, 0.03f, 1.0f);
            float3 F0        = mix(float3(0.04), alb, metallic);
            float3 diffuse_albedo = alb * (1.0 - metallic);

            float3 direct = float3(0.0);

            for (int i = 0; i < u.num_lights; i++) {
                float3 to_light = lights[i].position - hit.point;
                float dist_sq = dot(to_light, to_light);
                float dist = sqrt(dist_sq);
                float3 L = to_light / dist;
                float NdotL = max(0.0, dot(N, L));
                if (NdotL <= 0.0) continue;   // shadow ray would be wasted

                float sh = calc_analytic_shadow(hit.point, N, hit.geo_normal, hit.t, lights[i].position, lights[i].radius, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);
                if (sh <= 0.0) continue;

                float atten = lights[i].intensity / max(dist_sq, 0.01f);
                float3 H = normalize(L + V);
                float NdotH = max(0.0, dot(N, H));
                float VdotH = max(0.0, dot(V, H));

                // Blinn-Phong specular with roughness falloff
                float shininess = pow(2.0, (1.0 - roughness) * 11.0);
                float norm      = (shininess + 8.0) / 25.13274;   // /(8*pi)
                float3 F        = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

                float3 diff = diffuse_albedo * NdotL;
                float3 spec = F * pow(NdotH, shininess) * norm * NdotL * (1.0 - roughness * 0.7);
                direct += (diff + spec) * lights[i].color * atten * sh;
            }

            float ao;
            float3 indirect;
            float3 bounce = float3(0.0);

            if (hit.is_mesh) {
                // Sun + sky + traced AO. Scale the AO radius with the model so
                // it stays meaningful whatever the import scale.
                // Primary hits read the half-resolution AO buffer; secondary
                // bounces are rare enough in practice to trace directly, and
                // screen-space AO would not be valid for them anyway.
                float4 gi = (depth == u.max_depth)
                    ? upsample_ao(aoTex, giNormTex, fogDepthTex, screen_uv, hit.t, N, u)
                    : float4(0.0, 0.0, 0.0,
                             trace_ao(hit.point, N, hit.geo_normal, hit.t, 1.2,
                                      bvh_nodes, triangles, instances, u));
                ao = gi.w;
                bounce = gi.xyz;

                float sun_ndl = max(0.0, dot(N, kSunDir));
                if (sun_ndl > 0.0) {
                    float vis = sun_shadow(hit.point, hit.geo_normal, hit.t, bvh_nodes, triangles, instances, u);
                    if (vis > 0.0) {
                        float3 H = normalize(kSunDir + V);
                        float NdotH = max(0.0, dot(N, H));
                        float VdotH = max(0.0, dot(V, H));
                        float shininess = pow(2.0, (1.0 - roughness) * 11.0);
                        float norm = (shininess + 8.0) / 25.13274;
                        float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
                        float3 sun_spec = F * pow(NdotH, shininess) * norm * (1.0 - roughness * 0.7);
                        direct += (diffuse_albedo + sun_spec)
                                * sun_ndl * kSunColor * kSunIntensity * vis;
                    }
                }
                float3 bent = (length(bounce) > 1e-3) ? normalize(bounce) : N;
                indirect = sky_irradiance(bent) * ao;
            } else {
                ao = calc_analytic_ao(hit.point, N, spheres, cubes, u);
                float3 sky_ambient = sky_color(N, u) * u.ambient_light * 0.5;
                indirect = (sky_ambient + calc_gi(hit.point, N, materials, spheres, planes, cubes, cylinders, cones, tori, disks, u)) * ao;
            }

            direct += indirect * diffuse_albedo;
            // Ambient specular: only polished or metallic surfaces have sharp specular reflections.
            float ambient_spec_power = pow(max(0.0f, 1.0f - roughness), 4.0f);
            direct += sky_color(reflect(cur.direction, N), u) * F0 * (ao * ambient_spec_power * 0.5f);
            result += contrib * direct;

            // Spawn a recursive mirror ray ONLY for genuine reflective surfaces (metals or polished gloss):
            float3 spec_weight = F0 * pow(max(0.0f, 1.0f - roughness), 3.0f);
            float3 next_contrib = contrib * spec_weight;
            if (sp < MAX_STACK && depth > 1 && (metallic > 0.4f || roughness < 0.15f) &&
                max(max(next_contrib.x, next_contrib.y), next_contrib.z) > 0.02f) {
                stack_ray[sp] = make_ray(offset_ray(hit.point, hit.geo_normal, reflect(cur.direction, N), hit.t), reflect(cur.direction, N));
                stack_contrib[sp] = next_contrib;
                stack_depth[sp] = depth - 1;
                sp++;
            }
        }

        if (mat.type == GLASS && sp < MAX_STACK) {
            // Use the unflipped geometric normal so the entering/exiting test works
            float n1 = 1.0, n2 = mat.refractive_index;
            float3 Nf = hit.normal; float cos_i = -dot(cur.direction, Nf);
            if (cos_i < 0.0) { float tmp = n1; n1 = n2; n2 = tmp; Nf = -Nf; cos_i = -cos_i; }

            float eta = n1 / n2;
            float k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
            // Total internal reflection: all energy goes to the reflected ray
            float fresnel_r = (k < 0.0) ? 1.0 : schlick(cos_i, n1, n2).x;

            if (fresnel_r > EPSILON && sp < MAX_STACK) {
                stack_ray[sp] = make_ray(hit.point + Nf * 3e-3, reflect(cur.direction, Nf));
                stack_contrib[sp] = contrib * fresnel_r;
                stack_depth[sp] = depth - 1;
                sp++;
            }
            if (k >= 0.0 && (1.0 - fresnel_r) > EPSILON && sp < MAX_STACK) {
                float3 T = eta * cur.direction + (eta * cos_i - sqrt(k)) * Nf;
                stack_ray[sp] = make_ray(hit.point - Nf * 5e-3, T);
                stack_contrib[sp] = contrib * (1.0 - fresnel_r);
                stack_depth[sp] = depth - 1;
                sp++;
            }
        }
    }

    return clamp(result, float3(0.0), float3(100.0));
}

// ---------------------------------------------------------------- volumetric fog
//
// Fog is the single most expensive term in the renderer: every march step used to
// fire a shadow ray per light, and with a mesh loaded each of those is a full BVH
// any-hit traversal. At 16 steps and 2 lights that is 32 traversals *per pixel*,
// which measured at ~76% of the whole frame inside Sponza.
//
// The medium is low-frequency, so it is now marched in its own kernel at a
// fraction of the ray resolution (see kFogScale on the host) and bilinearly
// upsampled during compositing. Returns (inscatter.rgb, transmittance).
// Forward declarations: the mesh lighting block is defined above the fog
// march but the fog needs the sun constants, which are file scope.
float4 march_fog(float3 origin, float3 dir, float primary_dist, float jitter,
                 device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, device const Cylinder* cylinders, device const Cone* cones, device const Torus* tori, device const Disk* disks, device const Light* lights,
                 device const BVHNode* bvh_nodes, device const TriPos* triangles,
                 device const TriAttr* tri_attrs,
                 device const Instance* instances,
                 constant Uniforms& u) {
    int   steps     = max(4, min(u.fog_steps, 32));
    float max_dist  = min(primary_dist, 60.0f);
    float step_size = max_dist / float(steps);

    float3 vol_color     = float3(0.0);
    float  transmittance = 1.0;
    float3 sky_ambient   = sky_color(dir, u);

    float sun_cos = max(0.0, dot(dir, kSunDir));
    float sun_phase = 0.15 + 0.85 * pow(sun_cos, 8.0);

    for (int i = 0; i < steps; i++) {
        float3 sample_p = origin + dir * ((float(i) + jitter) * step_size);
        float  h        = sample_p.y + 1.0;
        float  density  = u.fog_density * exp(-max(0.0f, h) * 0.8f) + u.fog_density * 0.05f;

        float3 step_scattering = float3(0.0);
        if (density > 0.0001f) {
            if (u.enable_triangles > 0) {
                float vis = any_hit_instances(make_ray(sample_p, kSunDir), 200.0,
                                              bvh_nodes, triangles, instances, u)
                            ? 0.0 : 1.0;
                step_scattering += kSunColor * (kSunIntensity * 0.5f * sun_phase * vis);
            }
            for (int l = 0; l < u.num_lights; l++) {
                float3 to_light = lights[l].position - sample_p;
                float  dist     = length(to_light);
                float3 l_dir    = to_light / max(dist, 0.01f);
                float  sh = calc_analytic_shadow(sample_p, float3(0.0, 1.0, 0.0),
                                                 float3(0.0, 1.0, 0.0), 0.0,
                                                 lights[l].position, lights[l].radius,
                                                 spheres, planes, cubes, cylinders, cones, tori, disks,
                                                 bvh_nodes, triangles, tri_attrs, instances, u);
                float3 light_intensity = lights[l].color * lights[l].intensity / (dist * dist + 0.1);
                float  l_cos = max(0.0, dot(dir, l_dir));
                float  point_phase = 0.15 + 0.35 * pow(l_cos, 4.0);
                step_scattering += light_intensity * point_phase * sh;
            }
            step_scattering += sky_ambient * 0.7f;
        }

        float  extinction     = exp(-density * step_size);
        float3 step_inscatter = step_scattering * (1.0 - extinction);

        vol_color     += transmittance * step_inscatter;
        transmittance *= extinction;

        if (transmittance < 0.01) break;
    }

    return float4(vol_color, transmittance);
}

// Closest-hit distance only - no shading. Used by the fog kernel to bound its
// march without paying for a full trace_ray().
//
// The fog kernel runs at a fraction of the ray resolution and already resolves a
// primary hit, so ambient occlusion is computed there too and upsampled during
// compositing. Tracing AO per full-resolution pixel cost 13.6 ms of a 25 ms
// frame; at half resolution that is a quarter of the rays, and the upsample
// filter also smooths the banding that only six sample directions produce.
float primary_distance(Ray ray,
                       device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, device const Cylinder* cylinders, device const Cone* cones, device const Torus* tori, device const Disk* disks, device const BVHNode* bvh_nodes,
                       device const TriPos* triangles,
                 device const TriAttr* tri_attrs,
                 device const Instance* instances, constant Uniforms& u) {
    HitInfo h = find_closest(ray, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);
    return h.hit ? h.t : 60.0f;
}

kernel void fog_kernel(texture2d<float, access::write> outFog [[texture(0)]],
                       texture2d<float, access::write> outFogDepth [[texture(1)]],
                       texture2d<float, access::write> outAO [[texture(2)]],
                       texture2d<float, access::write> outGINorm [[texture(3)]],
                       texture2d_array<float, access::sample> mesh_textures [[texture(4)]],
                       device const Sphere*   spheres   [[buffer(1)]],
                       device const Plane*    planes    [[buffer(2)]],
                       device const Cube*     cubes     [[buffer(3)]],
                       device const Light*    lights    [[buffer(5)]],
                       constant Uniforms&     u         [[buffer(6)]],
                       device const TriPos*   triangles [[buffer(7)]],
                       device const BVHNode*  bvh_nodes [[buffer(8)]],
                       device const Material* mesh_mats [[buffer(9)]],
                       device const TriAttr*  tri_attrs [[buffer(10)]],
                       device const Instance* instances [[buffer(11)]],
                       device const Cylinder* cylinders [[buffer(12)]],
                       device const Cone*     cones     [[buffer(13)]],
                       device const Torus*    tori      [[buffer(14)]],
                       device const Disk*     disks     [[buffer(15)]],
                       uint2 gid [[thread_position_in_grid]]) {

    if (gid.x >= uint(u.fog_width) || gid.y >= uint(u.fog_height)) return;

    float2 px     = float2(gid) + 0.5;
    float  py_inv = u.fog_height - px.y;

    float nx = (2.0 * px.x     / u.fog_width  - 1.0) * u.aspect_ratio * u.tan_half_fov;
    float ny = (2.0 * py_inv   / u.fog_height - 1.0) * u.tan_half_fov;
    float3 dir = normalize(u.camera_forward + nx * u.camera_right + ny * u.camera_up);

    Ray r = make_ray(u.camera_origin, dir);

    // One primary hit serves both the fog bound and the AO probe.
    HitInfo ph = find_closest(r, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);
    float fd = ph.hit ? ph.t : 60.0f;

    // AO plus the normal it was computed for: the composite needs the normal to
    // reject taps that belong to a differently-oriented surface.
    float3 irr = float3(0.0);
    float  ao = 1.0;
    float3 ao_n = float3(0.0, 1.0, 0.0);
    if (ph.hit && ph.is_mesh) {
        ao_n = ph.normal;
        GIResult g = trace_gi(ph.point, ph.normal, ph.geo_normal, ph.t, 1.5,
                              bvh_nodes, triangles, instances, mesh_mats,
                              mesh_textures, u);
        irr = g.bent_normal;
        ao  = g.ao;
    }
    outAO.write(float4(irr, ao), gid);
    outGINorm.write(float4(ao_n, 0.0), gid);

    // Deterministic ordered offset rather than a random one: this renderer is
    // deliberately noise-free, and an ordered pattern removes step banding
    // without introducing a grain that then has to be filtered back out.
    // Fog is optional; the ambient-occlusion / visibility data this pass also
    // produces is not. Gating the whole kernel on enable_fog meant switching fog
    // off left the AO buffer stale and the shading lost its occlusion entirely.
    float4 fog = float4(0.0, 0.0, 0.0, 1.0);   // no inscatter, full transmittance
    if (u.enable_fog > 0) {
        float jitter = kBayer4[(gid.y & 3) * 4 + (gid.x & 3)];
        fog = march_fog(u.camera_origin, dir, fd, jitter,
                        spheres, planes, cubes, cylinders, cones, tori, disks, lights, bvh_nodes, triangles, tri_attrs, instances, u);
    }
    outFog.write(fog, gid);
    // Depth of the fog texel, so the composite can reject taps that belong to a
    // different surface and avoid haloing along silhouettes.
    outFogDepth.write(float4(fd, 0.0, 0.0, 0.0), gid);
}

// Depth-aware ("bilateral") upsample of the low-resolution fog. Plain bilinear
// would bleed bright shafts across object silhouettes; weighting each of the four
// taps by how well its primary distance matches this pixel's keeps edges clean,
// so the resolution reduction costs speed only, not quality.
float4 upsample_fog(texture2d<float, access::sample> fogTex,
                    texture2d<float, access::sample> fogDepthTex,
                    float2 uv, float center_dist, constant Uniforms& u) {
    constexpr sampler pt(coord::normalized, filter::nearest, address::clamp_to_edge);
    constexpr sampler bilin(coord::normalized, filter::linear, address::clamp_to_edge);

    float2 fog_dim = float2(u.fog_width, u.fog_height);
    float2 texel   = 1.0 / fog_dim;
    float2 fpx     = uv * fog_dim - 0.5;
    float2 base    = floor(fpx);
    float2 frac    = fpx - base;

    float4 sum = float4(0.0);
    float  wsum = 0.0;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 2; i++) {
            float2 tuv = (base + float2(i, j) + 0.5) * texel;
            float  bw  = (i == 0 ? 1.0 - frac.x : frac.x) *
                         (j == 0 ? 1.0 - frac.y : frac.y);
            float  d   = fogDepthTex.sample(pt, tuv).x;
            // Relative depth difference: tolerant far away, strict up close.
            float  dw  = exp(-abs(d - center_dist) / max(0.05 * center_dist, 0.05));
            float  w   = bw * dw;
            sum  += fogTex.sample(pt, tuv) * w;
            wsum += w;
        }
    }
    // All four taps rejected (thin geometry): fall back to plain bilinear.
    if (wsum < 1e-4) return fogTex.sample(bilin, uv);
    return sum / wsum;
}

// ACES tonemapping - RRT + ODT fit by Stephen Hill.
// Pre-exposed for a slightly lifted shadow floor (cinematic look, avoids
// pure-black crushed shadows that look too CG).
float3 aces_approx(float3 v) {
    v *= 0.62f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    float3 t = clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);

    // Cinematic S-curve: compress highlights further (film roll-off),
    // lift blacks slightly so shadows read warm and not crushed.
    // f(x) = x^(1 / (1 + x*0.18)) is a simple film-shoulder curve.
    t = pow(t, float3(1.0f / (1.0f + t * 0.20f)));

    // Subtle warm shadow tint + cool highlight desaturation (film print look)
    float lum = dot(t, float3(0.2126f, 0.7152f, 0.0722f));
    float3 shadow_tint    = float3(1.010f, 1.000f, 0.985f);   // barely warm
    float3 highlight_cool = float3(0.995f, 0.998f, 1.005f);   // barely cool
    float  shadow_w = clamp(1.0f - t.x * 3.0f, 0.0f, 1.0f) *
                      clamp(1.0f - t.y * 3.0f, 0.0f, 1.0f) *
                      clamp(1.0f - t.z * 3.0f, 0.0f, 1.0f);
    float  hi_w     = clamp(lum * lum * 2.0f, 0.0f, 1.0f);
    t *= mix(float3(1.0f), shadow_tint,    shadow_w * 0.6f);
    t *= mix(float3(1.0f), highlight_cool, hi_w     * 0.35f);

    return clamp(t, 0.0f, 1.0f);
}

// ---------------------------------------------------------------- ground grid
//
// The editor grid, drawn by the tracer rather than painted over the finished
// image, so geometry occludes it correctly and it takes the same fog and
// exposure as everything else.
//
// A compute kernel has no fwidth, so the pixel footprint is derived instead:
// at distance t a pixel covers t * 2 * tan_half / screen_height world units.
// That is what makes the lines a constant thickness on screen and what lets the
// spacing step by decades without shimmering.
struct GridSample { float3 color; float alpha; };

GridSample sample_grid(float3 p, float footprint, constant Uniforms& u) {
    GridSample out;
    out.color = float3(u.grid_color);
    out.alpha = 0.0;

    float step0, step1, blend;
    if (u.grid_auto_scale != 0) {
        float decade = max(0.0, log10(footprint * 28.0));
        step0 = pow(10.0, floor(decade));
        step1 = step0 * 10.0;
        blend = fract(decade);
    } else {
        step0 = max(u.grid_spacing, 0.05);
        step1 = step0 * 10.0;
        blend = 0.0;
    }

    // Half-width of lines in world units for crisp anti-aliased lines
    float line_w = max(footprint * 0.70, 0.004);

    // Distance to the nearest line of minor and major spacings
    float2 d0 = abs(fract(p.xz / step0 + 0.5) - 0.5) * step0;
    float2 d1 = abs(fract(p.xz / step1 + 0.5) - 0.5) * step1;

    float2 l0_2d = 1.0 - smoothstep(float2(0.0), float2(line_w * 1.5), d0);
    float l0 = max(l0_2d.x, l0_2d.y);

    float2 l1_2d = 1.0 - smoothstep(float2(0.0), float2(line_w * 1.8), d1);
    float l1 = max(l1_2d.x, l1_2d.y);

    // Major lines are slightly brighter than minor lines
    float3 base_col  = float3(u.grid_color);
    float3 major_col = mix(base_col, float3(0.9, 0.92, 0.95), 0.35);

    out.alpha = max(l1 * 0.80, l0 * 0.45 * (1.0 - blend));
    out.color = (l1 > l0 * (1.0 - blend)) ? major_col : base_col;

    // Origin Axes: X-axis (Red) along Z=0, Z-axis (Blue) along X=0
    float ax = 1.0 - smoothstep(0.0, line_w * 1.8, abs(p.z));
    float az = 1.0 - smoothstep(0.0, line_w * 1.8, abs(p.x));

    if (ax > 0.02) {
        out.color = mix(out.color, float3(u.grid_axis_x), ax);
        out.alpha = max(out.alpha, ax * 0.95);
    }
    if (az > 0.02) {
        out.color = mix(out.color, float3(u.grid_axis_z), az);
        out.alpha = max(out.alpha, az * 0.95);
    }

    return out;
}

// Composites the grid over an already-shaded pixel. `surface_t` is the distance
// to whatever the ray hit, so the grid is hidden behind geometry rather than
// drawn on top of it.
float3 composite_grid(float3 color, float3 origin, float3 dir, float surface_t,
                      float tan_half, constant Uniforms& u) {
    if (u.grid_enabled == 0) return color;
    if (abs(dir.y) < 1e-5) return color;

    float t = -origin.y / dir.y;
    if (t <= 0.0 || t >= surface_t) return color;

    float3 p = origin + dir * t;

    // Filter footprint with grazing angle correction to eliminate horizon aliasing
    float ray_cos = max(abs(dir.y), 0.035);
    float footprint = (t * 2.0 * tan_half / max(u.screen_height, 1.0)) / ray_cos;

    GridSample g = sample_grid(p, footprint, u);
    if (g.alpha <= 0.001) return color;

    // Smooth fade with distance and grazing angle
    float fade = 1.0 - smoothstep(u.grid_fade * 0.25, u.grid_fade, t);
    float grazing = smoothstep(0.01, 0.08, abs(dir.y));

    return mix(color, g.color, clamp(g.alpha * fade * grazing * u.grid_opacity, 0.0, 1.0));
}

kernel void raytrace_kernel(texture2d<float, access::write> outTexture [[texture(0)]],
                            texture2d_array<float, access::sample> mesh_textures [[texture(1)]],
                            texture2d<float, access::write> outDepth [[texture(2)]],
                            texture2d<float, access::write> outMotion [[texture(3)]],
                            texture2d<float, access::sample> fogTexture [[texture(4)]],
                            texture2d<float, access::sample> fogDepthTexture [[texture(5)]],
                            texture2d_array<float, access::sample> mesh_orm [[texture(6)]],
                            texture2d<float, access::sample> aoTexture [[texture(7)]],
                            texture2d<float, access::sample> giNormTexture [[texture(8)]],
                            device const Material* materials [[buffer(0)]],
                            device const Sphere*   spheres   [[buffer(1)]],
                            device const Plane*    planes    [[buffer(2)]],
                            device const Cube*     cubes     [[buffer(3)]],
                            device const Light*    lights    [[buffer(5)]],
                            constant Uniforms&     u         [[buffer(6)]],
                            device const TriPos*   triangles [[buffer(7)]],
                            device const BVHNode*  bvh_nodes [[buffer(8)]],
                            device const Material* mesh_mats [[buffer(9)]],
                            device const TriAttr*  tri_attrs [[buffer(10)]],
                            device const Instance* instances [[buffer(11)]],
                            device const Cylinder* cylinders [[buffer(12)]],
                            device const Cone*     cones     [[buffer(13)]],
                            device const Torus*    tori      [[buffer(14)]],
                            device const Disk*     disks     [[buffer(15)]],
                            uint2 gid [[thread_position_in_grid]]) {

    if (gid.x >= uint(u.screen_width) || gid.y >= uint(u.screen_height)) return;

    float2 px = float2(gid);
    // -1 so that an offset of 0.5 lands on the pixel centre; without it every ray
    // was cast half a pixel high, which the temporal scaler then had to fight.
    float  py_inv = u.screen_height - px.y - 1.0;

    // Sub-pixel jitter. The host drives this from a Halton(2,3) sequence and
    // hands the same offset to MTLFXTemporalScaler, so successive frames sample
    // different points inside the pixel and the scaler resolves them into
    // genuine extra resolution instead of a blur.
    float2 jitter = float2(0.5 + u.jitter_x, 0.5 - u.jitter_y);


    float3 color      = float3(0.0);
    float3 center_dir  = float3(0.0);
    float  center_fd   = 60.0;
    int    center_inst = -1;
    int    SAMPLES    = u.samples_per_pixel;
    if (SAMPLES < 1) SAMPLES = 1;
    if (SAMPLES > 4) SAMPLES = 4;

    for (int dy = 0; dy < SAMPLES; dy++) {
        for (int dx = 0; dx < SAMPLES; dx++) {
            float2 offset = float2(float(dx) + jitter.x, float(dy) + jitter.y) / float(SAMPLES);
            float  nx = (2.0 * (px.x + offset.x) / u.screen_width  - 1.0) * u.aspect_ratio * u.tan_half_fov;
            float  ny = (2.0 * (py_inv + offset.y) / u.screen_height - 1.0) * u.tan_half_fov;

            float3 dir = normalize(u.camera_forward + nx * u.camera_right + ny * u.camera_up);
            Ray r = make_ray(u.camera_origin, dir);

            float fd = 60.0;
            int   fi = -1;
            color += trace_ray(r, materials, spheres, planes, cubes, cylinders, cones, tori, disks, lights,
                               bvh_nodes, triangles, tri_attrs, instances, mesh_mats, mesh_textures,
                               mesh_orm, aoTexture, giNormTexture, fogDepthTexture,
                               (float2(gid) + 0.5) / float2(u.screen_width, u.screen_height),
                               u, fd, fi);
            if (dx == 0 && dy == 0) { center_dir = dir; center_fd = fd; center_inst = fi; }
        }
    }
    color /= float(SAMPLES * SAMPLES);

    // Grid before fog and tone mapping, so it sits in the scene rather than on
    // the screen: distance haze and exposure apply to it like anything else.
    color = composite_grid(color, u.camera_origin, center_dir, center_fd, u.tan_half_fov, u);

    // Composite the low-resolution fog pass. Bilinear is enough for a medium this
    // smooth, and it turns 32 BVH traversals per pixel into 32 per fog texel.
    if (u.enable_fog > 0) {
        float2 uv  = (float2(gid) + 0.5) / float2(u.screen_width, u.screen_height);
        float4 fog = upsample_fog(fogTexture, fogDepthTexture, uv, center_fd, u);
        color = color * fog.w + fog.xyz;
    }

    // ---- Debug views. Judging shading from the beauty pass alone is guesswork;
    // these show the inputs directly. 1=albedo 2=normal 3=metallic 4=roughness
    // 5=depth 6=direct light only 7=ambient only.
    if (u.debug_mode > 0) {
        float3 dir = normalize(u.camera_forward
                     + ((2.0 * (px.x + 0.5) / u.screen_width  - 1.0) * u.aspect_ratio * u.tan_half_fov) * u.camera_right
                     + ((2.0 * (py_inv + 0.5) / u.screen_height - 1.0) * u.tan_half_fov) * u.camera_up);
        Ray dr = make_ray(u.camera_origin, dir);
        HitInfo h = find_closest(dr, spheres, planes, cubes, cylinders, cones, tori, disks, bvh_nodes, triangles, tri_attrs, instances, u);
        float3 dbg = float3(0.0);
        if (h.hit) {
            Material m = h.is_mesh ? mesh_mats[h.mat_index] : materials[h.mat_index];
            float2 duv = float2(h.uv.x, h.uv.y);
            constexpr sampler dbg_s(coord::normalized, filter::linear,
                                    mip_filter::linear, address::repeat);
            if (h.is_mesh) {
                float4 bc = mesh_textures.sample(dbg_s, duv, h.mat_index, level(0.0));
                float4 orm = mesh_orm.sample(dbg_s, duv, h.mat_index, level(0.0));
                if (m.flags & MATFLAG_HAS_ORM_TEX) { m.roughness = orm.y; m.metallic = orm.z; }
                m.albedo = bc.xyz;
            }
            if      (u.debug_mode == 1) dbg = m.albedo;
            else if (u.debug_mode == 2) dbg = h.normal * 0.5 + 0.5;
            else if (u.debug_mode == 3) dbg = float3(m.metallic);
            else if (u.debug_mode == 4) dbg = float3(m.roughness);
            else if (u.debug_mode == 5) dbg = float3(h.t / 20.0);
            else if (u.debug_mode == 6) {   // material index as a distinct hue
                float k = float(h.mat_index);
                dbg = fract(float3(k * 0.1031, k * 0.2237, k * 0.3319) + 0.13);
            }
            else if (u.debug_mode == 7) dbg = float3(fract(duv), 0.0);  // UV
            else if (u.debug_mode == 13)
                dbg = float3(float(h.mat_index) / 64.0);   // read back numerically
            else if (u.debug_mode == 12) {
                // Mip level the beauty pass would pick, normalised over the
                // chain. White == sampling the 1x1 top mip, i.e. flat average.
                float cone_w = h.t * (2.0f * u.tan_half_fov / max(u.screen_height, 1.0f));
                float grazing = max(abs(dot(h.normal, dir)), 0.1f);
                float lod = log2(max(cone_w * h.uv_density / grazing, 1e-9f))
                          + log2(u.mesh_tex_dim);
                float levels = log2(u.mesh_tex_dim) + 1.0;
                dbg = float3(clamp(lod, 0.0, levels) / levels);
            }
            else if (u.debug_mode == 11)
                dbg = (h.is_mesh && (h.mat_index < 0 || h.mat_index >= u.mesh_mat_count))
                      ? float3(1.0, 0.0, 0.0) : float3(0.0, 0.4, 0.0);
            else if (u.debug_mode == 9)
                dbg = float3(trace_ao(h.point, h.normal, h.geo_normal, h.t, 1.2, bvh_nodes, triangles, instances, u));
            else if (u.debug_mode == 10)
                dbg = float3(sun_shadow(h.point, h.geo_normal, h.t, bvh_nodes, triangles, instances, u)
                             * max(0.0, dot(h.normal, kSunDir)));
            else if (u.debug_mode == 8) {   // raw texture fetch, no LOD, no flags
                dbg = h.is_mesh ? mesh_textures.sample(dbg_s, duv, h.mat_index, level(0.0)).xyz
                                : float3(0.0, 1.0, 0.0);
            }
        } else {
            // Magenta = the primary ray hit nothing, to tell a genuinely black
            // surface apart from a hole in the geometry.
            dbg = (u.debug_mode == 5) ? float3(1.0) : float3(1.0, 0.0, 1.0);
        }
        outTexture.write(float4(pow(clamp(dbg, 0.0, 1.0), 1.0/2.2), 1.0), gid);
        outDepth.write(float4(0.5, 0.0, 0.0, 0.0), gid);
        outMotion.write(float4(0.0), gid);
        return;
    }

    float3 mapped = aces_approx(color);
    mapped = pow(clamp(mapped, float3(0.0), float3(1.0)), float3(1.0/2.2));
    outTexture.write(float4(mapped, 1.0), gid);

    // ---- MetalFX guidance buffers.
    //
    // These used to be written as constants (0.5 and 0), which left the temporal
    // scaler with no way to reproject history - so it could only ever blur, and
    // the host compensated by resetting history on every camera move, discarding
    // the accumulation entirely. Both halves are fixed: real depth and real
    // motion here, reset only on genuine discontinuities on the host.

    // Non-reversed [0,1] depth from view-space Z along the camera forward axis.
    constexpr float kNear = 0.05, kFar = 200.0;
    float view_z = max(center_fd * dot(center_dir, u.camera_forward), kNear);
    float depth  = (kFar * (view_z - kNear)) / (view_z * (kFar - kNear));
    outDepth.write(float4(clamp(depth, 0.0f, 1.0f), 0.0, 0.0, 0.0), gid);

    // Motion vector: where this surface sat in the previous frame, in input
    // pixels.
    //
    // Camera reprojection alone answers that only for geometry that did not
    // move. For an instance that did, the surface has to be carried back through
    // its own transform first: world -> instance-local with the current inverse,
    // then local -> world with the transform the instance had last frame. A
    // static instance has both equal, so this reduces to the camera-only case.
    float3 world_p = u.camera_origin + center_dir * center_fd;
    float3 prev_world_p = world_p;
    if (center_inst >= 0 && center_inst < u.num_instances) {
        device const Instance& inst = instances[center_inst];
        float3 local = xform_point(inst.w2l0, inst.w2l1, inst.w2l2, world_p);
        prev_world_p = xform_point(inst.p2w0, inst.p2w1, inst.p2w2, local);
    }
    float4 prev_clip = u.prev_view_proj * float4(prev_world_p, 1.0);
    float2 motion = float2(0.0);
    if (prev_clip.w > 1e-5) {
        float2 prev_ndc = prev_clip.xy / prev_clip.w;
        float2 prev_px  = float2(prev_ndc.x * 0.5 + 0.5, 0.5 - prev_ndc.y * 0.5)
                        * float2(u.screen_width, u.screen_height);
        motion = prev_px - (float2(gid) + 0.5);
    }
    outMotion.write(float4(motion, 0.0, 0.0), gid);
}

// Copies the traced image to the drawable.
//
// Without this the only path to the screen was MetalFX, so a machine without it
// - or a render scale of 1:1, where a temporal *upscaler* has nothing to
// upscale - presented a black window while the tracer worked perfectly into an
// offscreen texture. Linear sampling covers the upscale when the scaler is off.
kernel void present_kernel(texture2d<float, access::sample> src [[texture(0)]],
                           texture2d<float, access::write>  dst [[texture(1)]],
                           uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    constexpr sampler smp(coord::normalized, filter::linear, address::clamp_to_edge);
    const float2 uv = (float2(gid) + 0.5f) / float2(dst.get_width(), dst.get_height());
    dst.write(float4(src.sample(smp, uv).rgb, 1.0f), gid);
}

