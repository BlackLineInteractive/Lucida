#include <metal_stdlib>
using namespace metal;

// -----------------------------------------------------------------------------
// Radiance Cascades 3D - Metal Compute Shader
// Deterministic Multi-Interval Cascades Global Illumination (No Flickering / Noise)
// -----------------------------------------------------------------------------

struct RCUniforms {
    float screen_width;
    float screen_height;
    float viewport_width;
    float viewport_height;

    float camera_pos[3];
    float time;

    float cam_forward[3];
    float fov_y;

    float cam_right[3];
    float aspect;

    float cam_up[3];
    float tan_half_fov;

    float sun_dir[3];
    float sun_intensity;

    float sun_color[3];
    float sky_intensity;

    float sky_zenith[3];
    float pad0;

    float sky_horizon[3];
    float pad1;

    float sky_ground[3];
    float pad2;

    float ambient[3];
    float pad3;

    int   cascade_count;
    int   num_spheres;
    int   num_planes;
    int   num_cubes;

    int   num_lights;
    int   num_materials;
    int   frame_index;
    int   pad4;
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
struct GPULight  { float position[3]; float intensity; float color[3]; float radius; };

struct HIT {
    float t;
    float3 p;
    float3 n;
    float3 albedo;
    float3 emission;
    float roughness;
    float metallic;
    float refractive_index;
    int type;
    int mat_idx;
    bool hit;
};

// -----------------------------------------------------------------------------
// Intersection Math
// -----------------------------------------------------------------------------

float hit_sphere(float3 ro, float3 rd, float3 center, float radius, thread float3& out_normal) {
    float3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0) return -1.0;
    float sq = sqrt(disc);
    float t = -b - sq;
    if (t < 0.001) t = -b + sq;
    if (t < 0.001) return -1.0;
    out_normal = normalize(oc + rd * t);
    return t;
}

float hit_box(float3 ro, float3 rd, float3 center, float3 half_size, thread float3& out_normal) {
    float3 local_ro = ro - center;
    float3 m = 1.0 / (rd + sign(rd) * 1e-6);
    float3 n = m * local_ro;
    float3 k = abs(m) * half_size;
    float3 t1 = -n - k;
    float3 t2 = -n + k;

    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far  = min(min(t2.x, t2.y), t2.z);

    if (t_near > t_far || t_far < 0.001) return -1.0;
    float t = t_near > 0.001 ? t_near : t_far;

    float3 p = local_ro + rd * t;
    float3 d = abs(p) - half_size;
    float3 local_norm = float3(0.0);
    if (d.x > d.y && d.x > d.z) local_norm.x = sign(p.x);
    else if (d.y > d.z)         local_norm.y = sign(p.y);
    else                        local_norm.z = sign(p.z);

    out_normal = normalize(local_norm);
    return t;
}

float hit_plane(float3 ro, float3 rd, float3 normal, float d_offset, thread float3& out_normal) {
    float denom = dot(normal, rd);
    if (abs(denom) < 1e-5) return -1.0;
    float t = (d_offset - dot(normal, ro)) / denom;
    if (t < 0.001) return -1.0;
    out_normal = denom < 0.0 ? normal : -normal;
    return t;
}

// Procedural pattern helper
float3 eval_procedural(int proc_id, float3 p, float3 base_albedo) {
    switch (proc_id) {
        case 1: { // MARBLE
            float v = sin(p.x * 4.0 + sin(p.y * 6.0 + p.z * 4.0) * 2.0);
            return mix(base_albedo, float3(0.15, 0.15, 0.18), smoothstep(0.4, 0.8, v * 0.5 + 0.5));
        }
        case 2: { // WOOD
            float r = length(p.xz * 6.0);
            float ring = fract(r);
            return mix(base_albedo * 0.7, base_albedo * 1.2, smoothstep(0.2, 0.7, ring));
        }
        case 3: { // RUST
            float n = fract(sin(dot(floor(p * 8.0), float3(12.9898, 78.233, 45.164))) * 43758.5453);
            return mix(base_albedo, float3(0.45, 0.18, 0.08), step(0.65, n));
        }
        case 4: { // TILES
            float2 grid = abs(fract(p.xz * 2.0) - 0.5);
            float border = min(grid.x, grid.y);
            return border < 0.04 ? float3(0.2, 0.2, 0.22) : base_albedo;
        }
        case 6: { // HEX
            float2 h = abs(fract(p.xz * 3.0) - 0.5);
            return (h.x + h.y * 1.732 < 0.8) ? base_albedo : base_albedo * 0.4;
        }
        case 8: { // PATINA
            float n = fract(sin(dot(floor(p * 6.0), float3(23.1, 54.3, 87.2))) * 12345.67);
            return mix(base_albedo, float3(0.25, 0.65, 0.55), step(0.6, n));
        }
        case 9: { // CONCRETE
            float n = fract(sin(dot(floor(p * 20.0), float3(15.7, 33.1, 71.9))) * 9876.54);
            return base_albedo * (0.85 + 0.3 * n);
        }
        default:
            return base_albedo;
    }
}

