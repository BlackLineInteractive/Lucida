#include "lucida/backend/PlatformSDL2.h"

#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Log.h"

#include <SDL2/SDL.h>
#if LUCIDA_PLATFORM_MACOS
#include <SDL2/SDL_metal.h>
#endif

#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"

#include <array>

namespace lucida {
namespace {

// Action map. One table instead of a chain of ifs: rebinding later means
// editing data, not code.
struct Binding { SDL_Scancode key; Action action; };

constexpr std::array<Binding, 14> kBindings{{
    {SDL_SCANCODE_W,      Action::MoveForward},
    {SDL_SCANCODE_S,      Action::MoveBack},
    {SDL_SCANCODE_A,      Action::MoveLeft},
    {SDL_SCANCODE_D,      Action::MoveRight},
    {SDL_SCANCODE_LSHIFT, Action::Sprint},
    {SDL_SCANCODE_RSHIFT, Action::Sprint},
    {SDL_SCANCODE_SPACE,  Action::Jump},
    {SDL_SCANCODE_LCTRL,  Action::Crouch},
    {SDL_SCANCODE_C,      Action::Crouch},
    {SDL_SCANCODE_TAB,    Action::ToggleMenu},
    {SDL_SCANCODE_ESCAPE, Action::ToggleMenu},
    {SDL_SCANCODE_V,      Action::ToggleFog},
    {SDL_SCANCODE_F,      Action::ToggleGameMode},
    {SDL_SCANCODE_F11,    Action::ToggleFullscreen},
}};

class PlatformSDL2 final : public IPlatform {
public:
    bool Init(const WindowDesc& desc) override {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
            LUCIDA_ERROR(Core, "SDL_Init: %s", SDL_GetError());
            return false;
        }

        Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;
        if (desc.resizable)  flags |= SDL_WINDOW_RESIZABLE;
        if (desc.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#if LUCIDA_PLATFORM_MACOS
        if (desc.metal_surface) flags |= SDL_WINDOW_METAL;
#endif

        m_window = SDL_CreateWindow(desc.title.c_str(), SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED, desc.width, desc.height, flags);
        if (!m_window) {
            LUCIDA_ERROR(Core, "SDL_CreateWindow: %s", SDL_GetError());
            return false;
        }
        m_fullscreen = desc.fullscreen;

#if LUCIDA_PLATFORM_MACOS
        if (desc.metal_surface) {
            m_metal_view = SDL_Metal_CreateView(m_window);
            if (!m_metal_view) {
                LUCIDA_ERROR(Core, "SDL_Metal_CreateView: %s", SDL_GetError());
                return false;
            }
            m_native_layer = SDL_Metal_GetLayer(m_metal_view);
        }
#endif
        LUCIDA_INFO(Core, "window %dx%d on %s", desc.width, desc.height, LUCIDA_PLATFORM_NAME);
        return true;
    }

    void Shutdown() override {
#if LUCIDA_PLATFORM_MACOS
        if (m_metal_view) SDL_Metal_DestroyView(m_metal_view);
        m_metal_view = nullptr;
#endif
        if (m_window) SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
    }

    SurfaceDesc Surface() const override {
        SurfaceDesc s;
        s.native_layer  = m_native_layer;
        s.native_window = m_window;
        GetDrawableSize(s.width, s.height);
        return s;
    }

    bool PumpEvents(InputState& input) override {
        input.ClearEdges();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            switch (event.type) {
            case SDL_QUIT:
                input.quit_requested = true;
                return false;

            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                if (event.key.repeat) break;
                const bool down = (event.type == SDL_KEYDOWN);
                for (const Binding& b : kBindings) {
                    if (b.key != event.key.keysym.scancode) continue;
                    const usize idx = static_cast<usize>(b.action);
                    input.down[idx] = down;
                    (down ? input.pressed : input.released)[idx] = true;
                }
                break;
            }

            case SDL_MOUSEMOTION:
                // Relative mode only while the view has the mouse; otherwise the
                // camera would spin while the user is clicking on the UI.
                if (m_captured) {
                    input.mouse_delta.x += f32(event.motion.xrel);
                    input.mouse_delta.y += f32(event.motion.yrel);
                }
                input.mouse_position = Vec2(f32(event.motion.x), f32(event.motion.y));
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) return false;
                break;

            default:
                break;
            }
        }

        input.mouse_captured = m_captured;
        return true;
    }

    void SetMouseCaptured(bool captured) override {
        if (m_captured == captured) return;
        m_captured = captured;
        SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
    }

    void ToggleFullscreen() override {
        m_fullscreen = !m_fullscreen;
        SDL_SetWindowFullscreen(m_window, m_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    void GetDrawableSize(i32& width, i32& height) const override {
        int w = 0, h = 0;
#if LUCIDA_PLATFORM_MACOS
        if (m_metal_view) {
            SDL_Metal_GetDrawableSize(m_window, &w, &h);
        } else
#endif
        {
            SDL_GetWindowSizeInPixels(m_window, &w, &h);
        }
        width  = w;
        height = h;
    }

    void OverlayInit() override {
#if LUCIDA_PLATFORM_MACOS
        ImGui_ImplSDL2_InitForMetal(m_window);
#else
        ImGui_ImplSDL2_InitForOpenGL(m_window, nullptr);
#endif
    }

    void OverlayNewFrame() override { ImGui_ImplSDL2_NewFrame(); }
    void OverlayShutdown() override { ImGui_ImplSDL2_Shutdown(); }

private:
    SDL_Window* m_window = nullptr;
    void*       m_native_layer = nullptr;
#if LUCIDA_PLATFORM_MACOS
    SDL_MetalView m_metal_view = nullptr;
#endif
    bool m_captured   = false;
    bool m_fullscreen = false;
};

} // namespace

std::unique_ptr<IPlatform> CreatePlatformSDL2() {
    return std::make_unique<PlatformSDL2>();
}

} // namespace lucida
