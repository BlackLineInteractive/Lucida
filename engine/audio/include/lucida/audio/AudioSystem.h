// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/audio/AudioBackend.h"
#include "lucida/runtime/System.h"

namespace lucida {

class AudioSystem final : public ISystem {
public:
    explicit AudioSystem(IAudioBackend& backend) : m_backend(backend) {}

    const char* Name() const override { return "audio"; }
    UpdatePhase Phase() const override { return UpdatePhase::Simulation; }
    void Update(World& world, const FrameTime& time) override;

    IAudioBackend& Backend() { return m_backend; }
    const IAudioBackend& Backend() const { return m_backend; }

    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused; }

private:
    IAudioBackend& m_backend;
    bool           m_paused = false;
};

} // namespace lucida
