#include "lucida/runtime/Engine.h"

#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Profiler.h"

namespace lucida {

bool Engine::Init(const EngineConfig& config) {
    m_config = config;
    LogSetLevel(config.log_level);

    LUCIDA_INFO(Runtime, "Lucida on %s, fixed step %.2f ms",
                LUCIDA_PLATFORM_NAME, config.loop.fixed_step * 1000.0f);

    m_world.Init(config.frame_arena_bytes);
    m_loop.Init(config.loop);
    m_ready = true;
    return true;
}

int Engine::Run(IApplication& app) {
    LUCIDA_ASSERT(m_ready, "Engine::Init was not called");

    if (!app.OnInit(m_world)) {
        LUCIDA_ERROR(Runtime, "application init failed");
        return 1;
    }

    m_running = true;
    while (m_running) {
        m_world.BeginFrame();
        const FrameTime& time = m_loop.BeginFrame();

        if (!app.OnPollEvents(m_world)) {
            m_running = false;
            break;
        }
        m_world.RunPhase(UpdatePhase::PreSimulation, time);

        while (m_loop.StepSimulation()) {
            app.OnFixedUpdate(m_world, m_loop.Time());
            m_world.RunPhase(UpdatePhase::Simulation, m_loop.Time());
            m_world.RunPhase(UpdatePhase::PostSimulation, m_loop.Time());
        }

        const FrameTime& frame = m_loop.EndSimulation();
        m_world.RunPhase(UpdatePhase::Presentation, frame);
        app.OnRender(m_world, frame);

        ProfileEndFrame();
    }

    app.OnShutdown(m_world);
    return 0;
}

void Engine::Shutdown() {
    if (!m_ready) return;
    m_world.Shutdown();
    m_ready = false;
    LUCIDA_INFO(Runtime, "engine down");
}

} // namespace lucida
