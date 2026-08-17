// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/ecs/Registry.h"
#include "lucida/render/Components.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Scene.h"
#include "lucida/resource/MeshBuilder.h"
#include "lucida/framework/SceneAssets.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
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
        if (!m_registry.Valid(m_entity)) return;
        if (LocalTransform* local = m_registry.Get<LocalTransform>(m_entity)) {
            *local = value;
            if (PrimitiveShape* shape = m_registry.Get<PrimitiveShape>(m_entity)) {
                const Vec3 half = shape->HalfExtents() * local->scale;
                if (LocalBounds* bounds = m_registry.Get<LocalBounds>(m_entity)) {
                    bounds->min = -half;
                    bounds->max =  half;
                }
            }
            UpdateWorldTransforms(m_registry);
        }
    }

    Registry&      m_registry;
    Entity         m_entity;
    LocalTransform m_before;
    LocalTransform m_after;
    std::string    m_name;
};

// Shape Edit Command (Dimensions, Extrusion, Radius, Height, Normals)
class ShapeEditCommand final : public ICommand {
public:
    ShapeEditCommand(Registry& registry, Entity entity,
                     const PrimitiveShape& before, const PrimitiveShape& after,
                     std::string name = "Edit Shape")
        : m_registry(registry), m_entity(entity), m_before(before), m_after(after),
          m_name(std::move(name)) {}

    void Apply() override  { Write(m_after); }
    void Revert() override { Write(m_before); }
    const char* Name() const override { return m_name.c_str(); }

private:
    void Write(const PrimitiveShape& value) {
        if (!m_registry.Valid(m_entity)) return;
        if (PrimitiveShape* shape = m_registry.Get<PrimitiveShape>(m_entity)) {
            *shape = value;
            Vec3 scale = Vec3(1.0f);
            if (const LocalTransform* lt = m_registry.Get<LocalTransform>(m_entity)) scale = lt->scale;
            const Vec3 half = shape->HalfExtents() * scale;
            if (LocalBounds* bounds = m_registry.Get<LocalBounds>(m_entity)) {
                bounds->min = -half;
                bounds->max =  half;
            }
            UpdateWorldTransforms(m_registry);
        }
    }

    Registry&      m_registry;
    Entity         m_entity;
    PrimitiveShape m_before;
    PrimitiveShape m_after;
    std::string    m_name;
};

// Mesh Sub-Element Edit Command (Vertices, Edges, Faces, Inset, Extrude, Bevel, Subdivide)
class MeshEditCommand final : public ICommand {
public:
    MeshEditCommand(Registry& registry, Entity entity,
                    const EditableMesh& before, const EditableMesh& after,
                    IRenderBackend* renderer = nullptr,
                    std::string name = "Mesh Edit")
        : m_registry(registry), m_entity(entity), m_before(before), m_after(after),
          m_renderer(renderer), m_name(std::move(name)) {}

    void Apply() override  { Write(m_after); }
    void Revert() override { Write(m_before); }
    const char* Name() const override { return m_name.c_str(); }

private:
    void Write(const EditableMesh& value) {
        if (!m_registry.Valid(m_entity)) return;
        if (EditableMeshComponent* emc = m_registry.Get<EditableMeshComponent>(m_entity)) {
            emc->mesh = value;
            emc->dirty = true;
            MeshData md = emc->mesh.BuildMeshData();
            if (MeshInstance* mi = m_registry.Get<MeshInstance>(m_entity)) {
                if (m_renderer && mi->mesh.IsValid()) {
                    m_renderer->UpdateMesh(mi->mesh, md);
                }
            }
            if (LocalBounds* bounds = m_registry.Get<LocalBounds>(m_entity)) {
                bounds->min = md.aabb_min;
                bounds->max = md.aabb_max;
            }
            UpdateWorldTransforms(m_registry);
        }
    }

    Registry&          m_registry;
    Entity             m_entity;
    EditableMesh       m_before;
    EditableMesh       m_after;
    IRenderBackend*    m_renderer = nullptr;
    std::string        m_name;
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
        UpdateWorldTransforms(m_registry);
    }

    Registry&   m_registry;
    Entity      m_entity;
    Entity      m_before;
    Entity      m_after;
    std::string m_name;
};

