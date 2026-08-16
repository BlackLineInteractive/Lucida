# Lucida Engine — Status vs. Reference Books

> Comparison of the current engine state against GEA (Gregory), RTR (Möller),
> PBRT (Pharr) and DOD (Fabian). Updated manually after each milestone.

---

## Legend

| Mark | Meaning |
|------|---------|
| done | Implemented, tests passing |
| partial | Scaffold / stub exists, logic missing |
| missing | Not yet started — on roadmap only |

---

## 1. Engine Layers  (GEA ch. 1.6 — Runtime Engine Architecture)

| Layer | GEA definition | Lucida | Status |
|-------|---------------|--------|--------|
| Platform / OS | Window, input, audio, video | `platform_sdl2`, SDL2 event pump | done |
| Core Systems | Memory, containers, math, logging | `engine/core` — FrameArena, Pool, GLM | done |
| Resource Manager | Async load, cache, hot reload | `engine/resource` — sync + Assimp | partial — async missing |
| Rendering Engine | Scene graph, visibility, lighting | `render_metal` — Whitted RT on Metal | done |
| Physics & Collision | Rigid, soft, queries | `physics_jolt` — Jolt Physics | partial — soft body missing |
| Animation | Skeleton, blend tree, IK | `engine/animation` — scaffolds | partial — runtime blend missing |
| Human Interface | Actions, not scancodes | `engine/input` — action map | done |
| Audio | Spatial, streaming, mixer | `engine/audio` — miniaudio | partial — spatial 3D missing |
| Networking | Replication, RPC | `GameplayComponents.h` stubs | partial — transport missing |
| Gameplay Foundation | World, ECS, game loop | `engine/runtime` — World + fixed step | done |
| Game-Specific | AI, scripting, UI, nodes | 107 nodes in `GameplayComponents.h` | partial — logic missing |

---

## 2. Subsystems

### 2.1 Rendering

| Feature | Source | Status |
|---------|--------|--------|
| Two-level BVH (TLAS / BLAS) | RTR ch. 25 | done |
| Radiance Cascades GI | Golubev 2022 | done |
| Whitted RT (analytic shadows, AO) | PBRT ch. 14 | done |
| MetalFX Temporal Upscaling + motion vectors | Apple docs | done |
| PBR — albedo, metallic, roughness, normal, AO, emissive | RTR ch. 9 | done |
| SSAO | RTR ch. 11 | done |
| HBAO / GTAO | RTR ch. 11 | missing — enum exists, kernel absent |
| FXAA | RTR ch. 5 | missing |
| SMAA | Jimenez 2012 | missing |
| TAA | RTR ch. 5 | done — via MetalFX temporal |
| Frustum Culling | GEA ch. 14 | partial — camera only |
| Occlusion Culling | GEA ch. 14 | missing |
| RT-specific culling (coherent ray traversal) | RTR ch. 25 | done |
| Cascade Shadow Maps | RTR ch. 7 | missing |
| Volumetric Fog + trochoidal water | PBRT ch. 15 | done |
| Decal rendering | RTR | partial — component exists |
| GPU Instancing / HLOD | GEA | partial — stubs only |
| Texture Streaming / Basis Universal | GEA ch. 6 | missing |
| Mesh LOD | GEA | partial — component exists, no runtime switch |
| UV editor | editor | missing |

### 2.2 Physics  (GEA ch. 12)

| Feature | Status |
|---------|--------|
| Rigid body dynamics (Jolt) | done |
| Collision detection (Jolt) | done |
| Raycast / shape cast queries | done |
| Trigger volumes | done |
| Character controller | partial — component done, Jolt Character not wired |
| Wheeled vehicle | partial — component done, Jolt VehicleConstraint not wired |
| Physics joints (Hinge, Fixed, …) | partial — component done, constraint not wired |
| Soft body / cloth | partial — component done, simulation missing |
| Destruction | partial — component done, Voronoi missing |
| Buoyancy | partial — component done, water coupling missing |
| Bullet fallback backend | missing (M19) |

### 2.3 Animation  (GEA ch. 11)

| Feature | Status |
|---------|--------|
| Skeleton import (Assimp) | partial |
| Clip playback | partial |
| Cross-fade / blend tree | missing |
| IK (FABRIK / CCD) | partial — component done, solver missing |
| Morph targets | partial — component done |
| Procedural animation (LPAS / Euphoria) | missing (M24) |

