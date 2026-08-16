// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
// Sandbox: the app that binds concrete backends together.
//
// This is the only file that names Metal, SDL2 and Jolt at the same time. The
// engine modules it drives know none of them.

#include "lucida/backend/JoltBackend.h"
#include "lucida/backend/PlatformSDL2.h"

#if defined(LUCIDA_HAS_METAL)
#include "lucida/backend/MetalBackend.h"
#endif
#if defined(LUCIDA_HAS_RC)
#include "lucida/backend/RadianceCascadesBackend.h"
#endif

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/CameraController.h"
#include "lucida/framework/DebugUI.h"
#include "lucida/framework/SceneAssets.h"
#include "lucida/framework/SceneLibrary.h"
#include "lucida/framework/Systems.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/resource/ModelLoader.h"
#include "lucida/resource/Project.h"
#include "lucida/resource/SceneSerializer.h"
#include "lucida/runtime/Engine.h"
#include "lucida/runtime/CharacterSystem.h"

#include <stb_image_write.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace lucida {
namespace {

struct Config {
    i32 width  = 1280;
    i32 height = 720;
    bool fullscreen = false;
    RenderSettings render;
    bool  walk_mode  = false;
    std::string backend = "metal"; // "metal" or "rc"
    std::string model_path;
    Vec3  model_pos{0.0f, -1.6f, -3.0f};
    f32   model_scale = 100.0f;
};

Config LoadConfig(const std::string& path) {
    Config cfg;
    cfg.render.render_scale = 0.55f;
    cfg.render.max_depth = 5;
    cfg.render.vsync = false;

    std::ifstream file(path);
    if (!file.is_open()) return cfg;

    std::string line;
    while (std::getline(file, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if      (key == "width")        cfg.width  = std::stoi(value);
        else if (key == "height")       cfg.height = std::stoi(value);
        else if (key == "fullscreen")   cfg.fullscreen = value != "0";
        else if (key == "samples")      cfg.render.samples = std::stoi(value);
        else if (key == "render_scale") cfg.render.render_scale = std::stof(value);
        else if (key == "max_depth")    cfg.render.max_depth = std::stoi(value);
        else if (key == "vsync")        cfg.render.vsync = value != "0";
        else if (key == "game_mode")    cfg.walk_mode = value != "0";
        else if (key == "model_path")   cfg.model_path = value;
        else if (key == "model_x")      cfg.model_pos.x = std::stof(value);
        else if (key == "model_y")      cfg.model_pos.y = std::stof(value);
        else if (key == "model_z")      cfg.model_pos.z = std::stof(value);
        else if (key == "model_scale")  cfg.model_scale = std::stof(value);
    }
    return cfg;
}

void SaveConfig(const std::string& path, const Config& cfg) {
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "width=" << cfg.width << "\nheight=" << cfg.height
         << "\nfullscreen=" << (cfg.fullscreen ? 1 : 0)
         << "\nsamples=" << cfg.render.samples
         << "\nrender_scale=" << cfg.render.render_scale
         << "\nmax_depth=" << cfg.render.max_depth
         << "\nvsync=" << (cfg.render.vsync ? 1 : 0)
         << "\ngame_mode=" << (cfg.walk_mode ? 1 : 0)
         << "\nmodel_path=" << cfg.model_path
         << "\nmodel_x=" << cfg.model_pos.x
         << "\nmodel_y=" << cfg.model_pos.y
         << "\nmodel_z=" << cfg.model_pos.z
         << "\nmodel_scale=" << cfg.model_scale << "\n";
}

// Headless verification: render N frames, optionally save the last one, quit.
// Without it the only way to know the renderer works is to look at it.
struct BenchOptions {
    i32 frames = 0;              // 0 disables
    std::string screenshot_path;
};

// Accepts an index or a name, so both `--scene 2` and `--scene lab` work.
scenes::BuiltIn ParseScene(const char* arg) {
    if (arg[0] >= '0' && arg[0] <= '9') {
        const int index = std::atoi(arg);
        if (index >= 0 && index < int(scenes::BuiltIn::Count)) return scenes::BuiltIn(index);
    }
    const std::string name = arg;
    if (name.find("empty") != std::string::npos) return scenes::BuiltIn::Empty;
    if (name.find("basic") != std::string::npos) return scenes::BuiltIn::BasicPrimitives;
    if (name.find("lab")   != std::string::npos) return scenes::BuiltIn::MaterialLab;
    if (name.find("water") != std::string::npos) return scenes::BuiltIn::WaterAndFog;
    LUCIDA_WARN(App, "unknown scene '%s', falling back to the default", arg);
    return scenes::BuiltIn::Empty;
}

class SandboxApp final : public IApplication {
public:
    SandboxApp(Config config, std::string config_path, BenchOptions bench)
        : m_config(std::move(config)), m_config_path(std::move(config_path)),
          m_bench(std::move(bench)) {}

    void SetStartScene(scenes::BuiltIn which) { m_ui_state.scene = which; }

    // A scene file wins over the built-in list: once a world lives on disk, the
    // built-ins are only a starting point.
    void SetSceneFile(std::string path) { m_scene_path = std::move(path); }

    // Opened before the engine starts, so window size and render defaults come
    // from the project rather than from a file next to the binary.
    void SetProject(Project project) { m_project = std::move(project); }

    bool Finished() const { return m_finished; }

    bool OnInit(World& world) override {
        m_platform = CreatePlatformSDL2();
        WindowDesc window;
        window.title      = "Lucida Engine";
        window.width      = m_config.width;
        window.height     = m_config.height;
        window.fullscreen = m_config.fullscreen;
        if (!m_platform->Init(window)) return false;

#if defined(LUCIDA_HAS_RC)
        if (m_config.backend == "rc" || m_config.backend == "radiance_cascades") {
            m_renderer = CreateRadianceCascadesBackend();
        }
#endif
        if (!m_renderer) {
#if defined(LUCIDA_HAS_METAL)
            m_renderer = CreateMetalBackend();
#elif defined(LUCIDA_HAS_RC)
            m_renderer = CreateRadianceCascadesBackend();
#endif
        }

        if (!m_renderer || !m_renderer->Init(m_platform->Surface())) return false;

        // ImGui context first, then both halves of its backend: each half
        // writes into the context and will fault if it does not exist yet.
        m_ui.Init();
        m_platform->OverlayInit();
        m_renderer->OverlayInit();

        if (m_config.backend == "rc" || m_config.backend == "radiance_cascades") {
            m_ui_state.current_backend   = UiState::RenderBackendType::RadianceCascades3D;
            m_ui_state.requested_backend = UiState::RenderBackendType::RadianceCascades3D;
        } else {
            m_ui_state.current_backend   = UiState::RenderBackendType::MetalRayTracing;
            m_ui_state.requested_backend = UiState::RenderBackendType::MetalRayTracing;
        }

        LoadScene(world, m_ui_state.scene);
        m_renderer->ApplySettings(m_config.render);
        m_camera.SetMode(m_config.walk_mode ? CameraMode::Walk : CameraMode::Fly);

        m_physics = CreateJoltBackend();
        if (m_physics->Init()) {
            m_physics_system = world.AddSystem<PhysicsSystem>(*m_physics);
            if (m_physics_system) {
                m_physics_system->SetPaused(m_ui_state.play_state != UiState::PlayState::Playing);
            }
        }
        m_script_system = world.AddSystem<ScriptSystem>();
        if (m_script_system) {
            m_script_system->SetPaused(m_ui_state.play_state != UiState::PlayState::Playing);
        }
        m_render_sync_system = world.AddSystem<RenderSyncSystem>(*m_renderer);

        if (!m_config.model_path.empty() && std::filesystem::exists(m_project.Resolve(m_config.model_path)))
            LoadModelFile(world, m_project.Resolve(m_config.model_path));

        // Always start in Editor mode: panels active, cursor free.
        m_ui_state.show_menu = true;
        m_platform->SetMouseCaptured(false);
        return true;
    }

    void SwitchBackend(World& world, UiState::RenderBackendType target_type) {
        LUCIDA_INFO(App, "Switching render backend to %s",
                    target_type == UiState::RenderBackendType::RadianceCascades3D ? "Radiance Cascades 3D" : "Metal Ray Tracer");

        RenderSettings saved_settings = m_renderer ? m_renderer->Settings() : m_config.render;

        if (m_renderer) {
            m_renderer->OverlayShutdown();
            m_renderer->Shutdown();
            m_renderer.reset();
        }

        if (target_type == UiState::RenderBackendType::RadianceCascades3D) {
#if defined(LUCIDA_HAS_RC)
            m_renderer = CreateRadianceCascadesBackend();
#endif
        }
        if (!m_renderer) {
#if defined(LUCIDA_HAS_METAL)
            m_renderer = CreateMetalBackend();
            target_type = UiState::RenderBackendType::MetalRayTracing;
#endif
        }

        if (!m_renderer || !m_renderer->Init(m_platform->Surface())) {
            LUCIDA_ERROR(App, "Failed to initialize new render backend!");
            return;
        }

        m_renderer->OverlayInit();
        const bool as_panel = m_ui_state.show_menu && m_ui_state.show_viewport;
        m_renderer->SetViewportAsPanel(as_panel);
        if (as_panel && m_ui_state.viewport_width > 0) {
            m_renderer->SetViewportSize(m_ui_state.viewport_width, m_ui_state.viewport_height);
        }
        m_renderer->ApplySettings(saved_settings);
        m_renderer->SetCamera(m_camera.Camera());

        if (m_render_sync_system) {
            m_render_sync_system->SetRenderer(*m_renderer);
        }

        m_fingerprint = 0; // Trigger full scene re-upload
        Republish(world);
        m_ui_state.current_backend   = target_type;
        m_ui_state.requested_backend = target_type;
    }

    void OnShutdown(World&) override {
        m_config.render = m_renderer->Settings();
        m_config.walk_mode = m_camera.Mode() == CameraMode::Walk;

        // With a project open, the project is the settings file. config.txt only
        // exists for the no-project case, and should not shadow it.
        if (m_project.IsOpen()) {
            ProjectSettings& settings = m_project.Settings();
            settings.window_width  = m_config.width;
            settings.window_height = m_config.height;
            settings.fullscreen    = m_config.fullscreen;
            settings.walk_mode     = m_config.walk_mode;
            settings.render        = m_config.render;
            settings.startup_model = m_project.MakeRelative(m_config.model_path);
            if (!m_scene_path.empty())
                settings.startup_scene = m_project.MakeRelative(m_scene_path);
            m_project.Save();
        } else {
            SaveConfig(m_config_path, m_config);
        }

        if (m_physics) m_physics->Shutdown();
        m_renderer->OverlayShutdown();
        m_platform->OverlayShutdown();
        m_renderer->Shutdown();
        m_ui.Shutdown();
        m_platform->Shutdown();
    }

    bool OnPollEvents(World&) override {
        LUCIDA_PROFILE("input");
        if (!m_platform->PumpEvents(m_input)) return false;

        if (m_input.Pressed(Action::ToggleMenu)) {
            m_ui_state.show_menu = !m_ui_state.show_menu;
            m_platform->SetMouseCaptured(!m_ui_state.show_menu);
        }
        if (m_input.Pressed(Action::ToggleFullscreen)) m_platform->ToggleFullscreen();
        if (m_input.Pressed(Action::ToggleGameMode))   m_camera.ToggleMode();
        if (m_input.Pressed(Action::ToggleFog)) {
            RenderSettings s = m_renderer->Settings();
            s.fog = !s.fog;
            m_renderer->ApplySettings(s);
        }
        return !m_ui_state.request_quit;
    }

    void OnFixedUpdate(World&, const FrameTime& time) override {
        LUCIDA_PROFILE("fixed");
        m_camera.FixedUpdate(m_input, time.delta);
        if (m_step_tick) {
            m_step_tick = false;
            if (m_physics_system) m_physics_system->SetPaused(true);
            if (m_script_system)  m_script_system->SetPaused(true);
        }
    }

    void OnRender(World& world, const FrameTime& time) override {
        LUCIDA_PROFILE("render");

        // Handle Play Mode state requests
        if (m_ui_state.request_play) {
            m_ui_state.request_play = false;
            if (m_ui_state.play_state == UiState::PlayState::Edit) {
                // Entering play mode: capture world snapshot
                m_play_snapshot = WorldSnapshot::Capture(world.Entities());
                LUCIDA_INFO(App, "Play mode started (captured snapshot with %zu entities)", m_play_snapshot.entities.size());
            }
            m_ui_state.play_state = UiState::PlayState::Playing;
            if (m_physics_system) m_physics_system->SetPaused(false);
            if (m_script_system)  m_script_system->SetPaused(false);
            // Character controller: create Jolt capsule for every CharacterBodyNode
            m_character_system.OnEnterPlay(world, m_physics.get());
            // Switch viewport to game camera
            m_ui_state.camera_source = UiState::CameraSource::GameCamera;
        }
        if (m_ui_state.request_pause) {
            m_ui_state.request_pause = false;
            m_ui_state.play_state = UiState::PlayState::Paused;
            if (m_physics_system) m_physics_system->SetPaused(true);
            if (m_script_system)  m_script_system->SetPaused(true);
            LUCIDA_INFO(App, "Play mode paused");
        }
        if (m_ui_state.request_step) {
            m_ui_state.request_step = false;
            m_ui_state.play_state = UiState::PlayState::Paused;
            m_step_tick = true;
            if (m_physics_system) m_physics_system->SetPaused(false);
            if (m_script_system)  m_script_system->SetPaused(false);
            LUCIDA_INFO(App, "Play mode step single tick");
        }
        if (m_ui_state.request_stop) {
            m_ui_state.request_stop = false;
            m_ui_state.play_state = UiState::PlayState::Edit;
            if (m_physics_system) m_physics_system->SetPaused(true);
            if (m_script_system)  m_script_system->SetPaused(true);

            // Leaving play mode: restore world snapshot
            m_play_snapshot.Restore(world.Entities());
            // Destroy character physics handles (entity IDs no longer valid)
            m_character_system.OnExitPlay(world, m_physics.get());
            // Switch back to editor camera
            m_ui_state.camera_source = UiState::CameraSource::Viewport;
            m_fingerprint = 0; // force scene republish
            Republish(world);
            LUCIDA_INFO(App, "Play mode stopped (restored world snapshot)");
        }

        if (m_ui_state.requested_backend != m_ui_state.current_backend) {
            SwitchBackend(world, m_ui_state.requested_backend);
        }

        // Camera control:
        //   Game mode (no menu): mouse is captured, free FPS camera.
        //   Editor mode: when RMB is held inside viewport, capture mouse and fly.
        const bool in_game_mode   = !m_ui_state.show_menu;
        const bool editor_looking = m_ui_state.show_menu && m_ui_state.viewport_rmb;

        if (in_game_mode || editor_looking) {
            m_platform->SetMouseCaptured(true);
            m_camera.Update(m_input, time.real_delta);
        } else {
            m_platform->SetMouseCaptured(false);
        }

        CameraState active_camera = m_camera.Camera();

        if (m_ui_state.camera_source == UiState::CameraSource::GameCamera) {
            // Find active in-scene camera entity
            for (auto [e, cam, lt] : world.Entities().View<CameraComponent, LocalTransform>().each()) {
                if (cam.is_primary) {
                    active_camera.position = lt.position;
                    const Vec3 fwd = lt.rotation * Vec3(0, 0, -1);
                    const f32 len = glm::length(fwd);
                    if (len > kEpsilon) {
                        const Vec3 ndir = fwd / len;
                        active_camera.yaw   = std::atan2(ndir.z, ndir.x);
                        active_camera.pitch = std::asin(Clamp(ndir.y, -0.999f, 0.999f));
                    }
                    active_camera.fov_y = glm::radians(cam.fov);
                    break;
                }
            }
        }

        m_renderer->SetCamera(active_camera);

        i32 w = 0, h = 0;
        m_platform->GetDrawableSize(w, h);
        if (w != m_config.width || h != m_config.height) {
            m_config.width = w;
            m_config.height = h;
            m_renderer->Resize(w, h);
        }

        RenderSettings settings = m_renderer->Settings();

        m_renderer->OverlayNewFrame();   // acquires the drawable, starts ImGui-Metal
        m_platform->OverlayNewFrame();
        // The image goes into a panel exactly when the editor is on screen, and
        // straight to the window when it is not. Deciding this every frame is
        // what makes toggling the menu mid-session behave: it used to be set once
        // at startup, so pressing the key gave you panels floating over a
        // presentation that knew nothing about them.
        const bool as_panel = m_ui_state.show_menu && m_ui_state.show_viewport;
        m_renderer->SetViewportAsPanel(as_panel);

        // The panel measured itself last frame; hand that to the renderer before
        // it traces this one. One frame of lag on a resize is invisible, and it
        // avoids measuring and reacting inside the same frame.
        if (as_panel && m_ui_state.viewport_width > 0) {
            m_renderer->SetViewportSize(m_ui_state.viewport_width, m_ui_state.viewport_height);
        } else if (!as_panel) {
            m_renderer->SetViewportSize(0, 0);   // back to filling the window
        }

        const f32 aspect =
            (as_panel && m_ui_state.viewport_height > 0)
                ? f32(m_ui_state.viewport_width) / f32(m_ui_state.viewport_height)
                : (h > 0 ? f32(w) / f32(h) : 16.0f / 9.0f);
        m_ui.Build(world, m_assets, m_ui_state, settings, m_renderer->Stats(), m_camera, time,
                   as_panel ? m_renderer->ViewportTexture() : nullptr, aspect, m_renderer.get());

        m_renderer->ApplySettings(settings);
        if (m_ui_state.request_fullscreen) {
            m_platform->ToggleFullscreen();
            m_ui_state.request_fullscreen = false;
        }
        if (m_ui_state.request_scene_reload) {
            LoadScene(world, m_ui_state.scene);
            m_ui_state.request_scene_reload = false;
        }
        Republish(world);
        if (!m_ui_state.pending_model_path.empty()) {
            LoadModelFile(world, m_ui_state.pending_model_path);
            m_ui_state.pending_model_path.clear();
        }

        m_renderer->Render(time);

        if (m_bench.frames > 0) RunBenchFrame(time);
    }

private:
    // Builds a scene and hands it over. Note the split: the backend takes the
    // geometry, the application takes the spawn point - placing a camera is not
    // the renderer's job.
    void LoadScene(World& world, scenes::BuiltIn which) {
        Registry& entities = world.Entities();

        // A scene load replaces the world. Entities that belong to the previous
        // scene go with it; the car and anything else the app owns is recreated
        // by whoever owns it.
        entities.Clear();
        m_car = kNullEntity;
        m_ui_state.selection = kNullEntity;
        m_config.model_path.clear();
        if (m_renderer) m_renderer->ClearMeshes();

        RenderScene loaded;
        if (!m_scene_path.empty() && LoadSceneFile(m_project.Resolve(m_scene_path), loaded)) {
            // A scene file still carries raw arrays; turn them back into entities
            // so the editor can touch them. This is the seam where the file
            // format will grow entities of its own.
            m_assets = SceneAssets{};
            m_assets.name           = loaded.name;
            m_assets.materials      = loaded.materials;
            m_assets.material_names = loaded.material_names;
            m_assets.environment    = loaded.environment;
            m_assets.model          = loaded.model;
            m_assets.spawn          = loaded.spawn;
            Adopt(entities, loaded);
        } else {
            if (!m_scene_path.empty()) {
                LUCIDA_WARN(App, "falling back to the built-in scene");
                m_scene_path.clear();
            }
            m_assets = scenes::Build(which, entities);
        }

        UpdateWorldTransforms(entities);
        m_camera.Camera() = m_assets.spawn;
        m_fingerprint = 0;   // force a publish on the next frame
        Republish(world);
    }

    // Rebuilds the backend's view of the world, but only when something the
    // backend can see has actually changed. Uploading buffers every frame would
    // make a still scene cost as much as a moving one.
    void Republish(World& world) {
        Registry& entities = world.Entities();
        const u64 fingerprint = SceneFingerprint(entities, m_assets);
        if (fingerprint == m_fingerprint) return;
        m_fingerprint = fingerprint;

        RenderScene scene;
        PublishScene(entities, m_assets, scene);
        m_renderer->SubmitScene(scene);
        m_scene_spheres = scene.spheres;
    }

    void Adopt(Registry& entities, const RenderScene& scene) {
        for (const GPUSphere& s : scene.spheres) {
            const Entity e = CreatePrimitive(entities, PrimitiveType::Sphere,
                                             Vec3(s.center[0], s.center[1], s.center[2]),
                                             s.mat_index);
            entities.Get<LocalTransform>(e)->scale = Vec3(s.radius);
        }
        for (const GPUCube& c : scene.cubes) {
            const Entity e = CreatePrimitive(entities, PrimitiveType::Box,
                                             Vec3(c.center[0], c.center[1], c.center[2]),
                                             c.mat_index);
            const Vec3 half(c.half_size[0], c.half_size[1], c.half_size[2]);
            entities.Get<PrimitiveShape>(e)->size = half;
            entities.Add<LocalBounds>(e, LocalBounds{-half, half});
        }
        for (const GPUPlane& p : scene.planes) {
            const Vec3 normal(p.normal[0], p.normal[1], p.normal[2]);
            const Entity e = CreatePrimitive(entities, PrimitiveType::Plane,
                                             normal * p.d_offset, p.mat_index);
            entities.Get<PrimitiveShape>(e)->normal = normal;
            entities.Get<PrimitiveShape>(e)->offset = p.d_offset;
        }
    }

    void RunBenchFrame(const FrameTime& time) {
        m_bench_times.push_back(time.real_delta * 1000.0f);
        if (i32(m_bench_times.size()) < m_bench.frames) return;

        // Drop the first frames: they carry shader compilation and the first
        // texture uploads, which say nothing about steady-state cost.
        const usize warmup = Min<usize>(10, m_bench_times.size() / 4);
        f32 total = 0.0f, worst = 0.0f;
        for (usize i = warmup; i < m_bench_times.size(); ++i) {
            total += m_bench_times[i];
            worst = Max(worst, m_bench_times[i]);
        }
        const f32 count = f32(m_bench_times.size() - warmup);
        LUCIDA_INFO(App, "bench %d frames: avg %.2f ms (%.1f fps), worst %.2f ms, gpu %.2f ms",
                    m_bench.frames, total / count, 1000.0f * count / total, worst,
                    m_renderer->Stats().gpu_frame_ms);

        if (!m_bench.screenshot_path.empty()) {
            std::vector<u8> rgba;
            i32 w = 0, h = 0;
            if (m_renderer->ReadbackFrame(rgba, w, h)) {
                stbi_write_png(m_bench.screenshot_path.c_str(), w, h, 4, rgba.data(), w * 4);
                LUCIDA_INFO(App, "wrote %s (%dx%d)", m_bench.screenshot_path.c_str(), w, h);
            } else {
                LUCIDA_ERROR(App, "readback failed");
            }
        }
        m_finished = true;
        m_ui_state.request_quit = true;
    }

    void LoadModelFile(World& world, const std::string& path) {
        LUCIDA_INFO(App, "loading %s", path.c_str());
        MeshData mesh = LoadModel(path, 2.0f);
        if (!mesh.valid) {
            LUCIDA_ERROR(App, "failed to load %s", path.c_str());
            return;
        }
        const MeshHandle handle = m_renderer->AddMesh(mesh);
        if (!handle.IsValid()) return;

        Registry& entities = world.Entities();
        std::string model_name = std::filesystem::path(path).stem().string();
        if (model_name.empty()) model_name = "Model";

        if (!entities.Valid(m_car)) m_car = entities.Create(model_name);
        else {
            if (Name* n = entities.Get<Name>(m_car)) n->value = model_name;
        }

        LocalTransform& local = *entities.Get<LocalTransform>(m_car);
        local.position = m_config.model_pos;
        f32 s = (m_config.model_scale > 10.0f) ? (m_config.model_scale * 0.01f) : (m_config.model_scale > 0.001f ? m_config.model_scale : 1.0f);
        local.scale    = Vec3(s);
        m_config.model_scale = s;

        MeshInstance instance;
        instance.mesh     = handle;
        instance.instance = m_renderer->AddInstance(handle, local.ToMatrix());
        entities.Add<MeshInstance>(m_car, instance);

        // The loader already computed these while normalising the mesh; carrying
        // them onto the entity is what makes the thing clickable.
        entities.Add<LocalBounds>(m_car, LocalBounds{mesh.aabb_min, mesh.aabb_max});

        // Instantiate child entities in the hierarchy for submeshes (e.g. Body, Wheels, Hood, Glass)
        for (const auto& sm : mesh.submeshes) {
            Entity child = entities.Create(sm.name);
            entities.Add<Parent>(child, Parent{m_car});
            entities.Add<LocalBounds>(child, LocalBounds{sm.aabb_min, sm.aabb_max});
        }

        m_renderer->SetMeshOrigin(m_config.model_pos);
        m_config.model_path = m_project.MakeRelative(path);
        LUCIDA_INFO(App, "world holds %zu entities (imported %zu submeshes)", entities.Count(), mesh.submeshes.size());
    }

    Config      m_config;
    std::string m_config_path;

    std::unique_ptr<IPlatform>       m_platform;
    std::unique_ptr<IRenderBackend>  m_renderer;
    std::unique_ptr<IPhysicsBackend> m_physics;
    PhysicsSystem*                   m_physics_system = nullptr;
    ScriptSystem*                    m_script_system  = nullptr;
    RenderSyncSystem*                m_render_sync_system = nullptr;

    WorldSnapshot    m_play_snapshot;
    bool             m_step_tick = false;
    CharacterSystem  m_character_system;

    CameraController m_camera;
    DebugUI          m_ui;
    UiState          m_ui_state;
    InputState       m_input;

    Entity m_car = kNullEntity;

    // Kept for gameplay collision against the analytic scene, which is the
    // application's business rather than the renderer's.
    std::vector<GPUSphere> m_scene_spheres;

    SceneAssets      m_assets;
    u64              m_fingerprint = 0;
    Project          m_project;
    std::string      m_scene_path;
    BenchOptions     m_bench;
    std::vector<f32> m_bench_times;
    bool             m_finished = false;
};

} // namespace
} // namespace lucida