// Entity Snapshot for complete state capture (Create/Delete/Duplicate/PlayMode)
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
    bool has_light = false;
    LightSource light{};
    bool has_camera = false;
    CameraComponent camera{};
    bool has_terrain = false;
    TerrainComponent terrain{};
    bool has_mesh_instance = false;
    MeshInstance mesh_instance{};
    bool has_scene_graph_node = false;
    bool has_group = false;
    GroupComponent group{};
    bool has_editable_mesh = false;
    EditableMeshComponent editable_mesh{};

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
        if (const LightSource* l = reg.Get<LightSource>(e)) { s.has_light = true; s.light = *l; }
        if (const CameraComponent* c = reg.Get<CameraComponent>(e)) { s.has_camera = true; s.camera = *c; }
        if (const TerrainComponent* tr = reg.Get<TerrainComponent>(e)) { s.has_terrain = true; s.terrain = *tr; }
        if (const MeshInstance* mi = reg.Get<MeshInstance>(e)) { s.has_mesh_instance = true; s.mesh_instance = *mi; }
        if (const GroupComponent* gc = reg.Get<GroupComponent>(e)) { s.has_group = true; s.group = *gc; }
        if (const EditableMeshComponent* emc = reg.Get<EditableMeshComponent>(e)) { s.has_editable_mesh = true; s.editable_mesh = *emc; }
        if (reg.Has<SceneGraphNode>(e)) { s.has_scene_graph_node = true; }
        return s;
    }

    Entity Restore(Registry& reg, Entity target = kNullEntity, bool restore_parent = true) const {
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
        if (has_light) reg.Add<LightSource>(e, light);
        if (has_camera) reg.Add<CameraComponent>(e, camera);
        if (has_terrain) reg.Add<TerrainComponent>(e, terrain);
        if (has_mesh_instance) reg.Add<MeshInstance>(e, mesh_instance);
        if (has_group) reg.Add<GroupComponent>(e, group);
        if (has_editable_mesh) reg.Add<EditableMeshComponent>(e, editable_mesh);
        if (has_scene_graph_node) reg.Add<SceneGraphNode>(e);
        if (restore_parent && has_parent && parent_entity != kNullEntity && reg.Valid(parent_entity))
            reg.Add<Parent>(e, Parent{parent_entity});
        return e;
    }
};

// Full World / Registry Snapshot for Play Mode state save and restore
struct WorldSnapshot {
    struct EntityEntry {
        Entity original_id = kNullEntity;
        EntitySnapshot snapshot;
    };

    std::vector<EntityEntry> entities;

    static WorldSnapshot Capture(Registry& reg) {
        WorldSnapshot ws;
        for (auto entity : reg.Raw().view<Name>()) {
            if (reg.Valid(entity)) {
                ws.entities.push_back({entity, EntitySnapshot::Capture(reg, entity)});
            }
        }
        return ws;
    }

    void Restore(Registry& reg) const {
        reg.Clear();
        std::unordered_map<Entity, Entity> old_to_new;
        old_to_new.reserve(entities.size());

        // First pass: restore all entities and components except parent pointers
        for (const auto& entry : entities) {
            Entity new_e = entry.snapshot.Restore(reg, kNullEntity, false);
            old_to_new[entry.original_id] = new_e;
        }

        // Second pass: reconstruct parent hierarchy using mapped entity IDs
        for (const auto& entry : entities) {
            if (entry.snapshot.has_parent && entry.snapshot.parent_entity != kNullEntity) {
                auto it_parent = old_to_new.find(entry.snapshot.parent_entity);
                auto it_child  = old_to_new.find(entry.original_id);
                if (it_parent != old_to_new.end() && it_child != old_to_new.end()) {
                    reg.Add<Parent>(it_child->second, Parent{it_parent->second});
                }
            }
        }
        UpdateWorldTransforms(reg);
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
            UpdateWorldTransforms(m_registry);
        }
    }
    void Revert() override {
        if (m_registry.Valid(m_entity)) {
            m_snapshot = EntitySnapshot::Capture(m_registry, m_entity);
            m_registry.Destroy(m_entity);
            UpdateWorldTransforms(m_registry);
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
            m_snapshot = EntitySnapshot::Capture(m_registry, m_entity);
            m_registry.Destroy(m_entity);
            UpdateWorldTransforms(m_registry);
        }
    }
    void Revert() override {
        m_entity = m_snapshot.Restore(m_registry);
        UpdateWorldTransforms(m_registry);
    }
    const char* Name() const override { return m_name.c_str(); }

private:
    Registry&      m_registry;
    Entity         m_entity;
    EntitySnapshot m_snapshot;
    std::string    m_name;
};

