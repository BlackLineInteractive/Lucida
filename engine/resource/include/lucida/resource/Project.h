// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// A game is a folder.
//
//   MyGame/
//   ├── project.json      name, startup scene, window and render defaults
//   ├── scenes/           .json scenes
//   ├── assets/           models and textures
//   └── scripts/          Lua, once M17 lands
//
// Everything the project references is stored relative to its root, so the
// folder can be zipped, moved to another machine and opened there. That is the
// whole test: an absolute path anywhere in a project file is a bug.

#include "lucida/render/RenderBackend.h"

#include <string>
#include <vector>

namespace lucida {

struct ProjectSettings {
    std::string name    = "Untitled";
    std::string version = "0.1.0";

    // Project-relative, e.g. "scenes/main.json". Empty means the built-in scene.
    std::string startup_scene;
    std::string startup_model;

    i32  window_width  = 1280;
    i32  window_height = 720;
    bool fullscreen    = false;
    bool walk_mode     = true;

    RenderSettings render;
};

class Project {
public:
    // Scaffolds the folder layout, writes project.json and a starter scene.
    // Refuses to touch a directory that already holds a project.
    static bool Create(const std::string& directory, const std::string& name);

    bool Open(const std::string& directory);
    bool Save() const;

    bool IsOpen() const { return !m_root.empty(); }
    const std::string& Root() const { return m_root; }

    ProjectSettings&       Settings()       { return m_settings; }
    const ProjectSettings& Settings() const { return m_settings; }

    // Project-relative to absolute, and back. Resolve returns the input
    // untouched when it is already absolute, so a path pasted from elsewhere
    // still works — it just will not survive being moved.
    std::string Resolve(const std::string& relative) const;
    std::string MakeRelative(const std::string& absolute) const;

private:
    std::string     m_root;
    ProjectSettings m_settings;
};

// Recently opened projects, stored per user rather than per project — it is
// state about the person, not about the game.
class RecentProjects {
public:
    void Load();
    void Save() const;
    void Add(const std::string& project_root);

    const std::vector<std::string>& Entries() const { return m_entries; }

private:
    static std::string StoragePath();

    std::vector<std::string> m_entries;
    static constexpr usize kMaxEntries = 10;
};

} // namespace lucida
