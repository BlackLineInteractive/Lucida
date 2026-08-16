# Lucida Engine — Status vs. Reference Books

> Звіт порівняння поточного стану рушія з GEA (Gregory), PBRT, RTR та іншими джерелами.
> Оновлюється вручну після кожного milestone.

---

## Легенда

| Значок | Стан |
|--------|------|
| ✅ | Реалізовано, тести проходять |
| 🔶 | Частково — є scaffold / stub |
| ❌ | Відсутнє — тільки в роадмапі |

---

## 1. Шари рушія (GEA ch. 1.6 — Runtime Engine Architecture)

| Шар | GEA | Lucida | Стан |
|-----|-----|--------|------|
| Platform / OS | Window, input, audio, video | `platform_sdl2`, SDL2 event pump | ✅ |
| Core Systems | Memory, containers, math, logging | `engine/core` — FrameArena, Pool, GLM | ✅ |
| Resource Manager | Async load, cache, hot reload | `engine/resource` — sync + Assimp; hot reload | 🔶 async ❌ |
| Rendering Engine | Scene graph, visibility, lighting | `render_metal` — Whitted RT on Metal | ✅ |
| Physics & Collision | Rigid, soft, queries | `physics_jolt` — Jolt Physics | 🔶 full soft body ❌ |
| Animation | Skeleton, blend tree, IK | `engine/animation` — component scaffolds | 🔶 runtime blend ❌ |
| Human Interface (Input) | Actions, not scancodes | `engine/input` — action map | ✅ |
| Audio | Spatial, streaming, mixer | `engine/audio` — miniaudio backend | 🔶 spatial 3D ❌ |
| Online / Networking | Replication, RPC | `GameplayComponents.h` NetworkXxx stubs | 🔶 stubs only ❌ |
| Gameplay Foundation | World, ECS, game loop | `engine/runtime` — World + fixed step | ✅ |
| Game-Specific | AI, scripting, UI, nodes | `runtime/GameplayComponents.h` — 107 nodes | 🔶 logic ❌ |

---

## 2. Підсистеми (GEA ch. 1)

### 2.1 Рендеринг

| Функція | Джерело | Стан |
|---------|---------|------|
| Two-level BVH (TLAS/BLAS) | RTR ch. 25 | ✅ |
| Radiance Cascades GI | Golubev 2022 | ✅ |
| Whitted RT (analytic shadows, AO) | PBRT ch. 14 | ✅ |
| MetalFX Temporal Upscaling + motion vectors | Apple docs | ✅ |
| PBR — albedo, metallic, roughness, normal, AO, emissive | RTR ch. 9 | ✅ |
| SSAO | RTR ch. 11 | ✅ |
| HBAO / GTAO | RTR ch. 11 | ❌ (settings enum є, kernel відсутній) |
| FXAA | RTR ch. 5 | ❌ |
| SMAA | Jimenez 2012 | ❌ |
| TAA | RTR ch. 5 | ✅ (MetalFX temporal) |
| Frustum Culling | GEA ch. 14 | 🔶 camera only |
| Occlusion Culling | GEA ch. 14 | ❌ |
| RT-специфічне culling (BVH traversal, coherent rays) | RTR ch. 25 | ✅ |
| Cascade Shadow Maps | RTR ch. 7 | ❌ |
| Volumetric Fog | PBRT ch. 15 | ✅ |
| Decal rendering | RTR | 🔶 component є |
| GPU Instancing / HLOD | GEA | 🔶 stubs |
| Texture Streaming / Basis Universal | GEA ch. 6 | ❌ |
| Mesh LOD | GEA | 🔶 LODGroup component, logic ❌ |
| UV Unwrapping tools | editor | ❌ |

### 2.2 Фізика (GEA ch. 12)

| Функція | Стан |
|---------|------|
| Rigid body dynamics (Jolt) | ✅ |
| Collision detection (Jolt) | ✅ |
| Raycast / shape cast queries | ✅ |
| Trigger volumes | ✅ |
| Character controller | 🔶 component, Jolt Character ❌ wired |
| Wheeled vehicle | 🔶 component, Jolt VehicleConstraint ❌ |
| Physics joints (Hinge, Fixed, …) | 🔶 component ✅, constraint ❌ |
| Soft body / cloth | 🔶 component, simulation ❌ |
| Destruction | 🔶 component, Voronoi ❌ |
| Buoyancy | 🔶 component ✅, water coupling ❌ |
| Bullet fallback backend | ❌ (M19) |

### 2.3 Анімація (GEA ch. 11)

| Функція | Стан |
|---------|------|
| Skeleton import (Assimp) | 🔶 |
| Clip playback | 🔶 |
| Cross-fade / blend tree | ❌ |
| IK (FABRIK / CCD) | 🔶 component, solver ❌ |
| Morph targets | 🔶 component |
| Procedural animation (LPAS / Euphoria) | ❌ (M24) |

