// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
// Sandbox: the app that binds concrete backends together.
//
// This is the only file that names Metal, SDL2 and Jolt at the same time. The
// engine modules it drives know none of them.

#include "lucida/backend/JoltBackend.h"
#include "lucida/backend/MetalBackend.h"
#include "lucida/backend/PlatformSDL2.h"

#include "lucida/core/diag/Profiler.h"
#include "lucida/framework/CameraController.h"
#include "lucida/framework/DebugUI.h"
#include "lucida/framework/SceneLibrary.h"
#include "lucida/framework/Systems.h"
#include "lucida/physics/Components.h"
#include "lucida/render/Components.h"
#include "lucida/resource/ModelLoader.h"
#include "lucida/resource/Project.h"
#include "lucida/resource/SceneSerializer.h"
#include "lucida/runtime/Engine.h"

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
    bool  walk_mode  = true;
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
    if (name.find("basic") != std::string::npos) return scenes::BuiltIn::BasicPrimitives;
    if (name.find("lab")   != std::string::npos) return scenes::BuiltIn::MaterialLab;
    if (name.find("water") != std::string::npos) return scenes::BuiltIn::WaterAndFog;
    LUCIDA_WARN(App, "unknown scene '%s', falling back to the default", arg);
    return scenes::BuiltIn::WaterAndFog;
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
        window.title      = "Lucida Sandbox";
        window.width      = m_config.width;
        window.height     = m_config.height;
        window.fullscreen = m_config.fullscreen;
        if (!m_platform->Init(window)) return false;

        m_renderer = CreateMetalBackend();
        if (!m_renderer->Init(m_platform->Surface())) return false;

        // ImGui context first, then both halves of its backend: each half
        // writes into the context and will fault if it does not exist yet.
        m_ui.Init();
        m_platform->OverlayInit();
        m_renderer->OverlayInit();

        LoadScene(m_ui_state.scene);
        m_renderer->ApplySettings(m_config.render);
        m_camera.SetMode(m_config.walk_mode ? CameraMode::Walk : CameraMode::Fly);

        m_physics = CreateJoltBackend();
        if (m_physics->Init()) {
            // The car is an entity now, not three members of this class. Its
            // pose is written by the physics system and read by the render
            // system; nothing here has to keep them in step.
            m_car = world.Entities().Create("Mustang Boss 302");
            Vehicle vehicle;
            vehicle.handle = m_physics->CreateVehicle(VehicleDesc{});
            world.Entities().Add<Vehicle>(m_car, vehicle);

            world.AddSystem<PhysicsSystem>(*m_physics);
        }
        world.AddSystem<RenderSyncSystem>(*m_renderer);

        if (!m_config.model_path.empty())
            LoadModelFile(world, m_project.Resolve(m_config.model_path));

        m_platform->SetMouseCaptured(true);
        m_ui_state.show_menu = false;
        return true;
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
        // Everything else this frame is a system: stepping physics and pushing
        // transforms belongs to them, not to the application.
        m_camera.FixedUpdate(m_input, time.delta);
    }

    void OnRender(World& world, const FrameTime& time) override {
        LUCIDA_PROFILE("render");

        // Mouse look belongs to the frame, not the tick: the delta already
        // covers exactly the time since the previous frame.
        if (!m_ui_state.show_menu) m_camera.Update(m_input, time.real_delta);
        m_renderer->SetCamera(m_camera.Camera());

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
        m_ui.Build(m_ui_state, settings, m_renderer->Stats(), m_camera, time);

        m_renderer->ApplySettings(settings);
        if (m_ui_state.request_fullscreen) {
            m_platform->ToggleFullscreen();
            m_ui_state.request_fullscreen = false;
        }
        if (m_ui_state.request_scene_reload) {
            LoadScene(m_ui_state.scene);
            m_ui_state.request_scene_reload = false;
        }
        if (!m_ui_state.pending_model_path.empty()) {
            LoadModelFile(world, m_ui_state.pending_model_path);
            m_ui_state.pending_model_path.clear();
        }

        m_renderer->Render(time);

        if (m_bench.frames > 0) RunBenchFrame(time);
    }

private:
    // Builds a scene and hands it over. Note the split: the backend takes the
    // geometry, the application takes the spawn point — placing a camera is not
    // the renderer's job.
    void LoadScene(scenes::BuiltIn which) {
        RenderScene scene;
        if (!m_scene_path.empty() && LoadSceneFile(m_project.Resolve(m_scene_path), scene)) {
            Submit(scene);
            return;
        }
        if (!m_scene_path.empty()) {
            LUCIDA_WARN(App, "falling back to the built-in scene");
            m_scene_path.clear();
        }
        Submit(scenes::Build(which));
    }

    void Submit(const RenderScene& scene) {
        m_renderer->SubmitScene(scene);
        m_camera.Camera() = scene.spawn;
        m_scene_spheres = scene.spheres;
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
        if (!entities.Valid(m_car)) m_car = entities.Create("model");

        LocalTransform& local = *entities.Get<LocalTransform>(m_car);
        local.position = m_config.model_pos;
        local.scale    = m_config.model_scale * 0.01f;

        MeshInstance instance;
        instance.mesh     = handle;
        instance.instance = m_renderer->AddInstance(handle, local.ToMatrix());
        entities.Add<MeshInstance>(m_car, instance);

        m_renderer->SetMeshOrigin(m_config.model_pos);
        m_config.model_path = m_project.MakeRelative(path);
        LUCIDA_INFO(App, "world holds %zu entities", entities.Count());
    }

    Config      m_config;
    std::string m_config_path;

    std::unique_ptr<IPlatform>       m_platform;
    std::unique_ptr<IRenderBackend>  m_renderer;
    std::unique_ptr<IPhysicsBackend> m_physics;

    CameraController m_camera;
    DebugUI          m_ui;
    UiState          m_ui_state;
    InputState       m_input;

    Entity m_car = kNullEntity;

    // Kept for gameplay collision against the analytic scene, which is the
    // application's business rather than the renderer's.
    std::vector<GPUSphere> m_scene_spheres;

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
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)      config_path = argv[++i];
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

    // A --scene argument is either a file or the name of a built-in.
    const bool scene_is_file = scene_arg.size() > 5 &&
                               scene_arg.compare(scene_arg.size() - 5, 5, ".json") == 0;
    const scenes::BuiltIn start_scene =
        scene_is_file ? scenes::BuiltIn::WaterAndFog : ParseScene(scene_arg.empty() ? "water"
                                                                                    : scene_arg.c_str());

    // Tool mode: write a built-in out as an editable file and stop. This is how
    // you get a starting point to edit rather than authoring JSON from nothing.
    if (!export_path.empty()) {
        const RenderScene scene = scenes::Build(start_scene);
        return SaveScene(scene, export_path) ? 0 : 1;
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