// Group Selected Entities under a new Group parent node (Ctrl+G)
class GroupEntitiesCommand final : public ICommand {
public:
    GroupEntitiesCommand(Registry& registry, const std::vector<Entity>& entities,
                         std::string group_name = "Group")
        : m_registry(registry), m_group_name(std::move(group_name)), m_group_entity(kNullEntity) {
        for (Entity e : entities) {
            if (m_registry.Valid(e)) {
                m_children.push_back(e);
                m_child_snapshots.push_back(EntitySnapshot::Capture(m_registry, e));
            }
        }
    }

    void Apply() override {
        if (m_children.empty()) return;

        Vec3 center(0.0f);
        int valid_count = 0;
        for (Entity e : m_children) {
            if (m_registry.Valid(e)) {
                if (const LocalTransform* lt = m_registry.Get<LocalTransform>(e)) {
                    center += lt->position;
                    valid_count++;
                }
            }
        }
        if (valid_count > 0) center /= static_cast<f32>(valid_count);

        m_group_entity = m_registry.Create(m_group_name);
        m_registry.Add<GroupComponent>(m_group_entity, GroupComponent{m_group_name, false});
        if (LocalTransform* lt = m_registry.Get<LocalTransform>(m_group_entity)) {
            lt->position = center;
        }

        for (Entity e : m_children) {
            if (m_registry.Valid(e)) {
                if (Parent* p = m_registry.Get<Parent>(e)) {
                    p->entity = m_group_entity;
                } else {
                    m_registry.Add<Parent>(e, Parent{m_group_entity});
                }
                if (LocalTransform* lt = m_registry.Get<LocalTransform>(e)) {
                    lt->position -= center;
                }
            }
        }
        UpdateWorldTransforms(m_registry);
    }

    void Revert() override {
        for (Entity e : m_children) {
            if (m_registry.Valid(e)) {
                if (const Parent* p = m_registry.Get<Parent>(e)) {
                    if (p->entity != kNullEntity && m_registry.Valid(p->entity)) {
                        if (m_registry.Get<GroupComponent>(p->entity)) {
                            m_registry.Destroy(p->entity);
                        }
                    }
                }
            }
        }
        for (usize i = 0; i < m_children.size(); ++i) {
            Entity e = m_children[i];
            if (m_registry.Valid(e)) {
                m_child_snapshots[i].Restore(m_registry, e);
            }
        }
        if (m_registry.Valid(m_group_entity)) {
            m_registry.Destroy(m_group_entity);
            m_group_entity = kNullEntity;
        }
        UpdateWorldTransforms(m_registry);
    }

    Entity GetGroupEntity() const { return m_group_entity; }
    const char* Name() const override { return "Group Entities"; }

private:
    Registry&                   m_registry;
    std::string                 m_group_name;
    Entity                      m_group_entity;
    std::vector<Entity>         m_children;
    std::vector<EntitySnapshot> m_child_snapshots;
};

// Ungroup Command: unparents children and removes Group entity (Ctrl+Alt+G)
class UngroupEntitiesCommand final : public ICommand {
public:
    UngroupEntitiesCommand(Registry& registry, Entity group_entity)
        : m_registry(registry), m_group_entity(group_entity) {
        if (m_registry.Valid(group_entity)) {
            m_group_snapshot = EntitySnapshot::Capture(m_registry, group_entity);
            for (auto [e, parent] : m_registry.View<Parent>().each()) {
                if (parent.entity == group_entity) {
                    m_children.push_back(e);
                    m_child_snapshots.push_back(EntitySnapshot::Capture(m_registry, e));
                }
            }
        }
    }

    void Apply() override {
        if (!m_registry.Valid(m_group_entity)) return;

        Vec3 group_pos = Vec3(0.0f);
        if (const LocalTransform* glt = m_registry.Get<LocalTransform>(m_group_entity)) {
            group_pos = glt->position;
        }

        for (Entity e : m_children) {
            if (m_registry.Valid(e)) {
                if (Parent* p = m_registry.Get<Parent>(e)) {
                    p->entity = kNullEntity;
                }
                if (LocalTransform* lt = m_registry.Get<LocalTransform>(e)) {
                    lt->position += group_pos;
                }
            }
        }
        m_registry.Destroy(m_group_entity);
        UpdateWorldTransforms(m_registry);
    }

    void Revert() override {
        m_group_entity = m_group_snapshot.Restore(m_registry);
        for (usize i = 0; i < m_children.size(); ++i) {
            Entity e = m_children[i];
            if (m_registry.Valid(e)) {
                m_child_snapshots[i].Restore(m_registry, e);
                if (Parent* p = m_registry.Get<Parent>(e)) {
                    p->entity = m_group_entity;
                } else {
                    m_registry.Add<Parent>(e, Parent{m_group_entity});
                }
            }
        }
        UpdateWorldTransforms(m_registry);
    }

