// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Editor theme and animated widgets.
//
// The palette is carried over from Climax Game Engine Toolkit: a cool neutral
// chrome with a single warm accent that marks state - checkmarks, slider grabs,
// the active tab, selection. One accent is enough hierarchy; a paintbox is not.
//
// Motion goes through ImAnim rather than hand-rolled lerps: hover and press are
// tweened so a control answers the pointer instead of snapping.

#include "lucida/core/platform/Platform.h"

namespace lucida {

struct ThemeColors {
    // C++ blue.
    f32 accent[4]     = {0.00f, 0.41f, 0.71f, 1.00f};
    f32 accent_hi[4]  = {0.16f, 0.58f, 0.89f, 1.00f};
    f32 accent_dim[4] = {0.04f, 0.22f, 0.39f, 1.00f};
};

void ApplyTheme(const ThemeColors& colors = ThemeColors{});

// Button that lifts towards the accent on hover and press. Same shape as an
// ImGui::Button, so it drops into existing panels.
bool AnimatedButton(const char* label, f32 width = 0.0f);

// Section header whose contents ease in from the left. Symmetric on purpose:
// the indent it pushes has to come back off, so EndSection pairs with a true
// return, the same way ImGui's own Begin/End pairs do.
//
//   if (BeginSection("Frame", true)) { ...; EndSection(); }
bool BeginSection(const char* label, bool default_open = false);
void EndSection();

} // namespace lucida
