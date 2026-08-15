// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// World: the systems registry and the per-frame scratch memory they share.
//
// It holds no gameplay logic. Systems are ordered by phase, and within a phase
// by registration order, so the update order is explicit rather than emergent.

#include "lucida/core/diag/Log.h"
#include "lucida/core/ecs/Registry.h"
#include "lucida/core/memory/FrameArena.h"
#include "lucida/runtime/System.h"

#include <memory>
#include <vector>

namespace lucida {

class World {
public:
    void Init(usize frame_arena_bytes = 8u << 20);
    void Shutdown();

    // Takes ownership; returns a borrowed pointer for the caller to keep.
    template <typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        auto system = std::make_unique<T>(static_cast<Args&&>(args)...);
        T* raw = system.get();
        Attach(std::move(system));
        return raw;
    }

    void RunPhase(UpdatePhase phase, const FrameTime& time);

    FrameArena& Arena() { return m_arena; }

    // The world's entities. Systems iterate views over this; nothing keeps a
    // parallel list of objects on the side.
    Registry& Entities() { return m_entities; }
    const Registry& Entities() const { return m_entities; }

    // Frame boundary: flips the scratch arena.
    void BeginFrame();

private:
    void Attach(std::unique_ptr<ISystem> system);

    std::vector<std::unique_ptr<ISystem>> m_systems;
    Registry   m_entities;
    FrameArena m_arena;
};

} // namespace lucida
