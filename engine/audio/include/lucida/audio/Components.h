// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/audio/AudioBackend.h"

#include <string>

namespace lucida {

struct AudioSourceComponent {
    std::string sound_path;
    f32  volume        = 1.0f;
    f32  pitch         = 1.0f;
    bool loop          = false;
    bool is_3d         = true;
    f32  min_distance  = 1.0f;
    f32  max_distance  = 50.0f;
    bool play_on_start = true;
    bool is_playing    = false;

    SoundHandle handle;
};

struct AudioListenerComponent {
    f32  master_volume = 1.0f;
    bool is_active     = true;
};

} // namespace lucida
