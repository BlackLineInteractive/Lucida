#pragma once
// Engine startup, main loop, shutdown (GEA 5.5).
//
// Explicit init/shutdown order instead of static constructors: the order
// static initialisers run across translation units is not defined, and
// subsystems have real dependencies.

#include "lucida/runtime/Application.h"
#include "lucida/runtime/GameLoop.h"
#include "lucida/runtime/World.h"

namespace lucida {

struct EngineConfig {
    LoopConfig loop;
    usize frame_arena_bytes = 8u << 20;
    LogLevel log_level = LogLevel::Info;
};

class Engine {
public:
    bool Init(const EngineConfig& config);
    int  Run(IApplication& app);
    void Shutdown();

    World&    GetWorld() { return m_world; }
    GameLoop& Loop()     { return m_loop; }
    void      RequestQuit() { m_running = false; }

private:
    World        m_world;
    GameLoop     m_loop;
    EngineConfig m_config;
    bool m_running = false;
    bool m_ready   = false;
};

} // namespace lucida
