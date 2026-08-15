#pragma once
// The built-in scenes, as data.
//
// These used to be a switch inside the Metal backend, which meant every scene
// was also a renderer change and no second backend could show anything. They are
// ordinary functions now: they build a RenderScene and hand it back.

#include "lucida/render/Scene.h"

namespace lucida::scenes {

// Analytic primitives under a hard-shadowed key light. Uses the plain Whitted
// kernel: no meshes, no fog, no water.
RenderScene BasicPrimitives();

// Adds a water surface and volumetric fog over the same primitives.
RenderScene WaterAndFog();

// A row of plinths, each carrying a sphere with a different procedural surface,
// so the whole material axis — conductor to dielectric, mirror to fully rough,
// opaque to refractive — is visible side by side in one frame.
RenderScene MaterialLab();

// Stable order for UI and command lines.
enum class BuiltIn : u8 { BasicPrimitives = 0, WaterAndFog, MaterialLab, Count };

RenderScene Build(BuiltIn which);
const char* Name(BuiltIn which);

} // namespace lucida::scenes