HIT trace_scene(float3 ro, float3 rd, float max_dist,
                constant GPUSphere* spheres,
                constant GPUPlane* planes,
                constant GPUCube* cubes,
                constant GPUMaterial* materials,
                constant RCUniforms& u) {
    HIT hit;
    hit.t = max_dist;
    hit.hit = false;
    hit.albedo = float3(0.7);
    hit.emission = float3(0.0);
    hit.roughness = 0.5;
    hit.metallic = 0.0;
    hit.refractive_index = 1.5;
    hit.type = 0;
    hit.mat_idx = -1;

    // 1. Spheres
    for (int i = 0; i < u.num_spheres; ++i) {
        constant GPUSphere& s = spheres[i];
        float3 norm;
        float t = hit_sphere(ro, rd, float3(s.center[0], s.center[1], s.center[2]), s.radius, norm);
        if (t > 0.001 && t < hit.t) {
            hit.t = t;
            hit.p = ro + rd * t;
            hit.n = norm;
            hit.hit = true;
            hit.mat_idx = s.mat_index;
            if (s.mat_index >= 0 && s.mat_index < u.num_materials) {
                constant GPUMaterial& mat = materials[s.mat_index];
                hit.albedo = eval_procedural(mat.proc_id, hit.p, float3(mat.albedo[0], mat.albedo[1], mat.albedo[2]));
                hit.emission = float3(mat.emission[0], mat.emission[1], mat.emission[2]);
                hit.roughness = mat.roughness;
                hit.metallic = mat.metallic;
                hit.refractive_index = mat.refractive_index;
                hit.type = mat.type;
            }
        }
    }

    // 2. Planes
    for (int i = 0; i < u.num_planes; ++i) {
        constant GPUPlane& p = planes[i];
        float3 norm;
        float t = hit_plane(ro, rd, float3(p.normal[0], p.normal[1], p.normal[2]), p.d_offset, norm);
        if (t > 0.001 && t < hit.t) {
            hit.t = t;
            hit.p = ro + rd * t;
            hit.n = norm;
            hit.hit = true;
            hit.mat_idx = p.mat_index;
            if (p.mat_index >= 0 && p.mat_index < u.num_materials) {
                constant GPUMaterial& mat = materials[p.mat_index];
                hit.albedo = float3(mat.albedo[0], mat.albedo[1], mat.albedo[2]);
                if (mat.type == 4) { // Checkerboard
                    float f = fmod(floor(hit.p.x * 0.5) + floor(hit.p.z * 0.5), 2.0);
                    hit.albedo = (f > 0.5 || f < -0.5) ? hit.albedo : float3(mat.albedo2[0], mat.albedo2[1], mat.albedo2[2]);
                }
                hit.emission = float3(mat.emission[0], mat.emission[1], mat.emission[2]);
                hit.roughness = mat.roughness;
                hit.metallic = mat.metallic;
                hit.refractive_index = mat.refractive_index;
                hit.type = mat.type;
            }
        }
    }

    // 3. Cubes
    for (int i = 0; i < u.num_cubes; ++i) {
        constant GPUCube& c = cubes[i];
        float3 norm;
        float t = hit_box(ro, rd, float3(c.center[0], c.center[1], c.center[2]), float3(c.half_size[0], c.half_size[1], c.half_size[2]), norm);
        if (t > 0.001 && t < hit.t) {
            hit.t = t;
            hit.p = ro + rd * t;
            hit.n = norm;
            hit.hit = true;
            hit.mat_idx = c.mat_index;
            if (c.mat_index >= 0 && c.mat_index < u.num_materials) {
                constant GPUMaterial& mat = materials[c.mat_index];
                hit.albedo = eval_procedural(mat.proc_id, hit.p, float3(mat.albedo[0], mat.albedo[1], mat.albedo[2]));
                hit.emission = float3(mat.emission[0], mat.emission[1], mat.emission[2]);
                hit.roughness = mat.roughness;
                hit.metallic = mat.metallic;
                hit.refractive_index = mat.refractive_index;
                hit.type = mat.type;
            }
        }
    }

    return hit;
}

