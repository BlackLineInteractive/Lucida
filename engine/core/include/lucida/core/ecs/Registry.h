// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Entity storage (DOD ch.4, GPP ch.5: Component).
//
// A thin facade over EnTT. Thin on purpose and honest about it: an ECS worth
// using is a template library, so hiding it completely behind a PIMPL would
// throw away the typed views that are the entire point. What this buys is one
// place that names the types - `lucida::Entity`, `lucida::Registry` - so the
// day EnTT is replaced, the change is here rather than in every system.
//
// The rule that still holds: application and gameplay code includes this
// header, never <entt/entt.hpp>.

#include "lucida/core/math/Math.h"

#include <entt/entt.hpp>

#include <string>

namespace lucida {

using Entity = entt::entity;
inline constexpr Entity kNullEntity = entt::null;

// --- Components every world has -------------------------------------------
// Kept in core because they carry no rendering, physics or platform meaning:
// a name is a name, and a transform is a transform.

struct Name {
    std::string value;
};

// Local transform. World transforms are derived, never stored: two copies of
// the same truth drift the moment one is written and the other is not.
struct LocalTransform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    Mat4 ToMatrix() const {
        Mat4 m = glm::mat4_cast(rotation);
        m[0] *= scale.x; m[1] *= scale.y; m[2] *= scale.z;
        m[3] = Vec4(position, 1.0f);
        return m;
    }
};

// Computed once per frame from LocalTransform and the parent chain.
struct WorldTransform {
    Mat4 matrix{1.0f};
};

// Hierarchy as a component rather than node pointers (DOD ch.2: relationships
// are data). Children are found by scanning for this parent, which is cheap at
// editor scale and avoids a second list to keep in sync.
struct Parent {
    Entity entity = kNullEntity;
};

struct Visibility {
    bool visible = true;
};

// --- Registry --------------------------------------------------------------

class Registry {
public:
    Entity Create(const std::string& name = {}) {
        const Entity e = m_registry.create();
        m_registry.emplace<Name>(e, name.empty() ? "entity" : name);
        m_registry.emplace<LocalTransform>(e);
        m_registry.emplace<WorldTransform>(e);
        m_registry.emplace<Visibility>(e);
        return e;
    }

    void Destroy(Entity e) {
        if (Valid(e)) m_registry.destroy(e);
    }

    bool Valid(Entity e) const { return m_registry.valid(e); }

    template <typename T, typename... Args>
    T& Add(Entity e, Args&&... args) {
        return m_registry.emplace_or_replace<T>(e, static_cast<Args&&>(args)...);
    }

    template <typename T>
    void Remove(Entity e) { m_registry.remove<T>(e); }

    template <typename T>
    T* Get(Entity e) { return m_registry.try_get<T>(e); }

    template <typename T>
    const T* Get(Entity e) const { return m_registry.try_get<T>(e); }

    template <typename... T>
    bool Has(Entity e) const { return m_registry.all_of<T...>(e); }

    // for (auto [entity, transform, mesh] : world.Entities().View<LocalTransform, MeshInstance>().each())
    template <typename... T>
    auto View() { return m_registry.view<T...>(); }

    usize Count() const { return m_registry.storage<Entity>()->in_use(); }

    void Clear() { m_registry.clear(); }

    // Escape hatch for code that genuinely needs EnTT itself (serialisation,
    // the editor's property inspector). Everything else uses the wrapper.
    entt::registry& Raw() { return m_registry; }

private:
    entt::registry m_registry;
};

// Resolves a local transform against its parent chain. Depth is bounded by the
// hierarchy, and a cycle would hang, so it is capped and reported instead.
Mat4 ComputeWorldTransform(Registry& registry, Entity entity);

// Refreshes WorldTransform for every entity. Call once per frame, before
// anything reads world positions.
void UpdateWorldTransforms(Registry& registry);

} // namespace lucida
