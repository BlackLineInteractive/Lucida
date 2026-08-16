// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/audio/AudioSystem.h"
#include "lucida/audio/Components.h"
#include "lucida/render/Components.h"
#include "lucida/runtime/World.h"

namespace lucida {

namespace {

class NullAudioBackend final : public IAudioBackend {
public:
    bool Init() override { return true; }
    void Shutdown() override {}
    void Update() override {}

    SoundHandle LoadSound(const SoundDesc&) override {
        return SoundHandle{++m_next_id, 1};
    }
    void UnloadSound(SoundHandle) override {}

    void Play(SoundHandle) override {}
    void Stop(SoundHandle) override {}
    void Pause(SoundHandle) override {}

    void SetVolume(SoundHandle, f32) override {}
    void SetPitch(SoundHandle, f32) override {}
    void SetPosition(SoundHandle, const Vec3&) override {}

    void UpdateListener(const ListenerDesc&) override {}

    const char* Name() const override { return "NullAudio"; }

private:
    u32 m_next_id = 0;
};

} // namespace

std::unique_ptr<IAudioBackend> CreateNullAudioBackend() {
    return std::make_unique<NullAudioBackend>();
}

void AudioSystem::Update(World& world, const FrameTime&) {
    if (m_paused) return;

    Registry& entities = world.Entities();

    // 1. Update active listener transform
    for (auto [entity, listener, local] : entities.View<AudioListenerComponent, LocalTransform>().each()) {
        if (!listener.is_active) continue;

        ListenerDesc desc;
        desc.position = local.position;
        desc.forward  = local.rotation * Vec3(0.0f, 0.0f, -1.0f);
        desc.up       = local.rotation * Vec3(0.0f, 1.0f, 0.0f);
        desc.master_volume = listener.master_volume;
        m_backend.UpdateListener(desc);
        break; // Single primary listener
    }

    // 2. Update sound sources
    for (auto [entity, src, local] : entities.View<AudioSourceComponent, LocalTransform>().each()) {
        if (!src.handle.IsValid() && !src.sound_path.empty()) {
            SoundDesc desc;
            desc.path         = src.sound_path;
            desc.is_3d        = src.is_3d;
            desc.loop         = src.loop;
            desc.volume       = src.volume;
            desc.pitch        = src.pitch;
            desc.min_distance = src.min_distance;
            desc.max_distance = src.max_distance;

            src.handle = m_backend.LoadSound(desc);
            if (src.play_on_start) {
                m_backend.Play(src.handle);
                src.is_playing = true;
            }
        }

        if (src.handle.IsValid() && src.is_3d) {
            m_backend.SetPosition(src.handle, local.position);
        }
    }

    m_backend.Update();
}

} // namespace lucida
