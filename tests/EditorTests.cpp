// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Editor behaviour that is easy to break and impossible to notice: picking maths
// and the undo stack. No window, no GPU - these are questions about entities.
//
// Run with `ctest --test-dir build` or by executing the binary directly; it
// returns the number of failed checks.

#include "lucida/animation/AnimationClip.h"
#include "lucida/animation/AnimationSystem.h"
#include "lucida/animation/Skeleton.h"
#include "lucida/audio/AudioSystem.h"
#include "lucida/audio/Components.h"
#include "lucida/backend/JoltBackend.h"
#include "lucida/core/math/Frustum.h"
#include "lucida/core/math/Tween.h"
#include "lucida/framework/Commands.h"
#include "lucida/framework/Picking.h"
#include "lucida/framework/Script.h"
#include "lucida/framework/Systems.h"
#include "lucida/render/Components.h"
#include "lucida/resource/MeshBuilder.h"
#include "lucida/resource/Prefab.h"
#include "lucida/resource/TextureManager.h"
#include "lucida/runtime/Particles.h"
#include "lucida/runtime/DebugDraw.h"
#include "lucida/runtime/GameplayComponents.h"
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

    // --- Spatial Math & Frustum Culling Test
    Mat4 proj = glm::perspective(60.0f * kDegToRad, 16.0f / 9.0f, 0.1f, 100.0f);
    Mat4 view = glm::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Frustum frustum = Frustum::FromMatrix(proj * view);

    check(frustum.ContainsPoint(Vec3(0.0f, 0.0f, 0.0f)), "frustum contains center point");
    check(!frustum.ContainsPoint(Vec3(0.0f, 0.0f, 10.0f)), "frustum rejects point behind camera");
    check(frustum.Intersects(AABB{Vec3(-0.5f), Vec3(0.5f)}), "frustum intersects central AABB");
    check(!frustum.Intersects(AABB{Vec3(50.0f, 50.0f, 0.0f), Vec3(60.0f, 60.0f, 1.0f)}), "frustum rejects distant off-screen AABB");

    // --- Tween & Timer Test
    check(Ease(EaseType::Linear, 0.5f) == 0.5f, "linear easing returns identity");
    check(Ease(EaseType::OutQuad, 0.5f) == 0.75f, "out-quad easing matches quadratic curve");
    check(Ease(EaseType::OutBounce, 1.0f) == 1.0f, "out-bounce completes at exactly 1.0");

    Timer timer(1.0f, false);
    check(!timer.Tick(0.4f) && !timer.Finished(), "timer ticks without early firing");
    check(timer.Progress() == 0.4f, "timer reports exact 0.4 progress");
    check(timer.Tick(0.6f) && timer.Finished(), "timer completes and fires after duration reached");

    // --- DOD Particle Simulation System Test
    World particle_world;
    particle_world.Init();
    ParticleSimulationSystem* part_sys = particle_world.AddSystem<ParticleSimulationSystem>();

    Entity emitter_ent = particle_world.Entities().Create("emitter");
    particle_world.Entities().Get<LocalTransform>(emitter_ent)->position = Vec3(0.0f, 1.0f, 0.0f);
    auto& emitter = particle_world.Entities().Add<ParticleEmitterComponent>(emitter_ent);
    emitter.max_particles = 100;
    emitter.emission_rate = 50.0f;

    FrameTime p_ft{0.1f, 0.1f, 0.1f, 1};
    particle_world.RunPhase(UpdatePhase::Simulation, p_ft);

    check(emitter.active_count > 0, "particle emitter spawned active particles in SoA buffers");
    check(emitter.positions[0].y != 0.0f, "particle position updated by simulation");
    particle_world.Shutdown();

    // --- Audio Subsystem Architecture Test
    World audio_world;
    audio_world.Init();
    auto audio_backend = CreateNullAudioBackend();
    check(audio_backend && audio_backend->Init(), "null audio backend initialized");

    AudioSystem* audio_sys = audio_world.AddSystem<AudioSystem>(*audio_backend);
    Entity audio_ent = audio_world.Entities().Create("sound_source");
    auto& src = audio_world.Entities().Add<AudioSourceComponent>(audio_ent);
    src.sound_path = "assets/sound/engine.wav";
    src.play_on_start = true;

    audio_world.RunPhase(UpdatePhase::Simulation, p_ft);
    check(src.handle.IsValid() && src.is_playing, "audio source loaded and playing via AudioSystem");
    audio_world.Shutdown();

    // --- Skeletal Animation & Skinning Palette Test
    World anim_world;
    anim_world.Init();
    AnimationSystem* anim_sys = anim_world.AddSystem<AnimationSystem>();

    auto skel = std::make_shared<Skeleton>();
    Joint root_j{"Root", -1, Transform{Vec3(0, 0, 0)}, Mat4(1.0f)};
    Joint arm_j{"Arm", 0, Transform{Vec3(0, 2.0f, 0)}, Mat4(1.0f)};
    skel->joints.push_back(root_j);
    skel->joints.push_back(arm_j);

    auto clip = std::make_shared<AnimationClip>();
    clip->name = "ArmRotate";
    clip->duration = 1.0f;
    JointTrack track;
    track.joint_index = 1;
    track.rotation_keys.push_back({0.0f, Quat(1.0f, 0.0f, 0.0f, 0.0f)});
    track.rotation_keys.push_back({1.0f, glm::angleAxis(kHalfPi, Vec3(0.0f, 0.0f, 1.0f))});
    clip->tracks.push_back(track);

    Entity char_ent = anim_world.Entities().Create("character");
    auto& anim_comp = anim_world.Entities().Add<AnimatorComponent>(char_ent);
    anim_comp.skeleton = skel;
    anim_comp.current_clip = clip;
    anim_comp.is_playing = true;

    anim_world.RunPhase(UpdatePhase::Simulation, p_ft);
    check(!anim_comp.skinning_palette.empty(), "animation system computes skinning palette");
    check(anim_comp.skinning_palette.size() == 2, "skinning palette has 2 joint matrices");
    check(anim_comp.current_time > 0.0f, "animation playback advances time");
    anim_world.Shutdown();

    // --- Sub-Element Mesh Editing & UV Tests
    EditableMesh sub_mesh = MeshBuilder::CreateCube(Vec3(0.5f));
    const size_t orig_faces = sub_mesh.faces.size();
    sub_mesh.ExtrudeFace(0, 0.5f);
    check(sub_mesh.faces.size() == orig_faces + 6, "ExtrudeFace adds 6 skirt triangles");

    const size_t post_extrude_faces = sub_mesh.faces.size();
    sub_mesh.InsetFace(0, 0.2f);
    check(sub_mesh.faces.size() == post_extrude_faces + 6, "InsetFace adds 6 inner skirt triangles");

    const size_t post_inset_faces = sub_mesh.faces.size();
    sub_mesh.SubdivideFace(0);
    check(sub_mesh.faces.size() == post_inset_faces + 3, "SubdivideFace splits face into 4 triangles");

    auto edges = sub_mesh.GetEdges();
    check(!edges.empty(), "EditableMesh::GetEdges returns non-empty edge list");

    sub_mesh.FlipFaceNormal(0);
    check(sub_mesh.faces.size() > 0, "FlipFaceNormal preserves triangle count");

    // UV generation test
    sub_mesh.GenerateUVs(UVProjectionMode::Box, Vec2(2.0f), Vec2(0.5f));
    check(sub_mesh.vertices[0].uv.x != 0.0f || sub_mesh.vertices[0].uv.y != 0.0f, "GenerateUVs assigns Box projection UVs");

    sub_mesh.WeldVertices(0.001f);
    check(!sub_mesh.vertices.empty() && !sub_mesh.faces.empty(), "WeldVertices cleans up duplicate vertices");

    // --- TextureManager Test
    std::vector<u8> dummy_pixels(64 * 64 * 4, 255);
    TextureHandle dummy_tex = TextureManager::Instance().RegisterTexture("memory_test_tex", 64, 64, 4,
                                                                        TextureFormat::RGBA8_UNORM, dummy_pixels);
    check(dummy_tex.IsValid(), "TextureManager registers memory texture");
    const TextureInfo* tex_info = TextureManager::Instance().GetTexture(dummy_tex);
    check(tex_info && tex_info->width == 64 && tex_info->height == 64, "TextureManager returns valid TextureInfo");
    check(tex_info->memory_bytes == 64 * 64 * 4, "Texture memory size calculated accurately");

    // --- Engine Prefabs & Node Archetypes Test
    World prefab_world;
    prefab_world.Init();

    Entity terr_node = Prefab::CreateTerrainNode(prefab_world, TerrainComponent{}, 0, "TestTerrain");
    check(prefab_world.Entities().Valid(terr_node), "CreateTerrainNode creates valid entity");
    check(prefab_world.Entities().Get<TerrainComponent>(terr_node) != nullptr, "TerrainNode contains TerrainComponent");

    Entity veh_node = Prefab::CreateVehicleNode(prefab_world, Vec3(0, 1, 0), 0, "TestCar");
    check(prefab_world.Entities().Get<Vehicle>(veh_node) != nullptr, "VehicleNode contains Vehicle component");
    check(prefab_world.Entities().Get<RigidBody>(veh_node) != nullptr, "VehicleNode contains RigidBody component");

    Entity actor_node = Prefab::CreatePhysicsActorNode(prefab_world, PrimitiveType::Sphere, BodyType::Dynamic, Vec3(0, 5, 0), 0, "PhysSphere");
    check(prefab_world.Entities().Get<RigidBody>(actor_node)->shape == ShapeType::Sphere, "PhysicsActorNode creates sphere collider");

    Entity pawn_node = Prefab::CreatePawnNode(prefab_world, Vec3(0, 2, 5), "Player");
    check(prefab_world.Entities().Get<CameraComponent>(pawn_node) != nullptr, "PawnNode contains CameraComponent");
    check(prefab_world.Entities().Get<AudioListenerComponent>(pawn_node) != nullptr, "PawnNode contains AudioListenerComponent");

    // All Node Archetypes Testing
    Entity decal_node = Prefab::CreateDecalNode(prefab_world, "blood.png", Vec3(0));
    check(prefab_world.Entities().Get<DecalComponent>(decal_node) != nullptr, "DecalNode contains DecalComponent");

    Entity text_node = Prefab::CreateText3DNode(prefab_world, "Score: 100", Vec3(0, 2, 0));
    check(prefab_world.Entities().Get<Text3DComponent>(text_node) != nullptr, "Text3DNode contains Text3DComponent");

    Entity dir_light = Prefab::CreateDirectionalLightNode(prefab_world, Vec3(0, -1, 0));
    check(prefab_world.Entities().Get<LightSource>(dir_light)->type == LightType::Directional, "DirectionalLightNode created");

    Entity fog_node = Prefab::CreateFogVolumeNode(prefab_world, Vec3(0), Vec3(10));
    check(prefab_world.Entities().Get<FogVolumeComponent>(fog_node) != nullptr, "FogVolumeNode contains FogVolumeComponent");

    Entity post_node = Prefab::CreatePostProcessVolumeNode(prefab_world, Vec3(0), Vec3(10));
    check(prefab_world.Entities().Get<PostProcessVolumeComponent>(post_node) != nullptr, "PostProcessVolumeNode created");

    Entity cine_cam = Prefab::CreateCinematicCameraNode(prefab_world, Vec3(0, 1.8f, 5.0f), 85.0f, 1.4f);
    check(prefab_world.Entities().Get<CinematicCameraComponent>(cine_cam)->f_stop == 1.4f, "CinematicCameraNode has 85mm f/1.4 lens");

    Entity spring_node = Prefab::CreateSpringArmNode(prefab_world, Vec3(0), 4.5f);
    check(prefab_world.Entities().Get<SpringArmComponent>(spring_node) != nullptr, "SpringArmNode contains SpringArmComponent");

    Entity water_node = Prefab::CreateWaterBodyNode(prefab_world, Vec3(0), Vec2(50, 50));
    check(prefab_world.Entities().Get<WaterBodyComponent>(water_node) != nullptr, "WaterBodyNode contains WaterBodyComponent");

    Entity river_node = Prefab::CreateRiverNode(prefab_world, {Vec3(0), Vec3(10,0,10)});
    check(prefab_world.Entities().Get<RiverSplineComponent>(river_node) != nullptr, "RiverNode contains RiverSplineComponent");

    Entity fol_node = Prefab::CreateFoliageNode(prefab_world, "tree.obj", 100);
    check(prefab_world.Entities().Get<FoliageInstancerComponent>(fol_node)->instance_count == 100, "FoliageNode instantiates 100 instances");

    Entity trig_node = Prefab::CreateTriggerVolumeNode(prefab_world, Vec3(0), Vec3(2));
    check(prefab_world.Entities().Get<RigidBody>(trig_node)->is_trigger, "TriggerVolumeNode is non-physical sensor");

    Entity ray_sensor = Prefab::CreateRaycastSensorNode(prefab_world, Vec3(0, 1, 0), Vec3(0, -1, 0));
    check(prefab_world.Entities().Get<RaycastSensorComponent>(ray_sensor) != nullptr, "RaycastSensorNode created");

    Entity joint_node = Prefab::CreatePhysicsJointNode(prefab_world, JointType::Hinge, Vec3(0));
    check(prefab_world.Entities().Get<PhysicsJointComponent>(joint_node)->joint_type == JointType::Hinge, "PhysicsJointNode creates Hinge joint");

    Entity buoy_node = Prefab::CreateBuoyancyNode(prefab_world, Vec3(0), 0.0f);
    check(prefab_world.Entities().Get<BuoyancyComponent>(buoy_node) != nullptr, "BuoyancyNode contains BuoyancyComponent");

    Entity char_body = Prefab::CreateCharacterBodyNode(prefab_world, Vec3(0, 1, 0));
    check(prefab_world.Entities().Get<CharacterBodyComponent>(char_body) != nullptr, "CharacterBodyNode contains CharacterBodyComponent");

    Entity ai_enemy = Prefab::CreateAIControllerNode(prefab_world, Vec3(0, 1, 0));
    check(prefab_world.Entities().Get<AIControllerComponent>(ai_enemy) != nullptr, "AIControllerNode contains AIControllerComponent");
    check(prefab_world.Entities().Get<BehaviorTreeComponent>(ai_enemy) != nullptr, "AIControllerNode contains BehaviorTreeComponent");

    Entity nav_bounds = Prefab::CreateNavMeshBoundsNode(prefab_world, Vec3(0), Vec3(50));
    check(prefab_world.Entities().Get<NavMeshBoundsComponent>(nav_bounds) != nullptr, "NavMeshBoundsNode created");

    Entity trail_node = Prefab::CreateTrailNode(prefab_world, Vec3(0));
    check(prefab_world.Entities().Get<TrailComponent>(trail_node) != nullptr, "TrailNode contains TrailComponent");

    Entity beam_node = Prefab::CreateBeamEmitterNode(prefab_world, Vec3(0), Vec3(0,0,10));
    check(prefab_world.Entities().Get<BeamEmitterComponent>(beam_node) != nullptr, "BeamEmitterNode contains BeamEmitterComponent");

    Entity reverb_node = Prefab::CreateAudioReverbZoneNode(prefab_world, Vec3(0), 20.0f);
    check(prefab_world.Entities().Get<AudioReverbZoneComponent>(reverb_node)->radius == 20.0f, "AudioReverbZoneNode has 20m radius");

    Entity chest_node = Prefab::CreateInteractableNode(prefab_world, Vec3(0), "Open Chest");
    check(prefab_world.Entities().Get<InteractableComponent>(chest_node) != nullptr, "InteractableNode created");

    Entity spawner_node = Prefab::CreateItemSpawnerNode(prefab_world, Vec3(0), "ManaPotion");
    check(prefab_world.Entities().Get<SpawnerComponent>(spawner_node)->prefab_name == "ManaPotion", "ItemSpawnerNode spawns ManaPotion");

    Entity quest_node = Prefab::CreateQuestTriggerNode(prefab_world, Vec3(0), "Quest_Boss_01");
    check(prefab_world.Entities().Get<QuestTriggerComponent>(quest_node) != nullptr, "QuestTriggerNode created");

    Entity ui_3d_node = Prefab::CreateWorldSpaceUINode(prefab_world, Vec3(0, 2, 0), "Dragon HP");
    check(prefab_world.Entities().Get<WorldSpaceUIComponent>(ui_3d_node) != nullptr, "WorldSpaceUINode created");

    Entity lod_node = Prefab::CreateLODGroupNode(prefab_world, Vec3(0));
    check(prefab_world.Entities().Get<LODGroupComponent>(lod_node) != nullptr, "LODGroupNode contains LODGroupComponent");

    prefab_world.Shutdown();

    // --- Extended Node Categories (new 107-node taxonomy)
    // Vehicles
    Entity wheeled_node = Prefab::CreateWheeledVehicleNode(prefab_world, Vec3(0,1,0), "TestCar");
    check(prefab_world.Entities().Get<WheeledVehicleComponent>(wheeled_node) != nullptr, "WheeledVehicleNode created");
    Entity tracked_node = Prefab::CreateTrackedVehicleNode(prefab_world, Vec3(0,1,0), "TestTank");
    check(prefab_world.Entities().Get<TrackedVehicleComponent>(tracked_node) != nullptr, "TrackedVehicleNode created");
    Entity aircraft_node = Prefab::CreateAircraftNode(prefab_world, Vec3(0,10,0));
    check(prefab_world.Entities().Get<AircraftComponent>(aircraft_node) != nullptr, "AircraftNode created");
    Entity watercraft_node = Prefab::CreateWatercraftNode(prefab_world, Vec3(0,0,0));
    check(prefab_world.Entities().Get<WatercraftComponent>(watercraft_node) != nullptr, "WatercraftNode created");
    Entity wheel_node = Prefab::CreateVehicleWheelNode(prefab_world, Vec3(0.8f,0.35f,1.5f));
    check(prefab_world.Entities().Get<VehicleWheelComponent>(wheel_node) != nullptr, "VehicleWheelNode created");

    // Animation & Kinematics
    Entity bone_node = Prefab::CreateBoneNode(prefab_world, "Spine_01");
    check(prefab_world.Entities().Get<BoneNodeComponent>(bone_node)->bone_name == "Spine_01", "BoneNode stores bone name");
    Entity socket_node = Prefab::CreateSocketNode(prefab_world, "Hand_R");
    check(prefab_world.Entities().Get<BoneAttachmentComponent>(socket_node)->joint_name == "Hand_R", "SocketNode stores joint name");
    Entity anim_player = Prefab::CreateAnimationPlayerNode(prefab_world, "Run");
    check(prefab_world.Entities().Get<AnimationTreeComponent>(anim_player)->clip_a == "Run", "AnimationPlayerNode stores clip name");
    Entity morph_node = Prefab::CreateMorphTargetNode(prefab_world);
    check(prefab_world.Entities().Get<MorphTargetComponent>(morph_node) != nullptr, "MorphTargetNode created");

    // AI & Navigation extended
    Entity obstacle_node = Prefab::CreateNavMeshObstacleNode(prefab_world, Vec3(0,0,0));
    check(prefab_world.Entities().Get<NavMeshObstacleComponent>(obstacle_node) != nullptr, "NavMeshObstacleNode created");
    Entity nav_link = Prefab::CreateNavMeshLinkNode(prefab_world, Vec3(0,0,0), Vec3(0,2,3));
    check(prefab_world.Entities().Get<NavMeshLinkComponent>(nav_link)->is_bidirectional, "NavMeshLinkNode is bidirectional");
    Entity nav_agent = Prefab::CreateNavigationAgentNode(prefab_world, Vec3(0,0,0));
    check(prefab_world.Entities().Get<NavigationAgentComponent>(nav_agent) != nullptr, "NavigationAgentNode created");
    Entity fsm_node = Prefab::CreateFSMNode(prefab_world, "Idle");
    check(prefab_world.Entities().Get<FSMComponent>(fsm_node)->current_state == "Idle", "FSMNode initialized to Idle state");
    Entity blackboard = Prefab::CreateBlackboardNode(prefab_world);
    check(prefab_world.Entities().Get<BlackboardComponent>(blackboard) != nullptr, "BlackboardNode created");

    // Gameplay extended
    Entity equip_node = Prefab::CreateEquipmentNode(prefab_world);
    check(prefab_world.Entities().Get<EquipmentComponent>(equip_node) != nullptr, "EquipmentNode created");
    Entity ability_node = Prefab::CreateAbilityNode(prefab_world, "Fireball");
    check(prefab_world.Entities().Get<AbilityComponent>(ability_node)->ability_name == "Fireball", "AbilityNode stores ability name");
    Entity savepoint = Prefab::CreateSavePointNode(prefab_world, Vec3(0,0,0));
    check(prefab_world.Entities().Get<SavePointComponent>(savepoint) != nullptr, "SavePointNode created");

    // VFX extended
    Entity vfx_node = Prefab::CreateVFXGraphNode(prefab_world, "explosion.vfx");
    check(prefab_world.Entities().Get<VFXGraphComponent>(vfx_node)->graph_asset == "explosion.vfx", "VFXGraphNode stores graph path");

    // Audio extended
    Entity audio_src = Prefab::CreateAudioSourceNode(prefab_world, "thunder.wav", Vec3(0,0,0));
    check(prefab_world.Entities().Get<AudioSourceComponent>(audio_src)->sound_path == "thunder.wav", "AudioSourceNode stores sound path");
    Entity music_node = Prefab::CreateMusicTrackNode(prefab_world, "boss_theme.ogg");
    check(prefab_world.Entities().Get<MusicTrackComponent>(music_node)->track_path == "boss_theme.ogg", "MusicTrackNode stores track path");

    // Networking
    Entity net_id = Prefab::CreateNetworkIdentityNode(prefab_world, 42);
    check(prefab_world.Entities().Get<NetworkIdentityComponent>(net_id)->net_id == 42, "NetworkIdentityNode stores net_id 42");
    Entity net_xform = Prefab::CreateNetworkTransformNode(prefab_world);
    check(prefab_world.Entities().Get<NetworkTransformComponent>(net_xform) != nullptr, "NetworkTransformNode created");
    Entity net_anim = Prefab::CreateNetworkAnimatorNode(prefab_world);
    check(prefab_world.Entities().Get<NetworkAnimatorComponent>(net_anim) != nullptr, "NetworkAnimatorNode created");
    Entity repl_mgr = Prefab::CreateReplicationManagerNode(prefab_world, 60);
    check(prefab_world.Entities().Get<ReplicationManagerComponent>(repl_mgr)->tick_rate_hz == 60, "ReplicationManagerNode stores 60 Hz tick rate");
    Entity rpc_node = Prefab::CreateRPCNode(prefab_world);
    check(prefab_world.Entities().Get<RPCComponent>(rpc_node) != nullptr, "RPCNode created");

    // UI & HUD
    Entity canvas_node = Prefab::CreateCanvasLayerNode(prefab_world, 5);
    check(prefab_world.Entities().Get<CanvasComponent>(canvas_node)->sorting_order == 5, "CanvasLayerNode stores sort order 5");
    Entity ui_panel = Prefab::CreateUIPanelNode(prefab_world, Vec2(320, 200));
    check(prefab_world.Entities().Get<UIPanelComponent>(ui_panel)->size.x == 320.0f, "UIPanelNode stores panel dimensions");
    Entity ui_btn = Prefab::CreateUIButtonNode(prefab_world, "Start Game");
    check(prefab_world.Entities().Get<UIButtonComponent>(ui_btn)->label == "Start Game", "UIButtonNode stores label");
    Entity ui_label = Prefab::CreateUILabelNode(prefab_world, "Score: 9999");
    check(prefab_world.Entities().Get<UILabelComponent>(ui_label)->text == "Score: 9999", "UILabelNode stores text");
    Entity ui_img = Prefab::CreateUIImageNode(prefab_world, "crosshair.png");
    check(prefab_world.Entities().Get<UIImageComponent>(ui_img)->texture_path == "crosshair.png", "UIImageNode stores texture path");
    Entity minimap = Prefab::CreateMiniMapNode(prefab_world);
    check(prefab_world.Entities().Get<MiniMapComponent>(minimap) != nullptr, "MiniMapNode created");

    // Scene & Optimization
    Entity hlod_node = Prefab::CreateHLODNode(prefab_world, Vec3(0,0,0));
    check(prefab_world.Entities().Get<HLODComponent>(hlod_node) != nullptr, "HLODNode created");
    Entity portal_node = Prefab::CreateOcclusionPortalNode(prefab_world, Vec3(0,1.5f,0));
    check(prefab_world.Entities().Get<OcclusionPortalComponent>(portal_node)->is_open, "OcclusionPortalNode defaults open");
    Entity partition = Prefab::CreateWorldPartitionCellNode(prefab_world, Vec3(2,0,3));
    check(prefab_world.Entities().Get<WorldPartitionComponent>(partition)->cell_index == Vec3(2,0,3), "WorldPartitionCellNode stores cell index");
    Entity signal_bus = Prefab::CreateSignalBusNode(prefab_world);
    check(prefab_world.Entities().Get<SignalBusComponent>(signal_bus) != nullptr, "SignalBusNode created");

    prefab_world.Shutdown();

    // --- Gameplay Health & Stats Logic Test
    HealthComponent test_hp{};
    test_hp.max_health = 100.0f;
    test_hp.current_health = 100.0f;
    test_hp.shield = 20.0f;
    test_hp.TakeDamage(30.0f);
    check(test_hp.shield == 0.0f && test_hp.current_health == 90.0f, "HealthComponent absorbs damage with shield first");
    test_hp.Heal(5.0f);
    check(test_hp.current_health == 95.0f, "HealthComponent heals accurately");

    // --- DebugDraw Visualizer Queue Test
    DebugDraw::Clear();
    DebugDraw::DrawLine(Vec3(0), Vec3(1, 1, 1), Vec4(1, 0, 0, 1));
    DebugDraw::DrawBox(Vec3(0), Vec3(1), Vec4(0, 1, 0, 1));
    DebugDraw::DrawSphere(Vec3(0, 2, 0), 0.5f, Vec4(0, 0, 1, 1));
    DebugDraw::DrawCone(Vec3(0, 5, 0), Vec3(0, -1, 0), 0.5f, 10.0f, Vec4(1, 1, 0, 1));
    check(DebugDraw::GetLines().size() == 1, "DebugDraw queues 3D lines");
    check(DebugDraw::GetBoxes().size() == 1, "DebugDraw queues wireframe bounding boxes");
    check(DebugDraw::GetSpheres().size() == 1, "DebugDraw queues wireframe spheres");
    check(DebugDraw::GetCones().size() == 1, "DebugDraw queues perception cones");
    DebugDraw::Clear();
    check(DebugDraw::GetLines().empty() && DebugDraw::GetBoxes().empty(), "DebugDraw::Clear flushes visual queues");

    // --- RenderSettings AO and AA Pipeline Test
    RenderSettings r_settings{};
    r_settings.ao.mode = AOMode::SSAO;
    r_settings.ao.radius = 2.0f;
    r_settings.aa.mode = AAMode::TAA;
    check(r_settings.ao.mode == AOMode::SSAO && r_settings.ao.radius == 2.0f, "RenderSettings configures SSAO ambient occlusion");
    check(r_settings.aa.mode == AAMode::TAA, "RenderSettings configures TAA anti-aliasing");

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : "SOME CHECKS FAILED");
    return failures;
}
