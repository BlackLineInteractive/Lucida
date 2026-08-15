// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include <metal_stdlib>
using namespace metal;

struct Material { packed_float3 albedo; float roughness; packed_float3 emission; float metallic; packed_float3 albedo2; float refractive_index; int type; int pad1, pad2, pad3; };
struct Sphere { packed_float3 center; float radius; int mat_index; int pad1, pad2, pad3; };
struct Plane { packed_float3 normal; float d_offset; int mat_index; int pad1, pad2, pad3; };
struct Cube { packed_float3 center; float pad1; packed_float3 half_size; int mat_index; };
struct Octahedron { packed_float3 v0; float pad0; packed_float3 v1; float pad1; packed_float3 v2; float pad2; packed_float3 v3; float pad3; packed_float3 v4; float pad4; packed_float3 v5; float pad5; int mat_index; int pad6_1, pad6_2, pad6_3; };
struct Light { packed_float3 position; float intensity; packed_float3 color; float radius; };

struct Uniforms {
    int num_spheres, num_planes, num_cubes, num_octahedrons;
    int num_lights,  max_depth,  num_triangles, enable_triangles;
    float tan_half_fov, aspect_ratio, screen_width, screen_height;
    packed_float3 ambient_light; float pad2;
    packed_float3 camera_origin; float pad3;
    packed_float3 camera_forward; float pad4;
    packed_float3 camera_right;   float pad5;
    packed_float3 camera_up;      float pad6;
    float time;
    int enable_fog;
    int enable_jitter;
    int samples_per_pixel;
    int pad_end;

    // The v02 kernel never used these fields, but the struct has to match the
    // host's byte for byte or every read past this point is garbage.
    float orm_tex_dim_unused; int mesh_mat_count_unused; int num_instances_unused; float pad10;
    packed_float3 sky_zenith;  float grid_opacity;
    packed_float3 sky_horizon; float grid_fade;
    packed_float3 sky_ground;  int   grid_enabled;
    packed_float3 grid_color;  float pad11;
    packed_float3 grid_axis_x; float pad12;
    packed_float3 grid_axis_z; float pad13;
};

constant float EPSILON = 1e-4;
constant float INF = 1e20;
constant int MAX_STACK = 10;
constant int DIFFUSE = 0, METAL = 1, GLASS = 2, EMISSIVE = 3, CHECKERBOARD = 4, WATER = 5;

struct Ray { float3 origin; float3 direction; };
struct HitInfo { bool hit; float t; float3 point, normal; int mat_index; float2 uv; };

Ray make_ray(float3 o, float3 d) { return {o, normalize(d)}; }

// --- intersections ---
HitInfo intersect_sphere(Ray ray, device const Sphere& s) {
    HitInfo info; info.hit = false; info.t = INF;
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
            info.normal = normalize(info.point - s.center);
            info.mat_index = s.mat_index;
            float3 pl = (info.point - s.center) / s.radius;
            info.uv = float2((atan2(pl.z, pl.x) + 3.14159) / (2.0 * 3.14159), (asin(clamp(pl.y, -1.0, 1.0)) + 1.5707) / 3.14159);
        }
    }
    return info;
}

HitInfo intersect_plane(Ray ray, device const Plane& p) {
    HitInfo info; info.hit = false; info.t = INF;
    float denom = dot(p.normal, ray.direction);
    if (abs(denom) > EPSILON) {
        float t = (p.d_offset - dot(ray.origin, p.normal)) / denom;
        if (t > EPSILON) {
            info.hit = true; info.t = t;
            info.point = ray.origin + ray.direction * t;
            info.normal = p.normal;
            info.mat_index = p.mat_index;
            float3 u_axis = abs(p.normal.y) > 0.9 ? float3(1,0,0) : normalize(cross(float3(0,1,0), p.normal));
            float3 v_axis = normalize(cross(p.normal, u_axis));
            info.uv = float2(dot(info.point, u_axis)*0.1, dot(info.point, v_axis)*0.1);
        }
    }
    return info;
}

