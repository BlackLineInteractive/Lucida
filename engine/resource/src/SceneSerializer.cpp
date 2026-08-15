// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/SceneSerializer.h"

#include "lucida/core/diag/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>

namespace lucida {
namespace {

using json = nlohmann::json;

// Enums travel as names. A scene file with "glass" in it can be read by a human
// and diffed usefully; a file with 2 in it cannot.
const std::unordered_map<std::string, MaterialType> kMaterialTypes = {
    {"diffuse", DIFFUSE}, {"metal", METAL}, {"glass", GLASS}, {"emissive", EMISSIVE},
    {"checkerboard", CHECKERBOARD}, {"water", WATER}, {"pbr", PBR},
};

const std::unordered_map<std::string, i32> kProceduralPatterns = {
    {"none", PROC_NONE}, {"marble", PROC_MARBLE}, {"wood", PROC_WOOD},
    {"rust", PROC_RUST}, {"tiles", PROC_TILES}, {"brushed", PROC_BRUSHED},
    {"hex", PROC_HEX}, {"roughness_ramp", PROC_ROUGH_RAMP},
    {"patina", PROC_PATINA}, {"concrete", PROC_CONCRETE},
};

template <typename Map>
const char* NameOf(const Map& map, i32 value, const char* fallback) {
    for (const auto& [name, v] : map) {
        if (i32(v) == value) return name.c_str();
    }
    return fallback;
}

template <typename Map, typename T>
bool ParseEnum(const Map& map, const json& node, const char* field, T& out) {
    const auto it = map.find(node.value(field, std::string{}));
    if (it == map.end()) {
        LUCIDA_ERROR(Resource, "unknown %s: '%s'", field,
                     node.value(field, std::string("<missing>")).c_str());
        return false;
    }
    out = T(it->second);
    return true;
}

json ToJson(const Vec3& v) { return json::array({v.x, v.y, v.z}); }

Vec3 ToVec3(const json& node, const Vec3& fallback = Vec3(0.0f)) {
    if (!node.is_array() || node.size() != 3) return fallback;
    return Vec3(node[0].get<f32>(), node[1].get<f32>(), node[2].get<f32>());
}

json ToJson(const float v[3]) { return json::array({v[0], v[1], v[2]}); }

} // namespace

bool SaveScene(const RenderScene& scene, const std::string& path) {
    json out;
    out["version"] = 1;
    out["name"]    = scene.name;
    out["model"]   = scene.model == ShadingModel::Whitted ? "whitted" : "whitted_gi";

    out["environment"] = {
        {"ambient", ToJson(scene.environment.ambient)},
        {"fog", scene.environment.fog_enabled},
        {"fog_density", scene.environment.fog_density},
        {"fog_steps", scene.environment.fog_steps},
    };

    out["spawn"] = {
        {"position", ToJson(scene.spawn.position)},
        {"yaw", scene.spawn.yaw},
        {"pitch", scene.spawn.pitch},
        {"fov_y_degrees", scene.spawn.fov_y * kRadToDeg},
    };

    for (usize i = 0; i < scene.materials.size(); ++i) {
        const GPUMaterial& m = scene.materials[i];
        json jm;
        jm["name"]       = i < scene.material_names.size() ? scene.material_names[i]
                                                           : "material_" + std::to_string(i);
        jm["type"]       = NameOf(kMaterialTypes, m.type, "diffuse");
        jm["albedo"]     = ToJson(m.albedo);
        jm["roughness"]  = m.roughness;
        jm["metallic"]   = m.metallic;
        if (m.emission[0] != 0.0f || m.emission[1] != 0.0f || m.emission[2] != 0.0f)
            jm["emission"] = ToJson(m.emission);
        if (m.type == GLASS || m.type == WATER) jm["ior"] = m.refractive_index;
        if (m.type == CHECKERBOARD) jm["albedo2"] = ToJson(m.albedo2);
        if (m.proc_id != PROC_NONE)
            jm["procedural"] = NameOf(kProceduralPatterns, m.proc_id, "none");
        out["materials"].push_back(jm);
    }

    auto material_name = [&](i32 index) -> std::string {
        if (index >= 0 && usize(index) < scene.material_names.size())
            return scene.material_names[index];
        return "material_" + std::to_string(index);
    };

    for (const GPUSphere& s : scene.spheres) {
        out["spheres"].push_back({{"center", ToJson(s.center)},
                                  {"radius", s.radius},
                                  {"material", material_name(s.mat_index)}});
    }
    for (const GPUPlane& p : scene.planes) {
        out["planes"].push_back({{"normal", ToJson(p.normal)},
                                 {"offset", p.d_offset},
                                 {"material", material_name(p.mat_index)}});
    }
    for (const GPUCube& c : scene.cubes) {
        out["cubes"].push_back({{"center", ToJson(c.center)},
                                {"half_size", ToJson(c.half_size)},
                                {"material", material_name(c.mat_index)}});
    }
    for (const GPULight& l : scene.lights) {
        out["lights"].push_back({{"position", ToJson(l.position)},
                                 {"intensity", l.intensity},
                                 {"color", ToJson(l.color)},
                                 {"radius", l.radius}});
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        LUCIDA_ERROR(Resource, "cannot write %s", path.c_str());
        return false;
    }
    file << out.dump(2) << '\n';
    LUCIDA_INFO(Resource, "wrote scene '%s' to %s", scene.name.c_str(), path.c_str());
    return true;
}

bool LoadSceneFile(const std::string& path, RenderScene& out_scene) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LUCIDA_ERROR(Resource, "cannot open %s", path.c_str());
        return false;
    }

    json in;
    try {
        in = json::parse(file, nullptr, true, /*ignore_comments=*/true);
    } catch (const json::parse_error& e) {
        LUCIDA_ERROR(Resource, "%s: %s", path.c_str(), e.what());
        return false;
    }

    // Build into a scratch scene so a failure halfway through leaves the caller's
    // scene intact rather than half-replaced.
    RenderScene scene;
    scene.name  = in.value("name", "untitled");
    scene.model = in.value("model", std::string("whitted_gi")) == "whitted"
                      ? ShadingModel::Whitted
                      : ShadingModel::WhittedGI;

    if (in.contains("environment")) {
        const json& env = in["environment"];
        scene.environment.ambient      = ToVec3(env.value("ambient", json{}), scene.environment.ambient);
        scene.environment.fog_enabled  = env.value("fog", scene.environment.fog_enabled);
        scene.environment.fog_density  = env.value("fog_density", scene.environment.fog_density);
        scene.environment.fog_steps    = env.value("fog_steps", scene.environment.fog_steps);
    }

    if (in.contains("spawn")) {
        const json& spawn = in["spawn"];
        scene.spawn.position = ToVec3(spawn.value("position", json{}), scene.spawn.position);
        scene.spawn.yaw      = spawn.value("yaw", scene.spawn.yaw);
        scene.spawn.pitch    = spawn.value("pitch", scene.spawn.pitch);
        scene.spawn.fov_y    = spawn.value("fov_y_degrees", 60.0f) * kDegToRad;
    }

    for (const json& jm : in.value("materials", json::array())) {
        MaterialType type = DIFFUSE;
        if (!ParseEnum(kMaterialTypes, jm, "type", type)) return false;

        Material m(type);
        m.albedo           = ToVec3(jm.value("albedo", json{}), m.albedo);
        m.albedo2          = ToVec3(jm.value("albedo2", json{}), m.albedo2);
        m.emission         = ToVec3(jm.value("emission", json{}), Vec3(0.0f));
        m.roughness        = jm.value("roughness", 0.5);
        m.metallic         = jm.value("metallic", 0.0);
        m.refractive_index = jm.value("ior", 1.5);

        i32 procedural = PROC_NONE;
        if (jm.contains("procedural") && !ParseEnum(kProceduralPatterns, jm, "procedural", procedural))
            return false;

        scene.AddMaterial(m, procedural, jm.value("name", std::string{}));
    }

    // Named lookup with an index fallback, so a file written by hand without
    // names still loads.
    auto material_index = [&](const json& node, const char* owner) -> i32 {
        if (node.is_number_integer()) return node.get<i32>();
        const std::string name = node.is_string() ? node.get<std::string>() : std::string{};
        const i32 index = scene.FindMaterial(name);
        if (index < 0) {
            LUCIDA_ERROR(Resource, "%s references unknown material '%s'", owner, name.c_str());
        }
        return index;
    };

    for (const json& j : in.value("spheres", json::array())) {
        const i32 mat = material_index(j.value("material", json{}), "sphere");
        if (mat < 0) return false;
        scene.AddSphere(ToVec3(j.value("center", json{})), j.value("radius", 1.0f), mat);
    }
    for (const json& j : in.value("planes", json::array())) {
        const i32 mat = material_index(j.value("material", json{}), "plane");
        if (mat < 0) return false;
        scene.AddPlane(ToVec3(j.value("normal", json{}), Vec3(0, 1, 0)),
                       j.value("offset", 0.0f), mat);
    }
    for (const json& j : in.value("cubes", json::array())) {
        const i32 mat = material_index(j.value("material", json{}), "cube");
        if (mat < 0) return false;
        scene.AddCube(ToVec3(j.value("center", json{})),
                      ToVec3(j.value("half_size", json{}), Vec3(0.5f)), mat);
    }
    for (const json& j : in.value("lights", json::array())) {
        scene.AddLight(ToVec3(j.value("position", json{})), j.value("intensity", 10.0f),
                       ToVec3(j.value("color", json{}), Vec3(1.0f)), j.value("radius", 0.5f));
    }

    LUCIDA_INFO(Resource, "loaded scene '%s': %zu materials, %zu spheres, %zu planes, "
                          "%zu cubes, %zu lights",
                scene.name.c_str(), scene.materials.size(), scene.spheres.size(),
                scene.planes.size(), scene.cubes.size(), scene.lights.size());
    out_scene = std::move(scene);
    return true;
}

} // namespace lucida