// -----------------------------------------------------------------------------
// Sky & Atmosphere
// -----------------------------------------------------------------------------

float3 sample_sky(float3 dir, constant RCUniforms& u) {
    float3 zenith  = float3(u.sky_zenith[0], u.sky_zenith[1], u.sky_zenith[2]);
    float3 horizon = float3(u.sky_horizon[0], u.sky_horizon[1], u.sky_horizon[2]);
    float3 ground  = float3(u.sky_ground[0], u.sky_ground[1], u.sky_ground[2]);

    float3 sky = dir.y >= 0.0 ? mix(horizon, zenith, pow(max(0.0, dir.y), 0.6))
                              : mix(horizon, ground, pow(max(0.0, -dir.y), 0.6));

    // Sun disk
    float3 sun_d = normalize(float3(u.sun_dir[0], u.sun_dir[1], u.sun_dir[2]));
    float sun_dot = max(0.0, dot(dir, sun_d));
    if (sun_dot > 0.0) {
        float3 sun_col = float3(u.sun_color[0], u.sun_color[1], u.sun_color[2]);
        sky += sun_col * (pow(sun_dot, 512.0) * 5.0 + pow(sun_dot, 64.0) * 0.4) * u.sun_intensity;
    }
    return sky;
}

// -----------------------------------------------------------------------------
// Direct Light Evaluation
// -----------------------------------------------------------------------------

float3 eval_direct_lights(float3 p, float3 N, float3 V, float3 albedo, float roughness, float metallic,
                          constant GPUSphere* spheres,
                          constant GPUPlane* planes,
                          constant GPUCube* cubes,
                          constant GPUMaterial* materials,
                          constant GPULight* lights,
                          constant RCUniforms& u) {
    float3 total_direct = float3(0.0);
    float3 F0 = mix(float3(0.04), albedo, metallic);

    // 1. Directional Sun Light
    float3 sun_d = normalize(float3(u.sun_dir[0], u.sun_dir[1], u.sun_dir[2]));
    float NdotL_sun = max(0.0, dot(N, sun_d));
    if (NdotL_sun > 0.0) {
        HIT sun_hit = trace_scene(p + N * 0.005, sun_d, 200.0, spheres, planes, cubes, materials, u);
        if (!sun_hit.hit) {
            float3 sun_col = float3(u.sun_color[0], u.sun_color[1], u.sun_color[2]) * u.sun_intensity;
            float3 H = normalize(sun_d + V);
            float NdotH = max(0.0, dot(N, H));
            float VdotH = max(0.0, dot(V, H));
            float shininess = pow(2.0, (1.0 - roughness) * 10.0);
            float norm = (shininess + 8.0) / 25.13274;
            float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

            float3 diff = albedo * (1.0 - metallic) * (1.0 / 3.14159265);
            float3 spec = F * pow(NdotH, shininess) * norm * (1.0 - roughness * 0.5);
            total_direct += (diff + spec) * sun_col * NdotL_sun;
        }
    }

    // 2. Point / Key / Fill Lights (Physical Inverse Square)
    for (int i = 0; i < u.num_lights; ++i) {
        constant GPULight& light = lights[i];
        float3 l_pos = float3(light.position[0], light.position[1], light.position[2]);
        float3 to_light = l_pos - p;
        float dist_sq = dot(to_light, to_light);
        float dist = sqrt(dist_sq);
        if (dist < 0.001) continue;

        float3 L = to_light / dist;
        float NdotL = max(0.0, dot(N, L));
        if (NdotL <= 0.0) continue;

        HIT s_hit = trace_scene(p + N * 0.005, L, dist - 0.01, spheres, planes, cubes, materials, u);
        if (s_hit.hit) continue; // In shadow

        float atten = light.intensity / max(dist_sq, 0.05f);
        float3 l_col = float3(light.color[0], light.color[1], light.color[2]);

        float3 H = normalize(L + V);
        float NdotH = max(0.0, dot(N, H));
        float VdotH = max(0.0, dot(V, H));
        float shininess = pow(2.0, (1.0 - roughness) * 10.0);
        float norm = (shininess + 8.0) / 25.13274;
        float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

        float3 diff = albedo * (1.0 - metallic) * (1.0 / 3.14159265);
        float3 spec = F * pow(NdotH, shininess) * norm * (1.0 - roughness * 0.5);
        total_direct += (diff + spec) * l_col * (atten * NdotL);
    }

    return total_direct;
}

