// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/backend/PlatformSDL2.h"

#include "lucida/core/diag/Assert.h"
#include "lucida/core/diag/Log.h"

#include <SDL2/SDL.h>
#if LUCIDA_PLATFORM_MACOS
#include <SDL2/SDL_metal.h>
#import <Cocoa/Cocoa.h>
#endif

#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include <stb_image.h>

#include <array>

namespace lucida {
namespace {

// Action map. One table instead of a chain of ifs: rebinding later means
// editing data, not code.
struct Binding { SDL_Scancode key; Action action; };

constexpr std::array<Binding, 16> kBindings{{
    {SDL_SCANCODE_W,      Action::MoveForward},
    {SDL_SCANCODE_S,      Action::MoveBack},
    {SDL_SCANCODE_A,      Action::MoveLeft},
    {SDL_SCANCODE_D,      Action::MoveRight},
    {SDL_SCANCODE_E,      Action::Jump},        // Fly Up
    {SDL_SCANCODE_Q,      Action::Crouch},      // Fly Down
    {SDL_SCANCODE_LSHIFT, Action::Sprint},
    {SDL_SCANCODE_RSHIFT, Action::Sprint},
    {SDL_SCANCODE_SPACE,  Action::Jump},
    {SDL_SCANCODE_LCTRL,  Action::Crouch},
    {SDL_SCANCODE_C,      Action::Crouch},
    {SDL_SCANCODE_TAB,    Action::ToggleEditMode},
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

        SetWindowIcon(desc.icon_path);

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

    void SetWindowIcon(const std::string& icon_path) override {
        if (!m_window) return;

        const char* candidates[] = {
            icon_path.c_str(),
            "media/ico.jpg",
            "../media/ico.jpg",
            "../../media/ico.jpg",
            "../../../media/ico.jpg"
        };

        int w = 0, h = 0, channels = 0;
        stbi_uc* pixels = nullptr;
        std::string found_path;

        for (const char* path : candidates) {
            if (!path || !path[0]) continue;
            pixels = stbi_load(path, &w, &h, &channels, 4);
            if (pixels) {
                found_path = path;
                break;
            }
        }

        if (!pixels) {
            LUCIDA_WARN(Core, "Application icon not found: %s", icon_path.c_str());
            return;
        }

        // 1. Set SDL2 Window Icon
        Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000;
        gmask = 0x00ff0000;
        bmask = 0x0000ff00;
        amask = 0x000000ff;
#else
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
#endif

        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            pixels, w, h, 32, w * 4, rmask, gmask, bmask, amask);
        if (surface) {
            SDL_SetWindowIcon(m_window, surface);
            SDL_FreeSurface(surface);
        }

        // 2. Set macOS Dock / Application Icon
#if LUCIDA_PLATFORM_MACOS
        @autoreleasepool {
            NSString* nsPath = [NSString stringWithUTF8String:found_path.c_str()];
            NSImage* app_image = [[NSImage alloc] initWithContentsOfFile:nsPath];
            if (app_image) {
                [NSApp setApplicationIconImage:app_image];
            }
        }
#endif

        stbi_image_free(pixels);
        LUCIDA_INFO(Core, "Loaded engine icon: %s (%dx%d)", found_path.c_str(), w, h);
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
