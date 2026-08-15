// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/Project.h"

#include "lucida/core/diag/Log.h"
#include "lucida/resource/SceneSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace lucida {
namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr const char* kProjectFile = "project.json";
constexpr i32 kFormatVersion = 1;

json RenderToJson(const RenderSettings& r) {
    return {{"samples", r.samples},
            {"max_depth", r.max_depth},
            {"debug_mode", r.debug_mode},
            {"render_scale", r.render_scale},
            {"fog", r.fog},
            {"vsync", r.vsync}};
}

RenderSettings RenderFromJson(const json& node, RenderSettings fallback) {
    if (!node.is_object()) return fallback;
    fallback.samples      = node.value("samples", fallback.samples);
    fallback.max_depth    = node.value("max_depth", fallback.max_depth);
    fallback.debug_mode   = node.value("debug_mode", fallback.debug_mode);
    fallback.render_scale = node.value("render_scale", fallback.render_scale);
    fallback.fog          = node.value("fog", fallback.fog);
    fallback.vsync        = node.value("vsync", fallback.vsync);
    return fallback;
}

// A starter scene, so a new project opens onto something rather than a void.
RenderScene StarterScene() {
    RenderScene scene;
    scene.name = "main";

    const i32 floor = scene.AddMaterial(
        Material(CHECKERBOARD, {0.8f, 0.8f, 0.8f}, {0, 0, 0}, 0.8, 0.0, 1.0,
                 {0.2f, 0.2f, 0.2f}), PROC_NONE, "floor");
    const i32 chrome = scene.AddMaterial(
        Material(METAL, {0.9f, 0.9f, 0.95f}, {0, 0, 0}, 0.05, 1.0), PROC_NONE, "chrome");

    scene.AddPlane({0, 1, 0}, -1.0f, floor);
    scene.AddSphere({0.0f, 0.0f, -4.0f}, 1.0f, chrome);
    scene.AddLight({-5.0f, 8.0f, -2.0f}, 50.0f, {1.0f, 0.95f, 0.9f}, 2.0f);

    scene.spawn.position = Vec3(0.0f, 0.0f, 2.0f);
    return scene;
}

} // namespace

bool Project::Create(const std::string& directory, const std::string& name) {
    const fs::path root(directory);
    std::error_code ec;

    if (fs::exists(root / kProjectFile)) {
        LUCIDA_ERROR(Resource, "%s already holds a project", directory.c_str());
        return false;
    }

    for (const char* sub : {"", "scenes", "assets", "scripts"}) {
        fs::create_directories(root / sub, ec);
        if (ec) {
            LUCIDA_ERROR(Resource, "cannot create %s: %s",
                         (root / sub).string().c_str(), ec.message().c_str());
            return false;
        }
    }

    if (!SaveScene(StarterScene(), (root / "scenes" / "main.json").string())) return false;

    Project project;
    project.m_root = fs::absolute(root, ec).string();
    project.m_settings.name = name;
    project.m_settings.startup_scene = "scenes/main.json";
    if (!project.Save()) return false;

    LUCIDA_INFO(Resource, "created project '%s' at %s", name.c_str(),
                project.m_root.c_str());
    return true;
}

bool Project::Open(const std::string& directory) {
    std::error_code ec;
    const fs::path root = fs::absolute(fs::path(directory), ec);
    const fs::path file = root / kProjectFile;

    std::ifstream stream(file);
    if (!stream.is_open()) {
        LUCIDA_ERROR(Resource, "no %s in %s", kProjectFile, directory.c_str());
        return false;
    }

    json in;
    try {
        in = json::parse(stream, nullptr, true, /*ignore_comments=*/true);
    } catch (const json::parse_error& e) {
        LUCIDA_ERROR(Resource, "%s: %s", file.string().c_str(), e.what());
        return false;
    }

    ProjectSettings settings;
    settings.name          = in.value("name", settings.name);
    settings.version       = in.value("version", settings.version);
    settings.startup_scene = in.value("startup_scene", std::string{});
    settings.startup_model = in.value("startup_model", std::string{});

    if (in.contains("window")) {
        const json& w = in["window"];
        settings.window_width  = w.value("width", settings.window_width);
        settings.window_height = w.value("height", settings.window_height);
        settings.fullscreen    = w.value("fullscreen", settings.fullscreen);
    }
    settings.walk_mode = in.value("walk_mode", settings.walk_mode);
    settings.render    = RenderFromJson(in.value("render", json{}), settings.render);

    m_root     = root.string();
    m_settings = std::move(settings);

    LUCIDA_INFO(Resource, "opened project '%s' at %s", m_settings.name.c_str(),
                m_root.c_str());
    return true;
}

