// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// The built-in scenes.
//
// They populate a registry with entities and fill in the material palette, so
// everything they create is selectable, inspectable and movable. A scene that
// builds raw GPU arrays is a picture; a scene that builds entities is a world.

#include "lucida/framework/SceneAssets.h"

namespace lucida::scenes {

// Nothing but a ground grid and a gradient sky. This is what a new project
// opens onto: an empty stage, not somebody else's demo.
SceneAssets Empty(Registry& registry);

// The original Radiance Cascades 3D Cornell box demo scene with colored diffuse walls,
// mirror reflections, glass refractions, and multi-cascade dynamic lighting.
SceneAssets RadianceCascades3D(Registry& registry);

// Analytic primitives under a hard-shadowed key light.
SceneAssets BasicPrimitives(Registry& registry);

// Adds a water surface and volumetric fog over the same primitives.
SceneAssets WaterAndFog(Registry& registry);

// A row of plinths, each carrying a sphere with a different procedural surface,
// so the whole material axis is visible side by side in one frame.
SceneAssets MaterialLab(Registry& registry);

// Active physics simulation with dynamic spheres, domino stacks, and character controllers.
SceneAssets PhysicsPlayground(Registry& registry);

enum class BuiltIn : u8 {
    Empty = 0,
    RadianceCascades3D,
    BasicPrimitives,
    WaterAndFog,
    MaterialLab,
    PhysicsPlayground,
    Count
};

SceneAssets Build(BuiltIn which, Registry& registry);
const char* Name(BuiltIn which);

} // namespace lucida::scenes
