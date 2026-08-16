// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Human Interface Devices layer (GEA ch.8).
//
// Gameplay asks "is the player moving forward", never "is scancode W down".
// The platform backend fills this state; swapping SDL for anything else, or
// adding a gamepad, changes nothing above this header.

#include "lucida/core/math/Math.h"

namespace lucida {

enum class Action : u8 {
    MoveForward, MoveBack, MoveLeft, MoveRight,
    Sprint, Jump, Crouch,
    ToggleMenu, ToggleFog, ToggleGameMode, ToggleFullscreen, ToggleEditMode,
    Count
};

inline constexpr usize kActionCount = static_cast<usize>(Action::Count);

struct InputState {
    bool down[kActionCount]     = {};
    bool pressed[kActionCount]  = {};   // edge: went down this frame
    bool released[kActionCount] = {};

    Vec2 mouse_delta{0.0f};
    Vec2 mouse_position{0.0f};
    bool mouse_captured = false;
    bool quit_requested = false;

    bool Down(Action a)     const { return down[static_cast<usize>(a)]; }
    bool Pressed(Action a)  const { return pressed[static_cast<usize>(a)]; }
    bool Released(Action a) const { return released[static_cast<usize>(a)]; }

    // x: right positive, y: forward positive. Not normalised - the caller
    // decides whether diagonal movement should be clamped.
    Vec2 MoveAxis() const {
        return Vec2(f32(Down(Action::MoveRight))   - f32(Down(Action::MoveLeft)),
                    f32(Down(Action::MoveForward)) - f32(Down(Action::MoveBack)));
    }

    // Call before the platform writes the new frame's state.
    void ClearEdges() {
        for (usize i = 0; i < kActionCount; ++i) { pressed[i] = false; released[i] = false; }
        mouse_delta = Vec2(0.0f);
    }
};

} // namespace lucida