### 2.4 AI  (GEA ch. 18)

| Feature | Status |
|---------|--------|
| NavMesh bake (Recast / Detour) | missing |
| Pathfinding (A*) | missing |
| Behavior tree runtime | partial — component done, executor missing |
| FSM | partial — component done, transitions missing |
| Perception (sight / sound) | partial — component done |
| Blackboard | partial — component done |
| Steering behaviors | missing |
| Crowd simulation | missing |

### 2.5 Audio  (GEA ch. 13)

| Feature | Status |
|---------|--------|
| Mono / stereo playback (miniaudio) | done |
| Spatial 3D audio (HRTF) | partial — component done, panner not wired |
| Reverb zones | partial — component done |
| Music streaming | partial — component done |
| Mixer / bus routing | missing |
| DSP effects | missing |

### 2.6 Editor  (GEA — Tool Chain)

| Feature | Status |
|---------|--------|
| ImGui dockspace + panels | done |
| Viewport (ray traced) | done |
| Scene Hierarchy with drag-drop | done |
| Inspector (transform, material) | done |
| Global Undo / Redo (Command pattern) | done |
| Asset Browser (PNG / JPG / HDR / models) | missing |
| UV Editor | missing |
| Terrain brush tools | missing |
| Mesh editing (vertex / edge / face) | missing |
| Play Mode | missing (M22) |
| Project save / load (.json) | partial |
| Build and ship (standalone) | missing (M23) |
| Scripting (Lua + sol2) | missing (M17) |
| Profiler overlay (Tracy) | missing (M18) |
| Node-based material editor | missing (M9) |

### 2.7 Networking

| Feature | Status |
|---------|--------|
| Network Identity / Transport | partial — stubs only |
| State replication | missing |
| RPC | partial — stub only |
| Client / Server loop | missing |

---

## 3. Priority Roadmap

Sorted by impact on the Engine-to-Game path (GEA 1.6).

| Priority | Task | Maps to |
|----------|------|---------|
| P1 | Character Controller — Jolt Character wired to CharacterBodyNode | GEA ch. 12 |
| P1 | NavMesh bake (Recast / Detour) + A* pathfinding | GEA ch. 18 |
| P1 | Play Mode — snapshot / restore world; systems active only in Play | M22 |
| P1 | Asset Browser — PNG / JPG / HDR texture and model picker panel | GEA tool chain |
| P2 | Behavior Tree executor — tick BT per AI entity each frame | GEA ch. 18 |
| P2 | Spatial audio (HRTF) — wire 3D position to miniaudio panner | GEA ch. 13 |
| P2 | Animation clip playback — sample skeleton, apply to SkinnedMesh | GEA ch. 11 |
| P2 | Wheeled Vehicle — wire component to Jolt VehicleConstraint | GEA ch. 12 |
| P2 | Lua scripting (sol2) — hot reload, game logic from script | M17 |
| P3 | HBAO / GTAO kernel in Metal compute | RTR ch. 11 |
| P3 | Cascade Shadow Maps | RTR ch. 7 |
| P3 | Async asset loading (enkiTS job system) | M11 + M14 |
| P3 | Mesh LOD runtime switching | GEA |
| P3 | UV editor panel | editor |
| P3 | Occlusion Culling (portal or HZB) | GEA ch. 14 |
| P4 | Soft body / cloth simulation | GEA ch. 12 |
| P4 | LPAS — Euphoria-grade procedural ragdoll | M24 |
| P4 | Networking transport layer (ENet) | GEA |
| P4 | Vulkan backend | M13 |

---

## 4. Already at Production-Engine Quality

- Deterministic RT on ordinary GPU — no ray tracing hardware required
- Radiance Cascades GI — not stochastic, no denoising, image final at frame one
- TLAS / BLAS two-level BVH with per-frame instance updates
- MetalFX Temporal with per-instance motion vectors — no ghosting
- Global Undo / Redo via Command + EntitySnapshot
- 107 nodes across 17 subsystems — complete gameplay scene vocabulary
- Strict DOD (SoA) in hot path, FrameArena instead of heap allocations
- Layering enforced by CMake — no engine module sees a backend header
