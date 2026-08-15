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
#include "lucida/resource/ModelLoader.h"
#include "lucida/runtime/Engine.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

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

class SandboxApp final : public IApplication {
public:
    SandboxApp(Config config, std::string config_path)
        : m_config(std::move(config)), m_config_path(std::move(config_path)) {}

    bool OnInit(World&) override {
        m_platform = CreatePlatformSDL2();
        WindowDesc window;
        window.title      = "Lucida Sandbox";
        window.width      = m_config.width;
        window.height     = m_config.height;
        window.fullscreen = m_config.fullscreen;
        if (!m_platform->Init(window)) return false;

        m_renderer = CreateMetalBackend();
        if (!m_renderer->Init(m_platform->Surface())) return false;

        // ImGui context first, then both halves of its backend.
        m_ui.Init();
        m_platform->OverlayInit();

        m_renderer->SetDemoScene(m_ui_state.demo_scene);
        m_renderer->ApplySettings(m_config.render);
        m_camera.SetMode(m_config.walk_mode ? CameraMode::Walk : CameraMode::Fly);

        m_physics = CreateJoltBackend();
        if (m_physics->Init()) {
            VehicleDesc car;
            m_vehicle = m_physics->CreateVehicle(car);
        }

        if (!m_config.model_path.empty()) LoadModelFile(m_config.model_path);

        m_platform->SetMouseCaptured(true);
        m_ui_state.show_menu = false;
        return true;
    }

    void OnShutdown(World&) override {
        m_config.render = m_renderer->Settings();
        m_config.walk_mode = m_camera.Mode() == CameraMode::Walk;
        SaveConfig(m_config_path, m_config);

        if (m_physics) m_physics->Shutdown();
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

        if (m_physics && m_vehicle.IsValid()) {
            m_physics->Step(time.delta);
            const VehicleState state = m_physics->GetVehicleState(m_vehicle);
            if (m_car_instance.IsValid()) {
                Transform t;
                t.position = state.position;
                t.rotation = state.rotation;
                t.scale    = m_car_scale;
                m_renderer->SetInstanceTransform(m_car_instance, t.ToMatrix());
            }
        }
    }

    void OnRender(World&, const FrameTime& time) override {
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
        if (!m_ui_state.pending_model_path.empty()) {
            LoadModelFile(m_ui_state.pending_model_path);
            m_ui_state.pending_model_path.clear();
        }

        m_renderer->Render(time);
    }

private:
    void LoadModelFile(const std::string& path) {
        LUCIDA_INFO(App, "loading %s", path.c_str());
        MeshData mesh = LoadModel(path, 2.0f);
        if (!mesh.valid) {
            LUCIDA_ERROR(App, "failed to load %s", path.c_str());
            return;
        }
        const MeshHandle handle = m_renderer->AddMesh(mesh);
        if (!handle.IsValid()) return;

        m_car_scale = m_config.model_scale * 0.01f;
        Transform t;
        t.position = m_config.model_pos;
        t.scale    = m_car_scale;
        m_car_instance = m_renderer->AddInstance(handle, t.ToMatrix());
        m_renderer->SetMeshOrigin(m_config.model_pos);
        m_config.model_path = path;
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

    VehicleHandle  m_vehicle;
    InstanceHandle m_car_instance;
    f32            m_car_scale = 1.0f;
};

} // namespace
} // namespace lucida

int main(int argc, char** argv) {
    using namespace lucida;

    std::string config_path = "config.txt";
    std::string model_override;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)      config_path = argv[++i];
        else if (std::strcmp(argv[i], "--mesh") == 0 && i + 1 < argc)   model_override = argv[++i];
        else if (std::strcmp(argv[i], "--verbose") == 0)                LogSetLevel(LogLevel::Debug);
    }

    Config config = LoadConfig(config_path);
    if (!model_override.empty()) config.model_path = model_override;

    EngineConfig engine_config;
    engine_config.loop.fixed_step = 1.0f / 60.0f;
    engine_config.log_level = LogLevel::Info;

    Engine engine;
    if (!engine.Init(engine_config)) return 1;

    SandboxApp app(config, config_path);
    const int result = engine.Run(app);
    engine.Shutdown();
    return result;
}