// -----------------------------------------------------------------------------
// Radiance Cascades 3D Evaluation (Deterministic Multi-Interval Integration)
// -----------------------------------------------------------------------------

float3 evaluate_radiance_interval(float3 origin, float3 normal, float interval_min, float interval_max,
                                 constant GPUSphere* spheres,
                                 constant GPUPlane* planes,
                                 constant GPUCube* cubes,
                                 constant GPUMaterial* materials,
                                 constant GPULight* lights,
                                 constant RCUniforms& u) {
    float3 total_irradiance = float3(0.0);
    const int num_directions = 16;

    // Fixed Fibonacci Hemisphere (Guarantees zero temporal jitter / zero flickering)
    for (int i = 0; i < num_directions; ++i) {
        float fi = float(i);
        float phi = fi * 2.3999632; // Golden ratio spiral
        float cos_theta = sqrt((fi + 0.5) / float(num_directions));
        float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

        float3 u_axis = abs(normal.y) < 0.99 ? normalize(cross(normal, float3(0, 1, 0))) : float3(1, 0, 0);
        float3 v_axis = cross(normal, u_axis);
        float3 ray_dir = normalize(u_axis * (cos(phi) * sin_theta) + v_axis * (sin(phi) * sin_theta) + normal * cos_theta);

        float3 ro = origin + ray_dir * interval_min + normal * 0.005;
        float ray_len = interval_max - interval_min;

        HIT h = trace_scene(ro, ray_dir, ray_len, spheres, planes, cubes, materials, u);
        if (h.hit) {
            float3 surface_radiance = h.emission;
            // Add direct bounce from sunlight onto the hit surface
            float3 sun_d = normalize(float3(u.sun_dir[0], u.sun_dir[1], u.sun_dir[2]));
            float n_dot_sun = max(0.0, dot(h.n, sun_d));
            if (n_dot_sun > 0.0) {
                HIT sh = trace_scene(h.p + h.n * 0.005, sun_d, 50.0, spheres, planes, cubes, materials, u);
                if (!sh.hit) {
                    float3 sun_col = float3(u.sun_color[0], u.sun_color[1], u.sun_color[2]) * u.sun_intensity;
                    surface_radiance += sun_col * (n_dot_sun / 3.14159265) * h.albedo;
                }
            }
            total_irradiance += surface_radiance * (cos_theta / float(num_directions));
        } else {
            // Reached sky within this interval
            total_irradiance += sample_sky(ray_dir, u) * (cos_theta / float(num_directions));
        }
    }

    return total_irradiance;
}

// -----------------------------------------------------------------------------
// Primary Radiance Cascades Presentation Kernel
// -----------------------------------------------------------------------------

