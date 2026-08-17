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
    { "RMB + Mouse",            "Look around in viewport with captured cursor." },
    { "RMB + W / A / S / D",     "Fly forward, left, backward, right in 3D world space." },
    { "RMB + Q / E",             "Fly down (Q) or up (E)." },
    { "Shift + MMB",             "Pan camera in viewport plane (Blender standard)." },
    { "MMB Drag",                "Orbit camera around selected entity or scene origin." },
    { "3D Gizmo Drag",           "Drag orientation sphere in top-right to rotate view." },
    { "RMB + Shift",             "Sprint fly: 3.0x speed boost." },
    { "RMB + Mouse Wheel",       "Adjust fly speed dynamically." },
    { "LMB Click",               "Select entity under cursor (raycast picking)." },
    { "F",                       "Focus and frame camera on selected entity." },
    { "View Preset Menu",        "Snap camera to Top, Bottom, Front, Back, Left, Right, Isometric." },
    { "F11",                     "Toggle fullscreen window." },
};

// --------------------------------------------------------------------- Gizmo -
inline constexpr Entry kGizmoEntries[] = {
    { "T or 1",                  "Translate Gizmo (move along X, Y, Z)." },
    { "R or 2",                  "Rotate Gizmo (rotate around Euler axes)." },
    { "S or 3",                  "Scale Gizmo (scale uniformly or per-axis)." },
    { "Local / World",           "Switch Gizmo coordinate space between Local and World." },
    { "Snap Toggle",             "Toggle grid and angle snapping." },
    { "Cmd+Z / Ctrl+Z",          "Undo last change (transforms, materials, mesh edits, hierarchy)." },
    { "Cmd+Shift+Z / Cmd+Y",     "Redo last undone change." },
    { "Cmd+D / Ctrl+D",          "Duplicate selected entity." },
    { "Del / Backspace",         "Delete selected entity (undoable)." },
    { "Hierarchy Drag & Drop",   "Drag entity onto another to configure parent-child transform chain." },
};

// ------------------------------------------------------------- Mesh Modeling -
inline constexpr Entry kMeshModelingEntries[] = {
    { "Tab",                     "Toggle between Object Mode and Mesh Edit Mode." },
    { "1 / 2 / 3",               "Switch selection mode: Vertex (1), Edge (2), Face (3)." },
    { "E",                       "Extrude selected face along its normal." },
    { "I",                       "Inset selected face." },
    { "Subdivide",               "Subdivide face or mesh to increase geometric detail." },
    { "Recalculate Normals",     "Recompute smooth face and vertex normals." },
    { "Generate UVs",            "Box / Triplanar UV coordinate projection." },
};

// ----------------------------------------------------------------- Play Mode -
inline constexpr Entry kPlayModeEntries[] = {
    { "Play / Stop (Cmd+P)",     "Toggle Play Mode with non-destructive state snapshot and restore." },
    { "Pause (Cmd+Shift+P)",     "Freeze physics and gameplay ticks." },
    { "Step (Cmd+.)",            "Advance simulation by exactly 1 tick (1/60s)." },
    { "Dynamic Body",            "Responds to gravity (-9.81 m/s²), forces, and surface collisions." },
    { "Static Body",             "Immovable collider (ground, architecture, terrain)." },
    { "Kinematic Body",          "Driven by animations and scripts, pushes dynamic bodies." },
};

// --------------------------------------------------- Materials & Rendering ---
inline constexpr const char* kRenderingBullets[] = {
    "Whitted Ray Tracer: Deterministic Metal compute ray tracer with analytic soft shadows and reflections.",
    "Radiance Cascades: 3D Global Illumination with hierarchical interval radiance transport.",
    "PBR Pipeline: Albedo, Roughness, Metallic, Normal Maps, Emission, and Index of Refraction.",
    "Atmosphere & Sun: Directional sun lighting, sky dome irradiance, volumetric height fog, ocean wave simulation.",
};

// ------------------------------------------------------- Hotkeys Reference ---
inline constexpr HotkeyEntry kHotkeys[] = {
    { "General",   "F1",                    "Open Controls & Manual window" },
    { "General",   "F11",                   "Toggle Fullscreen" },
    { "Edit",      "Cmd+Z / Ctrl+Z",        "Undo last action" },
    { "Edit",      "Cmd+Shift+Z / Cmd+Y",   "Redo last action" },
    { "Edit",      "Cmd+D / Ctrl+D",        "Duplicate selected entity" },
    { "Edit",      "Del / Backspace",       "Delete selected entity" },
    { "Edit",      "Cmd+, / Ctrl+,",        "Open Preferences window" },
    { "Gizmo",     "T or 1",                "Translate Mode" },
    { "Gizmo",     "R or 2",                "Rotate Mode" },
    { "Gizmo",     "S or 3",                "Scale Mode" },
    { "Viewport",  "F",                     "Focus camera on selection" },
    { "Viewport",  "RMB + WASD",            "Fly camera movement" },
    { "Viewport",  "RMB + Q / E",           "Fly down / up" },
    { "Viewport",  "RMB + Shift",           "Sprint fly mode" },
    { "Viewport",  "Shift + MMB",           "Pan camera in viewport" },
    { "Viewport",  "MMB",                   "Orbit camera around focus" },
    { "Modeling",  "Tab",                   "Toggle Edit / Object Mode" },
    { "Play Mode", "Cmd+P / Ctrl+P",        "Play / Stop simulation" },
    { "Play Mode", "Cmd+Shift+P",           "Pause / Resume simulation" },
    { "Play Mode", "Cmd+. / Ctrl+.",        "Step single frame" },
};

} // namespace lucida::manual
