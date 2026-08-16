// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Scene description, as data.
//
// A backend receives one of these and uploads it. It does not know how the scene
// was assembled - code, a JSON file or an editor - and it holds no scene of its
// own. That is the whole point of this header: before it existed, the demo scenes
// lived inside the Metal backend, which made a second backend impossible to write
// and a scene impossible to author.

#include "lucida/render/Camera.h"
#include "lucida/render/GpuTypes.h"

#include <string>
#include <vector>

namespace lucida {

// Which tracing kernel this scene needs. It is a property of the scene because
// the scene decides whether water, fog and mesh instances are present at all;
// the backend maps it onto whatever pipeline it has.
enum class ShadingModel : u8 {
    Whitted,     // analytic primitives, direct lighting, hard shadows
    WhittedGI    // adds meshes, water, volumetric fog, indirect bounce
};

struct SceneEnvironment {
    Vec3 ambient{0.08f, 0.10f, 0.14f};
    bool fog_enabled  = false;   // opt-in, not always-on
    f32  fog_density  = 0.022f;
    i32  fog_steps    = 16;

    // Sky gradient - neutral dark studio tones (Blender/Unreal viewport style).
    Vec3 sky_zenith{0.06f, 0.09f, 0.16f};
    Vec3 sky_horizon{0.18f, 0.22f, 0.28f};
    Vec3 sky_ground{0.04f, 0.04f, 0.06f};

    // Sun / Celestial Lighting
    bool sun_enabled = true;
    Vec3 sun_direction{0.527f, 0.422f, 0.738f};
    Vec3 sun_color{1.0f, 0.94f, 0.82f};
    f32  sun_intensity = 5.0f;

    // Water simulation parameters
    f32  water_height = 0.02f;
    f32  water_speed  = 1.0f;
    f32  water_frequency = 1.0f;
    f32  water_foam   = 0.8f;

    // Post-processing & Tonemapping
    f32  exposure = 1.0f;
    i32  tonemap_mode = 0; // 0: ACES, 1: Reinhard, 2: Filmic, 3: Linear
    f32  gamma = 2.2f;
    f32  dither_strength = 1.0f;

    // Ambient Occlusion
    f32  ao_radius = 1.2f;
    f32  ao_intensity = 0.5f;
    i32  ao_samples = 6;

    // Editor ground grid, drawn by the tracer so it sits behind geometry
    // correctly instead of being painted over the finished image.
    bool grid_enabled = true;
    Vec3 grid_color{0.42f, 0.44f, 0.48f};
    Vec3 grid_axis_x{0.78f, 0.28f, 0.32f};
    Vec3 grid_axis_z{0.30f, 0.48f, 0.85f};
    f32  grid_opacity = 0.65f;
    f32  grid_fade    = 120.0f;
    bool grid_auto_scale = true;
    f32  grid_spacing = 1.0f;
};

// Everything the tracer needs, in the layout the GPU reads. Kept as plain
// vectors: a scene is built once and uploaded, so ergonomics beat packing here.
struct RenderScene {
    std::string name = "untitled";

    std::vector<GPUMaterial> materials;
    // Authoring-only, never uploaded. A hand-edited scene file references
    // materials by name; without these, inserting one material at the top of a
    // file would silently repaint everything below it.
    std::vector<std::string> material_names;
    std::vector<GPUSphere>   spheres;
    std::vector<GPUPlane>    planes;
    std::vector<GPUCube>     cubes;
    std::vector<GPUCylinder> cylinders;
    std::vector<GPUCone>     cones;
    std::vector<GPUTorus>    tori;
    std::vector<GPUDisk>     disks;
    std::vector<GPULight>    lights;

    SceneEnvironment environment;
    ShadingModel     model = ShadingModel::WhittedGI;

    // Where the player starts. The backend ignores this; the application applies
    // it to its camera controller, because moving a camera is not a render job.
    CameraState spawn;

    i32 AddMaterial(const Material& m, i32 procedural = PROC_NONE,
                    const std::string& name = {}) {
        GPUMaterial gm{};
        SetVec3(gm.albedo, m.albedo);
        SetVec3(gm.emission, m.emission);
        SetVec3(gm.albedo2, m.albedo2);
        gm.roughness        = f32(m.roughness);
        gm.metallic         = f32(m.metallic);
        gm.refractive_index = f32(m.refractive_index);
        gm.type             = i32(m.type);
        gm.proc_id          = procedural;
        materials.push_back(gm);
        material_names.push_back(name.empty()
                                     ? "material_" + std::to_string(materials.size() - 1)
                                     : name);
        return i32(materials.size()) - 1;
    }

    i32 FindMaterial(const std::string& name) const {
        for (usize i = 0; i < material_names.size(); ++i) {
            if (material_names[i] == name) return i32(i);
        }
        return -1;
    }

    void AddSphere(const Vec3& center, f32 radius, i32 material) {
        GPUSphere s{};
        SetVec3(s.center, center);
        s.radius = radius;
        s.mat_index = material;
        spheres.push_back(s);
    }

    void AddPlane(const Vec3& normal, f32 offset, i32 material) {
        GPUPlane p{};
        SetVec3(p.normal, normal);
        p.d_offset = offset;
        p.mat_index = material;
        planes.push_back(p);
    }

    void AddCube(const Vec3& center, const Vec3& half_size, i32 material) {
        GPUCube c{};
        SetVec3(c.center, center);
        SetVec3(c.half_size, half_size);
        c.mat_index = material;
        cubes.push_back(c);
    }

    void AddCylinder(const Vec3& center, f32 radius, f32 height, i32 material) {
        GPUCylinder c{};
        SetVec3(c.center, center);
        c.radius = radius;
        c.height = height;
        c.mat_index = material;
        cylinders.push_back(c);
    }

    void AddCone(const Vec3& center, f32 radius, f32 height, i32 material) {
        GPUCone c{};
        SetVec3(c.center, center);
        c.radius = radius;
        c.height = height;
        c.mat_index = material;
        cones.push_back(c);
    }

    void AddTorus(const Vec3& center, f32 radius, f32 inner_radius, i32 material) {
        GPUTorus t{};
        SetVec3(t.center, center);
        t.radius = radius;
        t.inner_radius = inner_radius;
        t.mat_index = material;
        tori.push_back(t);
    }

    void AddDisk(const Vec3& center, f32 radius, const Vec3& normal, i32 material) {
        GPUDisk d{};
        SetVec3(d.center, center);
        d.radius = radius;
        SetVec3(d.normal, normal);
        d.mat_index = material;
        disks.push_back(d);
    }

    void AddLight(const Vec3& position, f32 intensity, const Vec3& color, f32 radius) {
        GPULight l{};
        SetVec3(l.position, position);
        l.intensity = intensity;
        SetVec3(l.color, color);
        l.radius = radius;
        lights.push_back(l);
    }

    void Clear() {
        materials.clear(); material_names.clear();
        spheres.clear(); planes.clear(); cubes.clear(); lights.clear();
        cylinders.clear(); cones.clear(); tori.clear(); disks.clear();
    }
};

} // namespace lucida
