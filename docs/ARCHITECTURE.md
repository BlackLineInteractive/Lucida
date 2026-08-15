# Lucida Engine — Architecture

This document fixes the **rules** the engine grows by. Anything that contradicts them
does not enter the tree. Every module is tied to a specific part of the sources:

| Source | Short | Role here |
| --- | --- | --- |
| Jason Gregory, *Game Engine Architecture* (3rd ed.) | **GEA** | runtime layering, which subsystems exist |
| Robert Nystrom, *Game Programming Patterns* — gameprogrammingpatterns.com | **GPP** | patterns inside those layers |
| Richard Fabian, *Data-Oriented Design* — dataorienteddesign.com/dodbook | **DOD** | memory layout, SoA, ECS |

Chapters actually leaned on, by the books' real table of contents:

* **GPP ch.3 Sequencing** — Game Loop, Update Method, Double Buffer  `engine/runtime`
* **GPP ch.5 Decoupling** — Component, Event Queue, Service Locator  `engine/core`
* **GPP ch.6 Optimization** — Data Locality, Object Pool, Spatial Partition, Dirty Flag
* **DOD ch.2 Relational Databases** — normalisation, primary keys  handles, not pointers
* **DOD ch.4 Component Based Objects** — components instead of hierarchies
* **DOD ch.8 Optimisations** — SoA over AoS  the `GPUTriPos` / `GPUTriAttr` split
* **DOD ch.9 Helping the Compiler** — cache behaviour, aliasing, branch prediction

Where the engine is going: [ROADMAP.md](ROADMAP.md).
What it must cost: [PERFORMANCE.md](PERFORMANCE.md).
Licence: GPL-3.0-or-later, see [LICENSE](../LICENSE) and [THIRD-PARTY.md](../THIRD-PARTY.md).

---

## 1. Runtime layers (GEA 1.6)

Bottom to top. **Dependencies point down only.** An upper layer knows the lower one;
never the reverse.

```

     apps/                 sandbox, benchmark        game / tool



     framework/            UI/UX, editor shell,      GEA 1.6.15 (game-specific)
                           debug menus, gizmos       GPP: Command, State



    runtime    render     physics   resource     input       GEA 1.6.9–1.6.14
    game loop front end  interface  manager     HID layer
    world     IRenderB.





     engine/core/         memory, containers, math   GEA 1.6.5 (core systems)
                          diag, events, services     GEA 1.6.4 (platform layer)
                          ecs, platform              DOD 1–4


     backends/           implementations: render_metal, render_vulkan, render_gl,
                         physics_jolt, physics_bullet, platform_sdl2
                          each depends on its own engine module and nothing else
```

### The hard rule

1. `core` depends on **nothing** but the standard library and `glm`.
2. `engine/*` modules depend on `core` (and, where stated, on `render`).
3. `backends/*` depend on **their own** interface module. A backend never links
   another backend.
4. The **application** picks the backends, not the engine. The engine sees interfaces.
5. No `engine/*` file includes SDL2, Metal, Jolt, Bullet or ImGui.

CMake enforces this mechanically: every module is a target whose
`target_link_libraries` lists **only** its permitted neighbours. A violation is a link
error, not a code review comment.

---

## 2. What is taken and what is written

The rule: **maths, BVH/SAH and physics are not rewritten.** Take the best available and
wrap it, so the library stays replaceable.

| Subsystem | Choice | Why |
| --- | --- | --- |
| Vector maths | `glm` | de-facto standard, SIMD paths, GLSL semantics |
| BVH + binned SAH | `bvh v2` (madmann91) | multithreaded build, proven tree quality |
| Physics | `Jolt` (default), `Bullet` (option) | Jolt: multithreaded, cache-friendly, AAA lineage |
| Model import | `assimp` | glTF 2.0 / GLB / OBJ / FBX |
| UI | `Dear ImGui` | immediate mode, no infrastructure to maintain |
| Window / input | `SDL2` | isolated in `backends/platform_sdl2` |

Written by hand only what cannot be bought: the layering, allocators, ECS, the game
loop, the render front end and the tracing kernels.

The full list of libraries planned for later subsystems, with the reason for each
choice, is in [ROADMAP.md](ROADMAP.md).

---

## 3. Modules

### `engine/core` — GEA 1.6.4–1.6.5, DOD

| Folder | Contents | Source |
| --- | --- | --- |
| `platform/` | fixed-width types, platform detection, monotonic clock | GEA 1.6.4, 8.5 |
| `memory/` | `LinearAllocator` with markers, `PoolAllocator`, `FrameArena` | GEA 6.2 |
| `container/` | generational `Handle`, dense-storage `HandleTable` | DOD 2; GPP: Object Pool |
| `math/` | facade over glm plus `AABB`, `Ray`, `Transform` | — |
| `diag/` | `LUCIDA_ASSERT`/`VERIFY`, channelled log, scoped profiler | GEA 3.3.3, 3.5 |
| `event/` | fixed-capacity `EventQueue` | GPP ch.5 |
| `service/` | `Locator<T>` | GPP ch.5 |
| `ecs/` | `Registry` facade over EnTT, transform hierarchy | DOD 4; GPP ch.5 |

### `engine/runtime` — GPP ch.3

Fixed-step simulation with render interpolation, `World`, systems, `Application`.

### `engine/render`

Front end: camera, mesh data, instance list, the `IRenderBackend` interface.
No graphics API appears here.

### `engine/physics`

`IPhysicsBackend`, `BodyDesc`, `VehicleDesc` — a clean abstraction over Jolt/Bullet.

### `engine/resource` — GEA 6.2

Mesh loading, texture array packing, BLAS construction, scene files and projects.

### `framework`