HitInfo intersect_cube(Ray ray, device const Cube& c) {
    HitInfo info; info.hit = false; info.t = INF;
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
        float3 hr = info.point - c.center;
        float3 n = float3(0.0);
        if (abs(abs(hr.x) - c.half_size.x) < EPSILON*10.0) n.x = sign(hr.x);
        else if (abs(abs(hr.y) - c.half_size.y) < EPSILON*10.0) n.y = sign(hr.y);
        else if (abs(abs(hr.z) - c.half_size.z) < EPSILON*10.0) n.z = sign(hr.z);
        info.normal = normalize(n);
        info.uv = float2(hr.x / c.half_size.x, hr.z / c.half_size.z);
    }
    return info;
}

HitInfo find_closest(Ray ray, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, constant Uniforms& u) {
    HitInfo closest; closest.hit = false; closest.t = INF;
    for (int i = 0; i < u.num_spheres; i++) { HitInfo h = intersect_sphere(ray, spheres[i]); if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_planes; i++)  { HitInfo h = intersect_plane(ray, planes[i]);  if (h.hit && h.t < closest.t) closest = h; }
    for (int i = 0; i < u.num_cubes; i++)   { HitInfo h = intersect_cube(ray, cubes[i]);    if (h.hit && h.t < closest.t) closest = h; }
    return closest;
}

// --- analytic basis for shadows and AO ---
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

float cone_cube_occlusion(float3 cone_o, float3 cone_d, float cone_angle, device const Cube& c) {
    float radius = length(c.half_size);
    return cone_sphere_occlusion(cone_o, cone_d, cone_angle, c.center, radius) * 0.5;
}

