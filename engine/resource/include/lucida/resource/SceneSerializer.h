// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Scenes on disk.
//
// A scene that exists only as C++ is a demo. This is the line the project
// crosses to become an engine: the world can be changed without a compiler.
//
// The format is JSON and is meant to be edited by hand — enums are written as
// names, materials are referenced by name rather than by index, so inserting a
// material at the top of a file does not repaint everything below it.

#include "lucida/render/Scene.h"

#include <string>

namespace lucida {

bool SaveScene(const RenderScene& scene, const std::string& path);

// Returns false and leaves out_scene untouched if the file is missing or invalid;
// the error is logged with the offending field.
bool LoadSceneFile(const std::string& path, RenderScene& out_scene);

} // namespace lucida