UI/UX shell: debug menus, statistics, file dialog, camera controller.

---

## 4. Data rules (DOD)

* Hot data is **SoA**, not an array of objects. `GPUTriPos` is separate from
  `GPUTriAttr`, so BVH traversal reads 48 bytes per candidate instead of 128.
* Identity is an integer handle, not a pointer. A handle survives relocation and a
  stale one is detected rather than dereferenced.
* Virtual calls are allowed **at subsystem boundaries** (once per frame), never in
  per-element loops.
* Allocation inside the frame loop comes from an arena that is reset in one `Reset()`.

---

## 5. Status

| Milestone | Contents | State |
| --- | --- | --- |
| M0 | skeleton, CMake graph, this document | done |
| M1 | `core`: memory, containers, diagnostics, events | done |
| M2 | `runtime`: game loop, world, update method | done |
| M3 | SDL2 platform + framework UI | done |
| M4 | `render` + Metal tracing backend | done |
| M5 | `physics` + Jolt | done |
| M6 | sandbox builds and runs | done |
| M7 | scene lifted out of the backend into `RenderScene` | done |
| M7b | scene serialised to JSON, loadable at runtime | done |
| M10 | ECS on EnTT; physics and render sync as systems | done |
| M20 | project folders: create, open, movable paths | done |
| M21+ | editor shell, see [ROADMAP.md](ROADMAP.md) | next |

Verified: `lucida_sandbox --bench 90 --shot f.png` gives 57.8 fps at 1971×1065 rays
(Intel Mac, Metal) and writes the frame out.

### Measured baseline

Reference hardware — MacBook Pro 16", Intel i9-9880H, **AMD Radeon Pro 5500M 8 GB**,
Metal 3. Mid-range mobile GPU with no ray tracing hardware at all: everything below
runs as compute.

| Scene | Resolution | Path | Frame rate |
| --- | --- | --- | --- |
| Sponza 4K (~5.7 M triangles, 4K textures) | 1080×720 | full RT, unoptimised | **15–30 fps** |
| Primitives + water + fog (demo 0.3) | 1971×1065 | full RT | 57.8 fps |

What matters as much as the number: **this method cannot produce noise or ghosting.**
Not "does not today" — cannot. Tracing is deterministic Whitted-style with analytic
soft shadows and AO instead of stochastic sampling, so there is no variance to
denoise and no history to accumulate. The image is final at frame one.

That is a design constraint, not an observation: a cheaper tier in M8 may drop an
effect, but it may never buy frame time with temporal accumulation or a denoiser.

The one place ghosting could enter is the MetalFX temporal upscaler, a stage bolted on
after tracing. It was doing exactly that; see the fix below.

### Fixed in M7: torn ghosting on moving objects

`shader_v03.metal` wrote motion vectors on the assumption stated in its own comment —
"the scene is static, so all motion comes from the camera" — reprojecting the world hit
point through `prev_view_proj` and nothing else. Any instance whose own transform
changed since the previous frame reported camera motion only, so MetalFX resolved
history from the wrong pixels and tore.

Motion is now per instance. `GPUInstance` carries the transform it had in the previously
rendered frame; the shader takes the hit point into instance-local space with the
current `world_to_local`, pushes it back out with the previous `local_to_world`, then
applies `prev_view_proj`. Static instances have both transforms equal, so the maths
collapses to the camera-only case and nothing regresses.

The previous transform advances once per *rendered frame* (`RollInstanceMotion`), not
per `SetInstanceTransform` call — including on the frame an object stops, where prev has
to catch up to current or a stationary object keeps smearing.

### Fixed: black window whenever MetalFX was not upscaling

The only path from the traced image to the screen was `MTLFXTemporalScaler`. When the
render scale was 1:1 the scaler had nothing to upscale and wrote nothing; the ImGui
overlay then drew onto a recycled drawable with `MTLLoadActionLoad`, so the window
showed black with several frames of stale panels smeared over each other. The same hole
meant any machine without MetalFX presented a black window while the tracer worked
perfectly into an offscreen texture.

`present_kernel` now copies the traced image to the drawable, and MetalFX became what it
should always have been: an optimisation layered on top of a path that always works,
engaged only when it is genuinely upscaling.

The lesson is worth keeping: the offscreen readback used for verification never touched
the drawable, so `--bench --shot` produced correct images for a renderer that displayed
nothing. A test that does not exercise the presentation path does not test presentation.

### Fixed: driver crash on the present blit

Moving the presented image to the drawable with the whole-texture
`copyFromTexture:toTexture:` crashed inside the AMD Metal driver. That convenience form
requires identical dimensions, formats *and* mip counts, and when they differ it does
not report an error — it dereferences. The compute copy that replaced it makes no such
demand and samples across any size difference, which is also what the renderer already
relied on before a present texture existed.

Worth remembering as a rule: prefer the operation that degrades to a wrong image over
the one that degrades to a crash in someone else's code.

### Two more assumptions M7 removed

Both were invisible until the scene became data and something contradicted them:

* The tracer traced analytic primitives **or** mesh instances, never both — spheres and
  cubes sat in the `else` branch of the instance loop.
* `AddMesh` zeroed the sphere, cube and light counts, so loading a model silently
  deleted the primitives and unlit the world.

A mesh is one more thing in a scene, not the scene. Both are gone, and the material lab
now renders with a mesh standing in it.

### Debt taken on deliberately

* `LoadMesh` survives beside `AddMesh`/`AddInstance` for compatibility and should go.
* `IPhysicsBackend::CreateBody` is unimplemented in the Jolt backend: the ported world
  knows only a vehicle and static ground.
* The GL and Vulkan backends sit in the tree in their inherited form and do not build.