float calc_analytic_shadow(float3 p, float3 n, float3 lpos, float lrad, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, constant Uniforms& u) {
    float3 L = lpos - p;
    float actual_dist = length(L);
    L /= actual_dist;
    
    float light_angle = atan(lrad / actual_dist);
    float occlusion = 0.0;
    float3 ro = p + n * 0.01;

    for(int i = 0; i < u.num_spheres; i++) {
        if (distance(spheres[i].center, lpos) < 1e-2) continue;
        occlusion += cone_sphere_occlusion(ro, L, light_angle, spheres[i].center, spheres[i].radius);
    }
    
    for(int i = 0; i < u.num_cubes; i++) {
        Ray shadow_ray = make_ray(ro, L);
        HitInfo h = intersect_cube(shadow_ray, cubes[i]);
        if (h.hit && h.t > 0.01 && h.t < actual_dist - 0.01) {
            occlusion = 1.0;
            break;
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
            float occ = cone_cube_occlusion(ro, n, ao_cone_angle, cubes[i]);
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

float3 calc_bounce_gi(float3 p, float3 n, device const Material* materials, device const Sphere* spheres, device const Plane* planes, device const Cube* cubes, constant Uniforms& u) {
    float3 gi = float3(0.0);
    float3 ro = p + n * 0.05;
    
    for (int i = 0; i < u.num_spheres; i++) {
        float3 to_obj = spheres[i].center - ro;
        float dist = length(to_obj);
        if (dist < 0.1 || dist > 6.0) continue;
        float3 L = to_obj / dist;
        float NdotL = max(0.0, dot(n, L));
        if (NdotL <= 0.0) continue;
        
        Material obj_mat = materials[spheres[i].mat_index];
        if (obj_mat.type == EMISSIVE) continue; 
        
        float3 obj_radiance = obj_mat.albedo * (sky_color(float3(0,1,0), u) * 0.5 + u.ambient_light * 0.3);
        float form_factor = (spheres[i].radius * spheres[i].radius * NdotL) / (dist * dist + 0.1);
        gi += obj_radiance * form_factor * 0.6;
    }
    
    for (int i = 0; i < u.num_cubes; i++) {
        float3 to_obj = cubes[i].center - ro;
        float dist = length(to_obj);
        if (dist < 0.1 || dist > 6.0) continue;
        float3 L = to_obj / dist;
        float NdotL = max(0.0, dot(n, L));
        if (NdotL <= 0.0) continue;
        
        Material obj_mat = materials[cubes[i].mat_index];
        if (obj_mat.type == EMISSIVE) continue;
        
        float3 obj_radiance = obj_mat.albedo * (sky_color(float3(0,1,0), u) * 0.5 + u.ambient_light * 0.3);
        float area = 4.0 * (cubes[i].half_size.x * cubes[i].half_size.y + 
                            cubes[i].half_size.y * cubes[i].half_size.z + 
                            cubes[i].half_size.z * cubes[i].half_size.x);
        float form_factor = (area * NdotL) / (dist * dist + area);
        gi += obj_radiance * form_factor * 0.4;
    }
    
    for (int i = 0; i < u.num_planes; i++) {
        if (planes[i].normal.y > 0.9) {
            float dist_to_floor = abs((planes[i].d_offset - dot(ro, planes[i].normal)) / planes[i].normal.y);
            if (dist_to_floor > 0.01 && dist_to_floor < 5.0 && n.y > 0.0) {
                Material floor_mat = materials[planes[i].mat_index];
                float3 floor_alb = floor_mat.albedo;
                if (floor_mat.type == CHECKERBOARD) {
                    floor_alb = (floor_mat.albedo + floor_mat.albedo2) * 0.5;
                }
                float3 floor_radiance = floor_alb * (sky_color(float3(0,1,0), u) * 0.6 + u.ambient_light * 0.4);
                float form_factor = n.y / (dist_to_floor * dist_to_floor + 0.5);
                gi += floor_radiance * form_factor * 0.5;
            }
        }
    }
    
    return gi;
}

float3 trace_ray(Ray ray, device const Material* materials, device const Sphere* spheres,
                 device const Plane* planes, device const Cube* cubes, device const Light* lights, constant Uniforms& u, thread float& first_dist) {
                 
    first_dist = 60.0;
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

        HitInfo hit = find_closest(cur, spheres, planes, cubes, u);

        if (first_iteration) {
            first_iteration = false;
            if (hit.hit) first_dist = hit.t;
        }

        if (!hit.hit) {
            result += contrib * sky_color(cur.direction, u);
            continue;
        }

        Material mat = materials[hit.mat_index];
        result += contrib * mat.emission;
        if (mat.type == EMISSIVE) continue;

        float3 N = hit.normal;
        float3 V = normalize(cur.origin - hit.point);
        float3 alb = mat.albedo;
        
        if (mat.type == CHECKERBOARD) {
            int cs = 4;
            bool white = (int(floor(hit.uv.x * float(cs))) + int(floor(hit.uv.y * float(cs)))) % 2 == 0;
            alb = white ? mat.albedo : mat.albedo2;
        }

        if (mat.type != GLASS) {
            float3 direct = float3(0.0);
            
            for (int i = 0; i < u.num_lights; i++) {
                float3 to_light = lights[i].position - hit.point;
                float dist_sq = dot(to_light, to_light);
                float dist = sqrt(dist_sq);
                float3 L = to_light / dist;
                float NdotL = max(0.0, dot(N, L));
                
                if (NdotL > 0.0) {
                    float sh = calc_analytic_shadow(hit.point, N, lights[i].position, lights[i].radius, spheres, planes, cubes, u);
                    if (sh > 0.0) {
                        float atten = lights[i].intensity / max(dist_sq, 0.01f);
                        float3 H = normalize(L + V);
                        float NdotH = max(0.0, dot(N, H));
                        float shininess = pow(2.0, (1.0 - mat.roughness) * 10.0);

                        if (mat.type == METAL) {
                            float3 spec = alb * lights[i].color * pow(NdotH, shininess) * atten;
                            direct += spec * sh;
                        } else {
                            float3 diff = alb * lights[i].color * NdotL * atten;
                            float3 spec = float3(1.0) * lights[i].color * pow(NdotH, shininess) * atten * 0.25 * (1.0 - mat.roughness);
                            direct += (diff + spec) * sh;
                        }
                    }
                }
            }
            
            float ao = calc_analytic_ao(hit.point, N, spheres, cubes, u);
            float3 sky_ambient = sky_color(N, u) * u.ambient_light * ao * 0.5;
            float3 bounce_gi = calc_bounce_gi(hit.point, N, materials, spheres, planes, cubes, u);
            bounce_gi *= ao;

            direct += (sky_ambient + bounce_gi) * alb;
            result += contrib * direct;
        }

        if (mat.type == METAL && sp < MAX_STACK) {
            float3 R = reflect(cur.direction, N);
            float refl = 1.0 - mat.roughness * 0.3;
            if (refl > EPSILON) {
                stack_ray[sp] = make_ray(hit.point + N * 3e-3, R);
                stack_contrib[sp] = contrib * alb * refl;
                stack_depth[sp] = depth - 1;
                sp++;
            }
        }
        else if (mat.type == GLASS && sp < MAX_STACK) {
            float n1 = 1.0, n2 = mat.refractive_index;
            float3 Nf = N; float cos_i = -dot(cur.direction, Nf);
            if (cos_i < 0.0) { float tmp = n1; n1 = n2; n2 = tmp; Nf = -Nf; cos_i = -dot(cur.direction, Nf); }
            float3 F = schlick(cos_i, n1, n2);
            float fresnel_r = F.x;

            if (fresnel_r > EPSILON && sp < MAX_STACK) {
                stack_ray[sp] = make_ray(hit.point + N * 3e-3, reflect(cur.direction, N));
                stack_contrib[sp] = contrib * fresnel_r;
                stack_depth[sp] = depth - 1;
                sp++;
            }
            if ((1.0 - fresnel_r) > EPSILON && sp < MAX_STACK) {
                float eta = n1 / n2;
                float k = 1.0 - eta * eta * (1.0 - cos_i * cos_i);
                if (k >= 0.0) {
                    float3 T = eta * cur.direction + (eta * cos_i - sqrt(k)) * Nf;
                    stack_ray[sp] = make_ray(hit.point - Nf * 5e-3, T);
                    stack_contrib[sp] = contrib * (1.0 - fresnel_r);
                    stack_depth[sp] = depth - 1;
                    sp++;
                }
            }
        }
    }
    
    return clamp(result, float3(0.0), float3(100.0));
}

kernel void raytrace_kernel(texture2d<float, access::write> outTexture [[texture(0)]],
                            texture2d_array<float, access::read> mesh_textures [[texture(1)]],
                            texture2d<float, access::write> outDepth [[texture(2)]],
                            texture2d<float, access::write> outMotion [[texture(3)]],
                            device const Material* materials [[buffer(0)]],
                            device const Sphere* spheres [[buffer(1)]],
                            device const Plane* planes [[buffer(2)]],
                            device const Cube* cubes [[buffer(3)]],
                            device const Light* lights [[buffer(5)]],
                            constant Uniforms& u [[buffer(6)]],
                            uint2 gid [[thread_position_in_grid]]) {
    
    if (gid.x >= uint(u.screen_width) || gid.y >= uint(u.screen_height)) return;

    float2 px = float2(gid);
    float py_inv = u.screen_height - px.y; 
    
    float3 color = float3(0.0);
    float3 center_dir = float3(0.0);
    float center_fd = 60.0;
    int SAMPLES = u.samples_per_pixel;
    if (SAMPLES < 1) SAMPLES = 1;
    if (SAMPLES > 4) SAMPLES = 4;
    
    for (int dy = 0; dy < SAMPLES; dy++) {
        for (int dx = 0; dx < SAMPLES; dx++) {
            float2 offset = float2(float(dx) + 0.5, float(dy) + 0.5) / float(SAMPLES);
            float nx = (2.0 * (px.x + offset.x) / u.screen_width - 1.0) * u.aspect_ratio * u.tan_half_fov;
            float ny = (2.0 * (py_inv + offset.y) / u.screen_height - 1.0) * u.tan_half_fov;
            
            float3 dir = normalize(u.camera_forward + nx * u.camera_right + ny * u.camera_up);
            Ray r = make_ray(u.camera_origin, dir);
            
            float fd = 60.0;
            color += trace_ray(r, materials, spheres, planes, cubes, lights, u, fd);
            if (dx == 0 && dy == 0) { center_dir = dir; center_fd = fd; }
        }
    }
    color /= float(SAMPLES * SAMPLES);

    // Grid before fog and tone mapping, so it sits in the scene rather than on
    // the screen.
    color = composite_grid(color, u.camera_origin, center_dir, center_fd, u.tan_half_fov, u);

    if (u.enable_fog > 0) {
        int steps = 16;
        float max_dist = min(center_fd, 60.0);
        float step_size = max_dist / float(steps);
        float3 p = u.camera_origin;
        float3 dir = center_dir;
        
        float3 vol_color = float3(0.0);
        float transmittance = 1.0;
        float3 sky_ambient = sky_color(dir, u);
        
        for (int i = 0; i < steps; i++) {
            float3 sample_p = p + dir * ((float(i) + 0.5) * step_size);
            float h = sample_p.y + 1.0;
            float density = 0.03 * exp(-max(0.0f, h) * 0.8f) + 0.005;
            
            float3 step_scattering = float3(0.0);
            if (density > 0.0001f) {
                for (int l = 0; l < u.num_lights; l++) {
                    float3 to_light = lights[l].position - sample_p;
                    float dist = length(to_light);
                    float3 l_dir = to_light / max(dist, 0.01f);
                    float sh = calc_analytic_shadow(sample_p, float3(0.0, 1.0, 0.0), lights[l].position, lights[l].radius, spheres, planes, cubes, u);
                    float3 light_intensity = lights[l].color * lights[l].intensity / (dist * dist + 0.1);
                    float l_cos = max(0.0, dot(dir, l_dir));
                    float phase = 0.15 + 0.35 * pow(l_cos, 4.0);
                    step_scattering += light_intensity * phase * sh;
                }
                step_scattering += sky_ambient * 0.7f;
            }
            
            float extinction = exp(-density * step_size);
            float3 step_inscatter = step_scattering * (1.0 - extinction);
            
            vol_color += transmittance * step_inscatter;
            transmittance *= extinction;
            if (transmittance < 0.01) break;
        }
        
        color = color * transmittance + vol_color;
    }
    
    float3 mapped = color / (color + float3(1.0));
    mapped = pow(clamp(mapped, float3(0.0), float3(1.0)), float3(1.0/2.2));
    
    outTexture.write(float4(mapped, 1.0), gid);
    outDepth.write(float4(0.5, 0.0, 0.0, 0.0), gid);
    outMotion.write(float4(0.0, 0.0, 0.0, 0.0), gid);
}

// Copies the traced image to the drawable.
//
// Without this the only path to the screen was MetalFX, so a machine without it
// — or a render scale of 1:1, where a temporal *upscaler* has nothing to
// upscale — presented a black window while the tracer worked perfectly into an
// offscreen texture. Linear sampling covers the upscale when the scaler is off.
kernel void present_kernel(texture2d<float, access::sample> src [[texture(0)]],
                           texture2d<float, access::write>  dst [[texture(1)]],
                           uint2 gid [[thread_position_in_grid]]) {
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    constexpr sampler smp(coord::normalized, filter::linear, address::clamp_to_edge);
    const float2 uv = (float2(gid) + 0.5f) / float2(dst.get_width(), dst.get_height());
    dst.write(float4(src.sample(smp, uv).rgb, 1.0f), gid);
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

    // Pick the decade whose lines are still far enough apart to read, and blend
    // into the next one so nothing pops as the camera pulls back.
    float decade = max(0.0, log10(footprint * 40.0));
    float step0 = pow(10.0, floor(decade));
    float step1 = step0 * 10.0;
    float blend = fract(decade);

    float half_width = footprint * 0.6;

    // Distance to the nearest line of a given spacing, in world units.
    float2 d0 = abs(fract(p.xz / step0 + 0.5) - 0.5) * step0;
    float2 d1 = abs(fract(p.xz / step1 + 0.5) - 0.5) * step1;

    float l0 = 1.0 - smoothstep(half_width, half_width * 2.0, min(d0.x, d0.y));
    float l1 = 1.0 - smoothstep(half_width, half_width * 2.0, min(d1.x, d1.y));

    // The coarser decade is always fully present; the finer one fades in.
    out.alpha = max(l1, l0 * (1.0 - blend));

    // Axes last so they win over the lines they sit on.
    float ax = 1.0 - smoothstep(half_width, half_width * 2.5, abs(p.z));
    float az = 1.0 - smoothstep(half_width, half_width * 2.5, abs(p.x));
    if (ax > 0.01) { out.color = float3(u.grid_axis_x); out.alpha = max(out.alpha, ax); }
    if (az > 0.01) { out.color = float3(u.grid_axis_z); out.alpha = max(out.alpha, az); }

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

    float footprint = t * 2.0 * tan_half / max(u.screen_height, 1.0);
    GridSample g = sample_grid(p, footprint, u);
    if (g.alpha <= 0.001) return color;

    // Fade with distance so the horizon stays clean instead of turning into a
    // solid band of aliased lines.
    float fade = 1.0 - smoothstep(u.grid_fade * 0.35, u.grid_fade, t);
    // And fade at grazing angles, where a pixel covers so much ground that every
    // line inside it would average to a smear.
    float grazing = smoothstep(0.0, 0.12, abs(dir.y));

    return mix(color, g.color, g.alpha * fade * grazing * u.grid_opacity);
}
