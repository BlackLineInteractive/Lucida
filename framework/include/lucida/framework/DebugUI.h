// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Debug and tuning UI. Immediate mode, so there is no widget state to keep in
// sync — the panel reads the live structs and writes them back.
//
// The framework layer is where tools live: nothing below it links ImGui.

#include "lucida/framework/CameraController.h"
#include "lucida/framework/SceneLibrary.h"
#include "lucida/render/RenderBackend.h"

#include <string>

namespace lucida {

struct UiState {
    bool show_menu     = true;
    bool request_quit  = false;
    bool request_fullscreen = false;
    scenes::BuiltIn scene = scenes::BuiltIn::WaterAndFog;
    bool request_scene_reload = false;
    std::string pending_model_path;   // non-empty when the user picked a file
};

class DebugUI {
public:
    void Init();
    void Shutdown();

    // Between platform->OverlayNewFrame() and backend->Render().
    void Build(UiState& ui, RenderSettings& settings, const RenderStats& stats,
               CameraController& camera, const FrameTime& time);

private:
    f32 m_fps_ema = 60.0f;
};

} // namespace lucida
