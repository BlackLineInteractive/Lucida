// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// SDL2 platform backend: window, surface and input.
//
// This is the only translation unit in the project that includes SDL. The
// engine sees a SurfaceDesc and an InputState; swapping SDL for GLFW or a
// native win32 loop is a change confined to this directory.

#include "lucida/input/Input.h"
#include "lucida/render/RenderBackend.h"

#include <memory>
#include <string>

namespace lucida {

struct WindowDesc {
    std::string title  = "Lucida";
    i32  width         = 1280;
    i32  height        = 720;
    bool fullscreen    = false;
    bool resizable     = true;
    bool metal_surface = true;   // create a CAMetalLayer for the Metal backend
};

class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual bool Init(const WindowDesc& desc) = 0;
    virtual void Shutdown() = 0;

    virtual SurfaceDesc Surface() const = 0;

    // Translates OS events into actions. Returns false when the user quits.
    virtual bool PumpEvents(InputState& input) = 0;

    virtual void SetMouseCaptured(bool captured) = 0;
    virtual void ToggleFullscreen() = 0;
    virtual void GetDrawableSize(i32& width, i32& height) const = 0;

    // ImGui platform half. The graphics half belongs to the render backend.
    virtual void OverlayInit() = 0;
    virtual void OverlayNewFrame() = 0;
    virtual void OverlayShutdown() = 0;
};

std::unique_ptr<IPlatform> CreatePlatformSDL2();

} // namespace lucida