int main(int argc, char** argv) {
    using namespace lucida;

    std::string config_path = "config.txt";
    std::string model_override;
    BenchOptions bench;
    std::string scene_arg;
    std::string export_path;
    std::string project_dir;
    std::string new_project_dir;
    std::string backend_arg;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)      config_path = argv[++i];
        else if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc) backend_arg = argv[++i];
        else if (std::strcmp(argv[i], "--mesh") == 0 && i + 1 < argc)   model_override = argv[++i];
        else if (std::strcmp(argv[i], "--bench") == 0 && i + 1 < argc)  bench.frames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc)   bench.screenshot_path = argv[++i];
        else if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc)  scene_arg = argv[++i];
        else if (std::strcmp(argv[i], "--export-scene") == 0 && i + 1 < argc) export_path = argv[++i];
        else if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc)     project_dir = argv[++i];
        else if (std::strcmp(argv[i], "--new-project") == 0 && i + 1 < argc) new_project_dir = argv[++i];
        else if (std::strcmp(argv[i], "--verbose") == 0)                LogSetLevel(LogLevel::Debug);
    }

    // Tool mode: scaffold a project folder and stop.
    if (!new_project_dir.empty()) {
        const std::string name = std::filesystem::path(new_project_dir).filename().string();
        return Project::Create(new_project_dir, name.empty() ? "Untitled" : name) ? 0 : 1;
    }

    // A project supplies what config.txt used to: window, render defaults, the
    // scene to open. Command-line arguments still win over both.
    Project project;
    Config config;
    if (!project_dir.empty()) {
        if (!project.Open(project_dir)) return 1;

        RecentProjects recents;
        recents.Load();
        recents.Add(project.Root());
        recents.Save();

        const ProjectSettings& settings = project.Settings();
        config.width      = settings.window_width;
        config.height     = settings.window_height;
        config.fullscreen = settings.fullscreen;
        config.walk_mode  = settings.walk_mode;
        config.render     = settings.render;
        config.model_path = settings.startup_model;
        if (scene_arg.empty()) scene_arg = settings.startup_scene;
    } else {
        config = LoadConfig(config_path);
    }
    if (!model_override.empty()) config.model_path = model_override;
    if (!backend_arg.empty())    config.backend = backend_arg;

    // A --scene argument is either a file or the name of a built-in.
    const bool scene_is_file = scene_arg.size() > 5 &&
                               scene_arg.compare(scene_arg.size() - 5, 5, ".json") == 0;
    const scenes::BuiltIn start_scene =
        scene_is_file ? scenes::BuiltIn::Empty : ParseScene(scene_arg.empty() ? "empty"
                                                                                    : scene_arg.c_str());

    // Tool mode: write a built-in out as an editable file and stop. This is how
    // you get a starting point to edit rather than authoring JSON from nothing.
    if (!export_path.empty()) {
        // Exporting needs a world to build the scene in, even without a window:
        // the scene lives as entities now, and the file is a view of them.
        Registry entities;
        const SceneAssets assets = scenes::Build(start_scene, entities);
        UpdateWorldTransforms(entities);

        RenderScene scene;
        PublishScene(entities, assets, scene);
        return SaveScene(scene, export_path) ? 0 : 1;
    }

    EngineConfig engine_config;
    engine_config.loop.fixed_step = 1.0f / 60.0f;
    engine_config.log_level = LogLevel::Info;

    Engine engine;
    if (!engine.Init(engine_config)) return 1;

    SandboxApp app(config, config_path, bench);
    app.SetProject(std::move(project));
    app.SetStartScene(start_scene);
    if (scene_is_file) app.SetSceneFile(scene_arg);
    const int result = engine.Run(app);
    engine.Shutdown();
    return result;
}