bool Project::Save() const {
    if (m_root.empty()) return false;

    json out;
    out["format"]        = kFormatVersion;
    out["name"]          = m_settings.name;
    out["version"]       = m_settings.version;
    out["startup_scene"] = m_settings.startup_scene;
    out["startup_model"] = m_settings.startup_model;
    out["window"]        = {{"width", m_settings.window_width},
                            {"height", m_settings.window_height},
                            {"fullscreen", m_settings.fullscreen}};
    out["walk_mode"]     = m_settings.walk_mode;
    out["render"]        = RenderToJson(m_settings.render);

    const fs::path file = fs::path(m_root) / kProjectFile;
    std::ofstream stream(file);
    if (!stream.is_open()) {
        LUCIDA_ERROR(Resource, "cannot write %s", file.string().c_str());
        return false;
    }
    stream << out.dump(2) << '\n';
    return true;
}

std::string Project::Resolve(const std::string& relative) const {
    if (relative.empty()) return {};
    const fs::path path(relative);
    if (path.is_absolute() || m_root.empty()) return relative;
    return (fs::path(m_root) / path).string();
}

std::string Project::MakeRelative(const std::string& absolute) const {
    if (absolute.empty() || m_root.empty()) return absolute;

    std::error_code ec;
    const fs::path relative = fs::relative(fs::path(absolute), fs::path(m_root), ec);
    // A path outside the project stays absolute: pretending otherwise would
    // produce "../../../Users/..." and break the moment the folder moves.
    if (ec || relative.empty() || relative.native().rfind("..", 0) == 0) return absolute;
    return relative.string();
}

// ---------------------------------------------------------------- recents --

std::string RecentProjects::StoragePath() {
    namespace fs = std::filesystem;
    const char* home = std::getenv("HOME");
#if LUCIDA_PLATFORM_WINDOWS
    if (!home) home = std::getenv("USERPROFILE");
#endif
    if (!home) return "lucida_recent.json";

    std::error_code ec;
    const fs::path dir = fs::path(home) / ".config" / "lucida";
    fs::create_directories(dir, ec);
    return (dir / "recent.json").string();
}

void RecentProjects::Load() {
    m_entries.clear();
    std::ifstream stream(StoragePath());
    if (!stream.is_open()) return;

    try {
        const json in = json::parse(stream, nullptr, true, true);
        for (const json& entry : in.value("projects", json::array())) {
            if (entry.is_string()) m_entries.push_back(entry.get<std::string>());
        }
    } catch (const json::parse_error&) {
        // A corrupt recent list is not worth failing a launch over.
        LUCIDA_WARN(Resource, "recent project list is unreadable, starting fresh");
    }
}

void RecentProjects::Save() const {
    json out;
    out["projects"] = m_entries;
    std::ofstream stream(StoragePath());
    if (stream.is_open()) stream << out.dump(2) << '\n';
}

void RecentProjects::Add(const std::string& project_root) {
    if (project_root.empty()) return;
    m_entries.erase(std::remove(m_entries.begin(), m_entries.end(), project_root),
                    m_entries.end());
    m_entries.insert(m_entries.begin(), project_root);
    if (m_entries.size() > kMaxEntries) m_entries.resize(kMaxEntries);
}

} // namespace lucida
