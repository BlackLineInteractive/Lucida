// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/render/Components.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Scene.h"
#include "lucida/framework/SceneAssets.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lucida {

class ICommand {
public:
    virtual ~ICommand() = default;

    virtual void Apply() = 0;
    virtual void Revert() = 0;
    virtual const char* Name() const = 0;
};

class CommandStack {
public:
    // Applies the command and takes ownership.
    void Execute(std::unique_ptr<ICommand> command);

    // Pushes an already-applied change.
    void Push(std::unique_ptr<ICommand> command);

    bool Undo();
    bool Redo();

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    const char* UndoName() const { return m_undo.empty() ? "" : m_undo.back()->Name(); }
    const char* RedoName() const { return m_redo.empty() ? "" : m_redo.back()->Name(); }

    void Clear();
    usize Depth() const { return m_undo.size(); }

private:
    static constexpr usize kMaxDepth = 256;

    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
};

// Generic Lambda Command for arbitrary actions
class LambdaCommand final : public ICommand {
public:
    LambdaCommand(std::function<void()> apply, std::function<void()> revert, std::string name)
        : m_apply(std::move(apply)), m_revert(std::move(revert)), m_name(std::move(name)) {}

    void Apply() override  { if (m_apply) m_apply(); }
    void Revert() override { if (m_revert) m_revert(); }
    const char* Name() const override { return m_name.c_str(); }

private:
    std::function<void()> m_apply;
    std::function<void()> m_revert;
    std::string m_name;
};

// Transform Command
class TransformCommand final : public ICommand {
public:
    TransformCommand(Registry& registry, Entity entity,
                     const LocalTransform& before, const LocalTransform& after,
                     std::string name = "Transform")
        : m_registry(registry), m_entity(entity), m_before(before), m_after(after),
          m_name(std::move(name)) {}

    void Apply() override  { Write(m_after); }
    void Revert() override { Write(m_before); }
    const char* Name() const override { return m_name.c_str(); }

private:
    void Write(const LocalTransform& value) {
        if (LocalTransform* local = m_registry.Get<LocalTransform>(m_entity)) *local = value;
    }

    Registry&      m_registry;
    Entity         m_entity;
    LocalTransform m_before;
    LocalTransform m_after;
    std::string    m_name;
};

// Material Command
class MaterialEditCommand final : public ICommand {
public:
    MaterialEditCommand(SceneAssets& assets, i32 mat_index,
                        const GPUMaterial& before, const GPUMaterial& after,
                        std::string name = "Edit Material")
        : m_assets(assets), m_mat_index(mat_index), m_before(before), m_after(after),
          m_name(std::move(name)) {}

    void Apply() override {
        if (m_mat_index >= 0 && m_mat_index < int(m_assets.materials.size()))
            m_assets.materials[m_mat_index] = m_after;
    }
    void Revert() override {
        if (m_mat_index >= 0 && m_mat_index < int(m_assets.materials.size()))
            m_assets.materials[m_mat_index] = m_before;
    }
    const char* Name() const override { return m_name.c_str(); }

private:
    SceneAssets& m_assets;
    i32          m_mat_index;
    GPUMaterial  m_before;
    GPUMaterial  m_after;
    std::string  m_name;
};

// Reparent Command
class ReparentCommand final : public ICommand {
public:
    ReparentCommand(Registry& registry, Entity entity, Entity before_parent, Entity after_parent,
                    std::string name = "Reparent")
        : m_registry(registry), m_entity(entity), m_before(before_parent), m_after(after_parent),
          m_name(std::move(name)) {}

    void Apply() override { SetParent(m_after); }
    void Revert() override { SetParent(m_before); }
    const char* Name() const override { return m_name.c_str(); }

private:
    void SetParent(Entity p) {
        if (!m_registry.Valid(m_entity)) return;
        if (p != kNullEntity && m_registry.Valid(p)) {
            m_registry.Add<Parent>(m_entity, Parent{p});
        } else {
            m_registry.Remove<Parent>(m_entity);
        }
    }

    Registry&   m_registry;
    Entity      m_entity;
    Entity      m_before;
    Entity      m_after;
    std::string m_name;
};

