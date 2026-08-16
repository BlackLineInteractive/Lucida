// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/core/math/Math.h"
#include "lucida/physics/PhysicsBackend.h"

#include <memory>
#include <utility>
#include <vector>

namespace lucida {

class World;

class NativeScript {
public:
    virtual ~NativeScript() = default;

    virtual void OnStart(World& world, Entity entity) {}
    virtual void OnUpdate(World& world, Entity entity, float dt) {}
    virtual void OnCollisionEnter(World& world, Entity entity, Entity other, const ContactPoint& contact) {}
    virtual void OnCollisionExit(World& world, Entity entity, Entity other) {}
    virtual void OnTriggerEnter(World& world, Entity entity, Entity other) {}
    virtual void OnTriggerExit(World& world, Entity entity, Entity other) {}

    bool is_started = false;
};

struct ScriptComponent {
    std::vector<std::shared_ptr<NativeScript>> scripts;

    template <typename T, typename... Args>
    T& Bind(Args&&... args) {
        auto script = std::make_shared<T>(std::forward<Args>(args)...);
        T& ref = *script;
        scripts.push_back(std::move(script));
        return ref;
    }
};

} // namespace lucida
