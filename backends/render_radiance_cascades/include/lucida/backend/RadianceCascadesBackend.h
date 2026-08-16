// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Factory for the Radiance Cascades 3D render backend.

#include "lucida/render/RenderBackend.h"

#include <memory>

namespace lucida {

std::unique_ptr<IRenderBackend> CreateRadianceCascadesBackend();

} // namespace lucida