// Entity Snapshot for complete state capture (Create/Delete/Duplicate)
struct EntitySnapshot {
    std::string name;
    bool has_transform = false;
    LocalTransform transform{};
    bool has_primitive = false;
    PrimitiveShape primitive{};
    bool has_mat_ref = false;
    MaterialRef mat_ref{};
    bool has_bounds = false;
    LocalBounds bounds{};
    bool has_visibility = false;
    Visibility visibility{true};
    bool has_rigid_body = false;
    RigidBody rigid_body{};
    bool has_parent = false;
    Entity parent_entity = kNullEntity;

    static EntitySnapshot Capture(Registry& reg, Entity e) {
        EntitySnapshot s;
        if (!reg.Valid(e)) return s;
        if (const Name* n = reg.Get<Name>(e)) s.name = n->value;
        if (const LocalTransform* t = reg.Get<LocalTransform>(e)) { s.has_transform = true; s.transform = *t; }
        if (const PrimitiveShape* p = reg.Get<PrimitiveShape>(e)) { s.has_primitive = true; s.primitive = *p; }
        if (const MaterialRef* m = reg.Get<MaterialRef>(e)) { s.has_mat_ref = true; s.mat_ref = *m; }
        if (const LocalBounds* b = reg.Get<LocalBounds>(e)) { s.has_bounds = true; s.bounds = *b; }
        if (const Visibility* v = reg.Get<Visibility>(e)) { s.has_visibility = true; s.visibility = *v; }
        if (const RigidBody* rb = reg.Get<RigidBody>(e)) { s.has_rigid_body = true; s.rigid_body = *rb; }
        if (const Parent* pr = reg.Get<Parent>(e)) { s.has_parent = true; s.parent_entity = pr->entity; }
        return s;
    }

    Entity Restore(Registry& reg, Entity target = kNullEntity) const {
        Entity e = (target != kNullEntity && reg.Valid(target)) ? target : reg.Create(name);
        if (Name* n = reg.Get<Name>(e)) n->value = name;
        if (has_transform) {
            if (LocalTransform* t = reg.Get<LocalTransform>(e)) *t = transform;
            else reg.Add<LocalTransform>(e, transform);
        }
        if (has_primitive) reg.Add<PrimitiveShape>(e, primitive);
        if (has_mat_ref) reg.Add<MaterialRef>(e, mat_ref);
        if (has_bounds) reg.Add<LocalBounds>(e, bounds);
        if (has_visibility) reg.Add<Visibility>(e, visibility);
        if (has_rigid_body) reg.Add<RigidBody>(e, rigid_body);
        if (has_parent && parent_entity != kNullEntity && reg.Valid(parent_entity))
            reg.Add<Parent>(e, Parent{parent_entity});
        return e;
    }
};

// Create Entity Command
class CreateEntityCommand final : public ICommand {
public:
    CreateEntityCommand(Registry& registry, Entity created_entity, EntitySnapshot snapshot,
                        std::string name = "Create Entity")
        : m_registry(registry), m_entity(created_entity), m_snapshot(std::move(snapshot)),
          m_name(std::move(name)) {}

    void Apply() override {
        if (!m_registry.Valid(m_entity)) {
            m_entity = m_snapshot.Restore(m_registry);
        }
    }
    void Revert() override {
        if (m_registry.Valid(m_entity)) {
            m_registry.Destroy(m_entity);
        }
    }
    const char* Name() const override { return m_name.c_str(); }

private:
    Registry&      m_registry;
    Entity         m_entity;
    EntitySnapshot m_snapshot;
    std::string    m_name;
};

// Destroy Entity Command
class DestroyEntityCommand final : public ICommand {
public:
    DestroyEntityCommand(Registry& registry, Entity entity,
                         std::string name = "Delete Entity")
        : m_registry(registry), m_entity(entity),
          m_snapshot(EntitySnapshot::Capture(registry, entity)),
          m_name(std::move(name)) {}

    void Apply() override {
        if (m_registry.Valid(m_entity)) {
            m_registry.Destroy(m_entity);
        }
    }
    void Revert() override {
        m_entity = m_snapshot.Restore(m_registry);
    }
    const char* Name() const override { return m_name.c_str(); }

private:
    Registry&      m_registry;
    Entity         m_entity;
    EntitySnapshot m_snapshot;
    std::string    m_name;
};

} // namespace lucida