### 2.4 AI (GEA ch. 18)

| Функція | Стан |
|---------|------|
| NavMesh bake (Recast/Detour) | ❌ |
| Pathfinding (A*) | ❌ |
| Behavior tree runtime | 🔶 component, executor ❌ |
| FSM | 🔶 component, transitions ❌ |
| Perception (sight/sound) | 🔶 component |
| Blackboard | 🔶 component |
| Steering behaviors | ❌ |
| Crowd simulation | ❌ |

### 2.5 Аудіо (GEA ch. 13)

| Функція | Стан |
|---------|------|
| Mono/stereo playback (miniaudio) | ✅ |
| Spatial 3D audio | 🔶 component, 3D pan ❌ |
| Reverb zones | 🔶 component |
| Music streaming | 🔶 MusicTrackComponent |
| Mixer / bus routing | ❌ |
| DSP effects | ❌ |

### 2.6 Editor (GEA — Tool Chain)

| Функція | Стан |
|---------|------|
| ImGui dockspace + panels | ✅ |
| Viewport (ray traced) | ✅ |
| Scene Hierarchy з drag-drop | ✅ |
| Inspector (transform, material) | ✅ |
| Global Undo/Redo (Command pattern) | ✅ |
| Asset Browser (PNG/JPG/HDR) | ❌ |
| UV Editor | ❌ |
| Terrain brush tools | ❌ |
| Mesh editing (vert/edge/face) | ❌ |
| Play mode | ❌ (M22) |
| Project save / load (.json) | 🔶 |
| Build & Ship (standalone) | ❌ (M23) |
| Scripting (Lua + sol2) | ❌ (M17) |
| Profiler overlay (Tracy) | ❌ (M18) |
| Node-based material editor | ❌ (M9) |

### 2.7 Мережа

| Функція | Стан |
|---------|------|
| Network Identity / Transport | 🔶 stubs |
| State replication | ❌ |
| RPC | 🔶 stub |
| Client/Server loop | ❌ |

---

## 3. Пріоритетний роадмап

| Пріоритет | Задача | Відповідає |
|-----------|--------|-----------|
| 🔴 1 | **Character Controller** — Jolt Character wired to CharacterBodyNode | GEA ch. 12 |
| 🔴 2 | **NavMesh bake (Recast/Detour)** + A\* pathfinding | GEA ch. 18 |
| 🔴 3 | **Play Mode** — snapshot/restore world, systems run only in Play | M22 |
| 🔴 4 | **Asset Browser** — PNG/JPG/HDR texture, model picker | GEA tool chain |
| 🟡 5 | **Behavior Tree executor** — tick BT per AI entity each frame | GEA ch. 18 |
| 🟡 6 | **Spatial audio (HRTF)** — wire 3D position to miniaudio panner | GEA ch. 13 |
| 🟡 7 | **Animation clip playback** — sample skeleton, apply to SkinnedMesh | GEA ch. 11 |
| 🟡 8 | **Wheeled Vehicle** — wire to Jolt VehicleConstraint | GEA ch. 12 |
| 🟡 9 | **Lua scripting** (sol2) — hot reload, game logic | M17 |
| 🟢 10 | **HBAO/GTAO** kernel in Metal compute | RTR ch. 11 |
| 🟢 11 | **Cascade Shadow Maps** | RTR ch. 7 |
| 🟢 12 | **Async asset loading** (job system, enkiTS) | M11 + M14 |
| 🟢 13 | **Mesh LOD** runtime switching | GEA |
| 🟢 14 | **UV editor** panel | editor |
| 🟢 15 | **Occlusion Culling** (portal or HZB) | GEA ch. 14 |
| ⚪ 16 | Soft body / Cloth simulation | GEA ch. 12 |
| ⚪ 17 | LPAS — Euphoria-grade procedural ragdoll | M24 |
| ⚪ 18 | Networking — actual transport layer (ENet) | GEA |
| ⚪ 19 | Vulkan backend | M13 |

---

## 4. Унікальне (production-рівень вже зараз)

- ✅ Детермінований RT без RT-ядер — повністю на compute
- ✅ Radiance Cascades GI — не стохастика, без денойзингу
- ✅ TLAS/BLAS двох-рівнева BVH з оновленням per-frame
- ✅ MetalFX Temporal з per-instance motion vectors
- ✅ Global Undo/Redo через Command + EntitySnapshot
- ✅ 107 нод у 17 підсистемах — повний словник gameplay сцени
- ✅ Суворе DOD (SoA) в hot path, FrameArena замість heap
- ✅ Layering enforced by CMake (модуль не бачить бекенд)
