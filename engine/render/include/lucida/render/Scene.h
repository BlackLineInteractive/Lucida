#pragma once
// Scene description, as data.
//
// A backend receives one of these and uploads it. It does not know how the scene
// was assembled — code, a JSON file or an editor — and it holds no scene of its
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
    Vec3 ambient{0.3f, 0.4f, 0.6f};
    bool fog_enabled  = true;
    f32  fog_density  = 0.022f;
    i32  fog_steps    = 16;
};

// Everything the tracer needs, in the layout the GPU reads. Kept as plain
// vectors: a scene is built once and uploaded, so ergonomics beat packing here.
struct RenderScene {
    std::string name = "untitled";

    std::vector<GPUMaterial> materials;
    std::vector<GPUSphere>   spheres;
    std::vector<GPUPlane>    planes;
    std::vector<GPUCube>     cubes;
    std::vector<GPULight>    lights;

    SceneEnvironment environment;
    ShadingModel     model = ShadingModel::WhittedGI;

    // Where the player starts. The backend ignores this; the application applies
    // it to its camera controller, because moving a camera is not a render job.
    CameraState spawn;

    i32 AddMaterial(const Material& m, i32 procedural = PROC_NONE) {
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
        return i32(materials.size()) - 1;
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

    void AddLight(const Vec3& position, f32 intensity, const Vec3& color, f32 radius) {
        GPULight l{};
        SetVec3(l.position, position);
        l.intensity = intensity;
        SetVec3(l.color, color);
        l.radius = radius;
        lights.push_back(l);
    }

    void Clear() {
        materials.clear(); spheres.clear(); planes.clear(); cubes.clear(); lights.clear();
    }
};

} // namespace lucida
