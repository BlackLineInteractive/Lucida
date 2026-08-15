// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/runtime/World.h"

namespace lucida {

void World::Init(usize frame_arena_bytes) {
    m_arena.Init(frame_arena_bytes);
    LUCIDA_INFO(Runtime, "world up, frame arena %.1f MiB x2",
                double(frame_arena_bytes) / (1024.0 * 1024.0));
}

void World::Shutdown() {
    for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it) {
        (*it)->OnDetach(*this);   // reverse order: later systems may depend on earlier
    }
    m_systems.clear();
    m_entities.Clear();
    m_arena.Shutdown();
}

void World::Attach(std::unique_ptr<ISystem> system) {
    LUCIDA_DEBUG(Runtime, "system '%s' attached", system->Name());
    system->OnAttach(*this);
    m_systems.push_back(std::move(system));
}

void World::BeginFrame() {
    m_arena.Flip();
    // World transforms are derived state: refresh them once, at a defined point,
    // before any system reads a world position.
    UpdateWorldTransforms(m_entities);
}

void World::RunPhase(UpdatePhase phase, const FrameTime& time) {
    for (auto& system : m_systems) {
        if (system->Phase() == phase) system->Update(*this, time);
    }
}

} // namespace lucida
