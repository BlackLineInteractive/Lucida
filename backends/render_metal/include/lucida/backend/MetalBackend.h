// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Factory for the Metal ray tracing backend. This is the only header the app
// includes to get Metal; everything else talks to IRenderBackend.

#include "lucida/render/RenderBackend.h"

#include <memory>

namespace lucida {

std::unique_ptr<IRenderBackend> CreateMetalBackend();

} // namespace lucida
