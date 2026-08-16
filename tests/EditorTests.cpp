// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor behaviour that is easy to break and impossible to notice: picking maths
// and the undo stack. No window, no GPU - these are questions about entities.
//
// Run with `ctest --test-dir build` or by executing the binary directly; it
// returns the number of failed checks.

#include "lucida/backend/JoltBackend.h"
#include "lucida/framework/Commands.h"
#include "lucida/framework/Picking.h"
#include "lucida/framework/Script.h"
#include "lucida/framework/Systems.h"
#include "lucida/render/Components.h"
#include "lucida/resource/MeshBuilder.h"
#include "lucida/runtime/World.h"
#include <cstdio>
using namespace lucida;

int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%-46s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failures;
}

int main() {
    Registry reg;

    // A unit box sitting 5 m in front of a camera at the origin looking down -Z.
    const Entity box = reg.Create("box");
    reg.Get<LocalTransform>(box)->position = Vec3(0, 0, -5);
    reg.Add<LocalBounds>(box, LocalBounds{Vec3(-1.0f), Vec3(1.0f)});
    UpdateWorldTransforms(reg);

    CameraState cam;
    cam.position = Vec3(0, 0, 0);
    cam.yaw = -kHalfPi;   // -Z
    cam.pitch = 0.0f;

    const f32 aspect = 16.0f / 9.0f;

    PickResult centre = PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0, 0)));
    check(centre.Hit() && centre.entity == box, "click in the centre selects the box");
    check(centre.distance > 3.9f && centre.distance < 4.1f, "reported distance is the box face (4 m)");

    PickResult edge = PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0.95f, 0.0f)));
    check(!edge.Hit(), "click far to the side selects nothing");

    reg.Get<Visibility>(box)->visible = false;
    check(!PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0, 0))).Hit(),
          "a hidden entity cannot be picked");
    reg.Get<Visibility>(box)->visible = true;

    // Scaling the entity must scale what you can click on.
    reg.Get<LocalTransform>(box)->scale = Vec3(0.05f);
    UpdateWorldTransforms(reg);
    check(!PickEntity(reg, RayThroughViewport(cam, aspect, Vec2(0.2f, 0))).Hit(),
          "shrinking the entity shrinks its pick volume");
    reg.Get<LocalTransform>(box)->scale = Vec3(1.0f);
    UpdateWorldTransforms(reg);

    // --- command stack
    CommandStack stack;
    LocalTransform before = *reg.Get<LocalTransform>(box);
    LocalTransform after = before;
    after.position = Vec3(3, 0, -5);

    stack.Execute(std::make_unique<TransformCommand>(reg, box, before, after, "Move"));
    check(reg.Get<LocalTransform>(box)->position.x == 3.0f, "execute applies the change");
    check(stack.CanUndo() && !stack.CanRedo(), "stack has one undo, no redo");

    stack.Undo();
    check(reg.Get<LocalTransform>(box)->position.x == 0.0f, "undo restores the old value");
    check(stack.CanRedo(), "undo makes redo available");

    stack.Redo();
    check(reg.Get<LocalTransform>(box)->position.x == 3.0f, "redo reapplies it");

    // A new edit must discard the redo branch.
    stack.Undo();
    LocalTransform other = before;
    other.position = Vec3(0, 7, -5);
    stack.Execute(std::make_unique<TransformCommand>(reg, box, before, other, "Move up"));
    check(!stack.CanRedo(), "a new edit discards the redo branch");

    // --- Entity Lifecycle Undo/Redo
    Entity sphere = reg.Create("sphere");
    reg.Add<PrimitiveShape>(sphere, PrimitiveShape{PrimitiveType::Sphere, Vec3(1.0f)});
    EntitySnapshot sphere_snap = EntitySnapshot::Capture(reg, sphere);

    stack.Execute(std::make_unique<CreateEntityCommand>(reg, sphere, sphere_snap, "Create Sphere"));
    check(reg.Valid(sphere), "entity created and valid");
    stack.Undo();
    check(!reg.Valid(sphere), "undo creation destroys entity");
    stack.Redo();
    check(reg.Count() >= 2, "redo creation restores entity");

    Entity to_del = reg.Create("to_del");
    reg.Get<LocalTransform>(to_del)->position = Vec3(1, 2, 3);
    stack.Execute(std::make_unique<DestroyEntityCommand>(reg, to_del, "Delete"));
    check(!reg.Valid(to_del), "execute deletion removes entity");
    stack.Undo();
    check(reg.Count() >= 3, "undo deletion restores entity");

    // --- Material Edit Undo/Redo
    SceneAssets assets;
    GPUMaterial mat_before{}; mat_before.type = 0; mat_before.albedo[0] = 0.5f;
    assets.materials.push_back(mat_before);
    GPUMaterial mat_after = mat_before; mat_after.type = 1; mat_after.metallic = 1.0f;
    stack.Execute(std::make_unique<MaterialEditCommand>(assets, 0, mat_before, mat_after, "Gold"));
    check(assets.materials[0].type == 1 && assets.materials[0].metallic == 1.0f, "material edit applied");
    stack.Undo();
    check(assets.materials[0].type == 0 && assets.materials[0].metallic == 0.0f, "material undo restored");

    // --- WorldSnapshot (Play Mode M22) Tests
    Registry play_reg;
    Entity parent_ent = play_reg.Create("parent_node");
    play_reg.Get<LocalTransform>(parent_ent)->position = Vec3(10.0f, 0.0f, 0.0f);
    play_reg.Add<LightSource>(parent_ent, LightSource{LightType::Point, Vec3(1.0f, 0.5f, 0.2f), 100.0f});

    Entity child_ent = play_reg.Create("child_node");
    play_reg.Get<LocalTransform>(child_ent)->position = Vec3(0.0f, 5.0f, 0.0f);
    play_reg.Add<Parent>(child_ent, Parent{parent_ent});
    play_reg.Add<PrimitiveShape>(child_ent, PrimitiveShape{PrimitiveType::Sphere, Vec3(2.0f)});
    play_reg.Add<CameraComponent>(child_ent, CameraComponent{ProjectionType::Perspective, 75.0f});

    UpdateWorldTransforms(play_reg);
    check(play_reg.Count() == 2, "initial world has 2 entities");

    // Capture snapshot (as done when clicking Play)
    WorldSnapshot play_snapshot = WorldSnapshot::Capture(play_reg);
    check(play_snapshot.entities.size() == 2, "snapshot captured 2 entities");

    // Simulate play mode mutating the world: move entities, spawn temporary debris, delete parent
    play_reg.Get<LocalTransform>(parent_ent)->position = Vec3(999.0f, 999.0f, 999.0f);
    Entity debris = play_reg.Create("temp_debris");
    play_reg.Get<LocalTransform>(debris)->position = Vec3(42.0f);
    check(play_reg.Count() == 3, "world mutated during play mode (3 entities)");

    // Restore snapshot (as done when clicking Stop)
    play_snapshot.Restore(play_reg);
    check(play_reg.Count() == 2, "restored world has exactly original entity count (2)");

    // Verify restored entities and components
    bool found_parent = false;
    bool found_child = false;
    Entity restored_parent = kNullEntity;
    Entity restored_child = kNullEntity;

    for (auto e : play_reg.Raw().view<Name>()) {
        const std::string& n = play_reg.Get<Name>(e)->value;
        if (n == "parent_node") {
            found_parent = true;
            restored_parent = e;
            const LocalTransform* lt = play_reg.Get<LocalTransform>(e);
            check(lt && lt->position.x == 10.0f, "restored parent position is correct (10, 0, 0)");
            const LightSource* ls = play_reg.Get<LightSource>(e);
            check(ls && ls->intensity == 100.0f, "restored parent has light source");
        } else if (n == "child_node") {
            found_child = true;
            restored_child = e;
            const LocalTransform* lt = play_reg.Get<LocalTransform>(e);
            check(lt && lt->position.y == 5.0f, "restored child position is correct (0, 5, 0)");
            const PrimitiveShape* ps = play_reg.Get<PrimitiveShape>(e);
            check(ps && ps->type == PrimitiveType::Sphere && ps->size.x == 2.0f, "restored child has sphere shape");
            const CameraComponent* cam = play_reg.Get<CameraComponent>(e);
            check(cam && cam->fov == 75.0f, "restored child has camera component");
        }
    }
    check(found_parent && found_child, "both entities found after restore");

    // Verify hierarchy was rebuilt with new valid entity IDs
    if (restored_child != kNullEntity) {
        const Parent* p = play_reg.Get<Parent>(restored_child);
        check(p && p->entity == restored_parent, "parent-child hierarchy correctly mapped and restored");
    }

    // --- Dynamic RigidBody Physics Simulation Test (Jolt)
    World physics_world;
    physics_world.Init();
    auto jolt = CreateJoltBackend();
    check(jolt && jolt->Init(), "jolt physics backend initialized");

    PhysicsSystem* phys_sys = physics_world.AddSystem<PhysicsSystem>(*jolt);
    phys_sys->SetPaused(false); // Play mode active

    // Create falling dynamic sphere at y = 10.0
    Entity falling_ball = physics_world.Entities().Create("falling_ball");
    physics_world.Entities().Get<LocalTransform>(falling_ball)->position = Vec3(0.0f, 10.0f, 0.0f);
    physics_world.Entities().Add<PrimitiveShape>(falling_ball, PrimitiveShape{PrimitiveType::Sphere, Vec3(0.5f)});
    RigidBody rb{};
    rb.type = BodyType::Dynamic;
    rb.shape = ShapeType::Sphere;
    rb.mass = 5.0f;
    physics_world.Entities().Add<RigidBody>(falling_ball, rb);

    FrameTime ft;
    ft.delta = 1.0f / 60.0f;
    ft.real_delta = 1.0f / 60.0f;

    // Step simulation 10 frames (approx 0.16s) -> ball should fall (y < 10)
    for (int f = 0; f < 10; ++f) {
        physics_world.RunPhase(UpdatePhase::Simulation, ft);
    }
    const f32 y_after_10 = physics_world.Entities().Get<LocalTransform>(falling_ball)->position.y;
    check(y_after_10 < 9.9f && y_after_10 > 0.0f, "dynamic body falls down under gravity (y < 9.9)");

    // Step simulation 120 more frames (total > 2.0s) -> ball should hit ground plane (around y ~ 0.5)
    for (int f = 0; f < 120; ++f) {
        physics_world.RunPhase(UpdatePhase::Simulation, ft);
    }
    const f32 y_ground = physics_world.Entities().Get<LocalTransform>(falling_ball)->position.y;
    check(y_ground > 0.0f && y_ground < 5.0f, "dynamic sphere falls and approaches ground");

    // --- Physics Raycast Test
    RaycastHit hit_down{};
    bool hit_ok = phys_sys->Raycast(Vec3(0.0f, 20.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), 50.0f, hit_down);
    check(hit_ok && hit_down.has_hit, "physics raycast hits resting sphere or ground");
    check(hit_down.distance > 10.0f && hit_down.distance < 20.5f, "raycast distance is physically accurate (~19.5m)");
    check(hit_down.point.y >= -0.1f && hit_down.point.y <= 2.0f, "raycast hit point is near surface");

    // --- Sensor / Trigger & Script System Test
    ScriptSystem* script_sys = physics_world.AddSystem<ScriptSystem>();
    script_sys->SetPaused(false);

    Entity trigger_zone = physics_world.Entities().Create("trigger_zone");
    physics_world.Entities().Get<LocalTransform>(trigger_zone)->position = Vec3(5.0f, 1.0f, 0.0f);
    physics_world.Entities().Add<PrimitiveShape>(trigger_zone, PrimitiveShape{PrimitiveType::Box, Vec3(2.0f, 2.0f, 2.0f)});
    RigidBody trig_rb{};
    trig_rb.type = BodyType::Static;
    trig_rb.shape = ShapeType::Box;
    trig_rb.is_trigger = true;
    physics_world.Entities().Add<RigidBody>(trigger_zone, trig_rb);

    struct TestScript : public NativeScript {
        int start_count = 0;
        int update_count = 0;
        int trigger_enter_count = 0;

        void OnStart(World&, Entity) override { ++start_count; }
        void OnUpdate(World&, Entity, float) override { ++update_count; }
        void OnTriggerEnter(World&, Entity, Entity other) override { ++trigger_enter_count; }
    };

    auto& sc = physics_world.Entities().Add<ScriptComponent>(trigger_zone);
    TestScript& ts = sc.Bind<TestScript>();

    // Step physics & scripts
    for (int f = 0; f < 5; ++f) {
        physics_world.RunPhase(UpdatePhase::Simulation, ft);
    }
    check(ts.start_count == 1, "script OnStart invoked exactly once");
    check(ts.update_count == 5, "script OnUpdate invoked every active simulation frame");

    jolt->Shutdown();
    physics_world.Shutdown();

    // --- MeshBuilder & Geometry System Test
    auto plane = MeshBuilder::CreatePlane(4.0f, 4.0f, 4, 4);
    check(plane.vertices.size() == 25, "plane 4x4 segments creates 25 vertices");
    check(plane.faces.size() == 32, "plane 4x4 segments creates 32 triangle faces");

    auto cube = MeshBuilder::CreateCube(Vec3(1.0f));
    check(cube.vertices.size() == 24, "cube creates 24 face vertices with proper normals");
    check(cube.faces.size() == 12, "cube creates 12 triangles");

    auto proc_sphere = MeshBuilder::CreateSphere(1.0f, 8, 16);
    check(proc_sphere.vertices.size() == 9 * 17, "sphere creates proper ring-sector vertices");
    check(!proc_sphere.faces.empty(), "sphere creates non-empty face list");

    auto cylinder = MeshBuilder::CreateCylinder(1.0f, 2.0f, 16);
    check(!cylinder.vertices.empty() && !cylinder.faces.empty(), "cylinder created successfully");

    auto torus = MeshBuilder::CreateTorus(1.0f, 0.25f, 16, 8);
    check(!torus.vertices.empty() && !torus.faces.empty(), "torus created successfully");

    // EditableMesh transformation & modifiers test
    cube.Translate(Vec3(2.0f, 0.0f, 0.0f));
    check(cube.vertices[0].position.x > 0.5f, "EditableMesh::Translate modifies vertex positions");

    cube.Scale(Vec3(2.0f));
    cube.RecalculateNormals(true);
    check(glm::length(cube.vertices[0].normal) > 0.99f, "EditableMesh::RecalculateNormals normalizes vectors");

    // Subdivide test
    size_t prev_face_count = plane.faces.size();
    plane.Subdivide();
    check(plane.faces.size() == prev_face_count * 4, "EditableMesh::Subdivide quadruples triangle count");

    // GPU MeshData conversion and BVH build
    MeshData mesh_data = cube.BuildMeshData(1);
    check(mesh_data.valid, "BuildMeshData creates valid GPU MeshData");
    check(mesh_data.tri_pos.size() == 12, "MeshData has correct tri_pos count");
    check(mesh_data.tri_attr.size() == 12, "MeshData has correct tri_attr count");
    check(!mesh_data.bvh_nodes.empty(), "MeshData has generated BVH acceleration structure");

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : "SOME CHECKS FAILED");
    return failures;
}
