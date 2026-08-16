// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>

namespace lucida::manual {

struct Entry {
    const char* label;
    const char* description;
};

struct HotkeyEntry {
    const char* category;
    const char* shortcut;
    const char* description;
};

// ---------------------------------------------------------------- Navigation -
inline constexpr Entry kNavigationEntries[] = {
    { "RMB (Hold) + Mouse",     "Look around / Rotate camera (First-Person Mouselook)." },
    { "RMB + W / A / S / D",     "Fly forward, left, backward, right in 3D world space." },
    { "RMB + Q / E",             "Fly straight Down (Q) or Up (E)." },
    { "RMB + Shift",             "Sprint mode: multiplies fly speed by sprint multiplier." },
    { "RMB + Mouse Wheel",       "Instantly accelerate / decelerate camera fly speed." },
    { "LMB Click",               "Select entity under cursor (raycast picking). Click empty space to deselect." },
    { "F (Hotkey)",              "Focus / Frame camera on the currently selected entity." },
    { "View Preset Dropdown",    "Snap camera to Top, Bottom, Front, Back, Left, Right, or Isometric presets." },
    { "F11",                     "Toggle fullscreen window mode." },
};

// --------------------------------------------------------------------- Gizmo -
inline constexpr Entry kGizmoEntries[] = {
    { "T or 1",                  "Translate Gizmo (Move object along X, Y, Z axes)." },
    { "R or 2",                  "Rotate Gizmo (Rotate object around Euler axes)." },
    { "S or 3",                  "Scale Gizmo (Scale object uniformly or per-axis)." },
    { "Local / World",           "Switch Gizmo coordinate space between entity local orientation and world axes." },
    { "Snap Toggle",             "Enable / disable grid snapping for translation, rotation, and scaling." },
    { "Cmd+Z / Ctrl+Z",          "Undo last modification (transform, material, create, delete)." },
    { "Cmd+Shift+Z / Cmd+Y",     "Redo last undone change." },
    { "Cmd+D / Ctrl+D",          "Duplicate selected entity." },
    { "Del / Backspace",         "Delete selected entity (fully undoable)." },
    { "Hierarchy Drag & Drop",   "Drag an entity onto another in the Hierarchy panel to set parent-child relationships." },
};

// ----------------------------------------------------------------- Play Mode -
inline constexpr Entry kPlayModeEntries[] = {
    { "Play / Stop (Cmd+P)",     "Toggle Play Mode. Snapshots world state and activates real-time physics." },
    { "Pause (Cmd+Shift+P)",     "Freeze physics simulation and gameplay ticks." },
    { "Step Frame (Cmd+.)",      "Advance physics simulation by exactly 1 tick (1/60s)." },
    { "RigidBody: Dynamic",      "Object falls under gravity (-9.81 m/s²), collides with other bodies and ground." },
    { "RigidBody: Static",       "Immovable collider (ground, walls, obstacles) that blocks dynamic bodies." },
    { "RigidBody: Kinematic",    "Moved purely by scripts / animations without responding to external physical forces." },
    { "Mass, Friction, Bounce",  "Adjust mass (kg), surface friction, and bounciness (restitution) in the Inspector." },
};

// --------------------------------------------------- Materials & Rendering ---
inline constexpr const char* kRenderingBullets[] = {
    "Whitted Ray Tracer: Hardware-accelerated Metal RT with deterministic reflections and refractions.",
    "Radiance Cascades: 3D Global Illumination with directional radiance intervals.",
    "PBR Parameters: Albedo (base color), Roughness (microfacet gloss), Metallic (dielectric vs metal), Emission (glow), IOR (refractive index).",
    "Light Sources: Analytic Point Lights, Directional Sun with soft shadows, Spot Lights, Area Lights.",
    "Atmosphere & Water: Procedural ocean waves with Gerstner wave spectrum, volumetric height fog, and sky dome.",
};

// ------------------------------------------------------- Hotkeys Reference ---
inline constexpr HotkeyEntry kHotkeys[] = {
    { "General",   "F1",                    "Open Controls & Manual window" },
    { "General",   "F11",                   "Toggle Fullscreen" },
    { "Edit",      "Cmd+Z / Ctrl+Z",        "Undo last modification" },
    { "Edit",      "Cmd+Shift+Z / Cmd+Y",   "Redo last modification" },
    { "Edit",      "Cmd+D / Ctrl+D",        "Duplicate selected entity" },
    { "Edit",      "Del / Backspace",       "Delete selected entity" },
    { "Edit",      "Cmd+, / Ctrl+,",        "Open Preferences & Settings" },
    { "Gizmo",     "T or 1",                "Translate Mode" },
    { "Gizmo",     "R or 2",                "Rotate Mode" },
    { "Gizmo",     "S or 3",                "Scale Mode" },
    { "Viewport",  "F",                     "Focus camera on selected entity" },
    { "Viewport",  "RMB + WASD",            "Fly Camera Movement" },
    { "Viewport",  "RMB + Q / E",           "Fly Camera Down / Up" },
    { "Viewport",  "RMB + Shift",           "Camera Fly Sprint Mode" },
    { "Viewport",  "RMB + Scroll",          "Adjust Fly Speed dynamically" },
    { "Play Mode", "Cmd+P / Ctrl+P",        "Play / Stop Simulation" },
    { "Play Mode", "Cmd+Shift+P",           "Pause / Resume Simulation" },
    { "Play Mode", "Cmd+. / Ctrl+.",        "Step Single Frame" },
};

} // namespace lucida::manual
