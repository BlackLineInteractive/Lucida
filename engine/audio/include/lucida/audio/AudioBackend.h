// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Audio subsystem interface (GEA ch.13 Audio Engine).

#include "lucida/core/container/Handle.h"
#include "lucida/core/math/Math.h"

#include <memory>
#include <string>

namespace lucida {

LUCIDA_DECLARE_HANDLE(SoundHandle);

struct SoundDesc {
    std::string path;
    bool is_3d         = false;
    bool loop          = false;
    f32  volume        = 1.0f;
    f32  pitch         = 1.0f;
    f32  min_distance  = 1.0f;
    f32  max_distance  = 50.0f;
};

struct ListenerDesc {
    Vec3 position{0.0f};
    Vec3 forward{0.0f, 0.0f, -1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    f32  master_volume = 1.0f;
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;

    virtual SoundHandle LoadSound(const SoundDesc& desc) = 0;
    virtual void        UnloadSound(SoundHandle sound) = 0;

    virtual void Play(SoundHandle sound) = 0;
    virtual void Stop(SoundHandle sound) = 0;
    virtual void Pause(SoundHandle sound) = 0;

    virtual void SetVolume(SoundHandle sound, f32 volume) = 0;
    virtual void SetPitch(SoundHandle sound, f32 pitch) = 0;
    virtual void SetPosition(SoundHandle sound, const Vec3& position) = 0;

    virtual void UpdateListener(const ListenerDesc& listener) = 0;

    virtual const char* Name() const = 0;
};

std::unique_ptr<IAudioBackend> CreateNullAudioBackend();

} // namespace lucida