kernel void rc_present_kernel(
    texture2d<float, access::write> out_texture [[texture(0)]],
    constant RCUniforms& u [[buffer(0)]],
    constant GPUSphere* spheres [[buffer(1)]],
    constant GPUPlane* planes [[buffer(2)]],
    constant GPUCube* cubes [[buffer(3)]],
    constant GPUMaterial* materials [[buffer(4)]],
    constant GPULight* lights [[buffer(5)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= uint(u.viewport_width) || gid.y >= uint(u.viewport_height)) return;

    float px = float(gid.x);
    float py_inv = u.viewport_height - 1.0 - float(gid.y);

    float nx = (2.0 * (px + 0.5) / u.viewport_width - 1.0) * u.aspect * u.tan_half_fov;
    float ny = (2.0 * (py_inv + 0.5) / u.viewport_height - 1.0) * u.tan_half_fov;

    float3 fwd = float3(u.cam_forward[0], u.cam_forward[1], u.cam_forward[2]);
    float3 rgt = float3(u.cam_right[0], u.cam_right[1], u.cam_right[2]);
    float3 up  = float3(u.cam_up[0], u.cam_up[1], u.cam_up[2]);

    float3 ray_dir = normalize(fwd + rgt * nx + up * ny);
    float3 ray_origin = float3(u.camera_pos[0], u.camera_pos[1], u.camera_pos[2]);

    // 1. Primary Ray Trace
    HIT primary_hit = trace_scene(ray_origin, ray_dir, 1000.0, spheres, planes, cubes, materials, u);

    float3 final_color = float3(0.0);

    if (primary_hit.hit) {
        float3 pos = primary_hit.p;
        float3 norm = primary_hit.n;
        float3 view_d = -ray_dir;

        // Handle Metallic Mirror Reflection (Type 1)
        if (primary_hit.type == 1 && primary_hit.roughness < 0.15) {
            float3 refl_dir = reflect(ray_dir, norm);
            HIT refl_hit = trace_scene(pos + norm * 0.005, refl_dir, 500.0, spheres, planes, cubes, materials, u);
            float3 refl_col;
            if (refl_hit.hit) {
                float3 refl_direct = eval_direct_lights(refl_hit.p, refl_hit.n, -refl_dir, refl_hit.albedo, refl_hit.roughness, refl_hit.metallic,
                                                        spheres, planes, cubes, materials, lights, u);
                refl_col = refl_hit.emission + refl_direct;
            } else {
                refl_col = sample_sky(refl_dir, u);
            }
            final_color = mix(refl_col * primary_hit.albedo, primary_hit.emission, 0.5);
        }
        // Handle Glass Dielectric Refraction/Reflection (Type 2)
        else if (primary_hit.type == 2) {
            float n1 = 1.0;
            float n2 = max(primary_hit.refractive_index, 1.05);
            float eta = dot(ray_dir, norm) < 0.0 ? (n1 / n2) : (n2 / n1);
            float3 refr_n = dot(ray_dir, norm) < 0.0 ? norm : -norm;
            float3 refr_dir = refract(ray_dir, refr_n, eta);
            float3 refl_dir = reflect(ray_dir, norm);

            // Schlick fresnel
            float r0 = pow((n1 - n2) / (n1 + n2), 2.0);
            float fresnel = r0 + (1.0 - r0) * pow(1.0 - abs(dot(ray_dir, refr_n)), 5.0);

            float3 refl_col = sample_sky(refl_dir, u);
            float3 refr_col = float3(0.0);
            if (length(refr_dir) > 0.01) {
                HIT refr_hit = trace_scene(pos - refr_n * 0.01, refr_dir, 500.0, spheres, planes, cubes, materials, u);
                refr_col = refr_hit.hit ? (refr_hit.albedo * 0.8 + refr_hit.emission) : sample_sky(refr_dir, u);
            }
            final_color = mix(refr_col, refl_col, fresnel) * primary_hit.albedo;
        }
        // Diffuse, PBR, Checkerboard & Emissive Surfaces
        else {
            // 2. Direct Lighting (Sun + Point / Spot lights)
            float3 direct_light = eval_direct_lights(pos, norm, view_d, primary_hit.albedo, primary_hit.roughness, primary_hit.metallic,
                                                     spheres, planes, cubes, materials, lights, u);

            // 3. Multi-Interval Radiance Cascades (Hierarchical Probe Merging)
            float3 c0 = evaluate_radiance_interval(pos, norm, 0.02, 0.5, spheres, planes, cubes, materials, lights, u);
            float3 c1 = evaluate_radiance_interval(pos, norm, 0.5, 2.5, spheres, planes, cubes, materials, lights, u);
            float3 c2 = evaluate_radiance_interval(pos, norm, 2.5, 12.0, spheres, planes, cubes, materials, lights, u);
            float3 c3 = evaluate_radiance_interval(pos, norm, 12.0, 60.0, spheres, planes, cubes, materials, lights, u);

            // Radiance cascade smooth hierarchical merge
            float3 indirect_irradiance = c0 * 0.35 + c1 * 0.30 + c2 * 0.20 + c3 * 0.15;
            float3 ambient = float3(u.ambient[0], u.ambient[1], u.ambient[2]);
            indirect_irradiance += ambient * 0.3;

            float3 indirect_diffuse = indirect_irradiance * (primary_hit.albedo / 3.14159265) * (1.0 - primary_hit.metallic);

            // Composite final surface illumination
            final_color = primary_hit.emission + direct_light + indirect_diffuse;
        }
    } else {
        // Sky Background
        final_color = sample_sky(ray_dir, u);
    }

    // 4. ACES Film Tone Mapping
    float3 x = max(float3(0.0), final_color);
    float3 mapped = clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);

    // Gamma correction
    float3 srgb = pow(mapped, float3(1.0 / 2.2));

    out_texture.write(float4(srgb, 1.0), gid);
}