    const char* Name() const override { return "Ungroup Entities"; }

private:
    Registry&                   m_registry;
    Entity                      m_group_entity;
    EntitySnapshot              m_group_snapshot;
    std::vector<Entity>         m_children;
    std::vector<EntitySnapshot> m_child_snapshots;
};

// Join Meshes Command (Ctrl+J in Blender style)
class JoinMeshesCommand final : public ICommand {
public:
    JoinMeshesCommand(Registry& registry, const std::vector<Entity>& entities)
        : m_registry(registry) {
        for (Entity e : entities) {
            if (m_registry.Valid(e)) {
                m_entities.push_back(e);
                m_snapshots.push_back(EntitySnapshot::Capture(m_registry, e));
            }
        }
    }

    void Apply() override {
        if (m_entities.size() < 2) return;
        Entity target = m_entities[0];
        if (!m_registry.Valid(target)) return;

        EditableMesh merged;
        if (EditableMeshComponent* emc = m_registry.Get<EditableMeshComponent>(target)) {
            merged = emc->mesh;
        } else if (PrimitiveShape* ps = m_registry.Get<PrimitiveShape>(target)) {
            if (ps->type == PrimitiveType::Box) merged = MeshBuilder::CreateCube(ps->size);
            else if (ps->type == PrimitiveType::Sphere) merged = MeshBuilder::CreateSphere(ps->size.x);
            else if (ps->type == PrimitiveType::Cylinder) merged = MeshBuilder::CreateCylinder(ps->size.x, ps->cylinder_height);
            else merged = MeshBuilder::CreateCube(Vec3(0.5f));
        }

        Mat4 target_inv = glm::inverse(m_registry.Get<LocalTransform>(target)->ToMatrix());

        for (usize i = 1; i < m_entities.size(); ++i) {
            Entity src = m_entities[i];
            if (!m_registry.Valid(src)) continue;

            EditableMesh other;
            if (EditableMeshComponent* emc = m_registry.Get<EditableMeshComponent>(src)) {
                other = emc->mesh;
            } else if (PrimitiveShape* ps = m_registry.Get<PrimitiveShape>(src)) {
                if (ps->type == PrimitiveType::Box) other = MeshBuilder::CreateCube(ps->size);
                else if (ps->type == PrimitiveType::Sphere) other = MeshBuilder::CreateSphere(ps->size.x);
                else if (ps->type == PrimitiveType::Cylinder) other = MeshBuilder::CreateCylinder(ps->size.x, ps->cylinder_height);
                else other = MeshBuilder::CreateCube(Vec3(0.5f));
            }

            Mat4 src_to_target = target_inv * m_registry.Get<LocalTransform>(src)->ToMatrix();
            uint32_t offset = static_cast<uint32_t>(merged.vertices.size());
            for (const auto& v : other.vertices) {
                Vertex tv = v;
                tv.position = Vec3(src_to_target * Vec4(v.position, 1.0f));
                tv.normal   = glm::normalize(Vec3(src_to_target * Vec4(v.normal, 0.0f)));
                merged.vertices.push_back(tv);
            }
            for (const auto& f : other.faces) {
                TriangleFace tf = f;
                tf.i0 += offset;
                tf.i1 += offset;
                tf.i2 += offset;
                merged.faces.push_back(tf);
            }
            m_registry.Destroy(src);
        }

        if (EditableMeshComponent* emc = m_registry.Get<EditableMeshComponent>(target)) {
            emc->mesh = merged;
            emc->dirty = true;
        } else {
            m_registry.Add<EditableMeshComponent>(target, EditableMeshComponent{merged, true});
        }
        UpdateWorldTransforms(m_registry);
    }

    void Revert() override {
        for (usize i = 0; i < m_entities.size(); ++i) {
            Entity e = m_entities[i];
            if (m_registry.Valid(e)) {
                m_snapshots[i].Restore(m_registry, e);
            } else {
                m_entities[i] = m_snapshots[i].Restore(m_registry);
            }
        }
        UpdateWorldTransforms(m_registry);
    }

    const char* Name() const override { return "Join Meshes"; }

private:
    Registry&                   m_registry;
    std::vector<Entity>         m_entities;
    std::vector<EntitySnapshot> m_snapshots;
};

} // namespace lucida
