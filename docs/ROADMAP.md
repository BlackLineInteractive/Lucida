# Lucida — Roadmap

## What this engine is for

Lucida is a **narrow, opinionated engine**: procedural materials plus selective ray
tracing. The goal is not another UE, it is to make ray-traced graphics affordable on
ordinary hardware — spend a ray only where a ray is visible (glass, mirrors, water,
caustics, contact shadows), and compute everything else from procedural material code
that costs no memory and ships no gigabytes of textures.

Every decision below follows from that:

* **Memory beats features.** A procedural texture is 40 lines of shader instead of
  60 MB of BC7. That is the optimisation the engine sells.
* **Rays are scarce.** A material declares which effects are worth a ray. Everything
  else is approximated cheaply.
* **One image across hardware tiers.** One shader, several quality tiers — not
  separate renderers for weak and strong machines.
* **No noise, no ghosting, ever.** Cheaper tiers drop effects; they never buy frame
  time with temporal accumulation artefacts or denoiser mush.

### Where we start from

Reference machine: MacBook Pro 16", i9-9880H, **AMD Radeon Pro 5500M 8 GB** — a
mid-range mobile GPU with no ray tracing hardware, everything in compute.

| Scene | Resolution | Path | Frame rate |
| --- | --- | --- | --- |
| Sponza 4K (~5.7 M tris) | 1080×720 | full RT, unoptimised | **15–30 fps** |
| Demo 0.3 (primitives, water, fog) | 1971×1065 | full RT | 57.8 fps |

Sponza at 15–30 fps is the number the roadmap exists to move. It is also already
clean: no noise, no ghosting, no denoiser softness, because the path is deterministic
rather than stochastic. The optimisation work below has to raise the frame rate
**without** spending that property.

---

## Dependency decisions

Same rule as [ARCHITECTURE.md](ARCHITECTURE.md) §2: take what exists, hide it behind a
facade. Below is the choice and the reason, not a catalogue of what is available.

| Area | Choice | Why this one |
| --- | --- | --- |
| ECS | **EnTT** | header-only, sparse sets that match our SoA layout, no runtime of its own. Flecs gives hierarchies and queries out of the box but brings its own world model — more than this profile needs |
| Job system | **enkiTS** | ~2k lines, task-parallel, exactly the shape of "update N systems". Taskflow's dependency graph is heavier; revisit if a real graph appears |
| Shaders | **slang** (primary) + **SPIRV-Cross** | one source → SPIR-V, MSL, HLSL, and slang understands ray tracing constructs the GLSL→SPIR-V path does not expose. `glslang`/`shaderc` stay as the fallback for GLSL sources |
| Vulkan memory | **VMA** | hand-rolling `VkDeviceMemory` is two weeks and a pile of bugs VMA already fixed |
| RHI | **our own, thin** | neither bgfx nor sokol_gfx exposes `VK_KHR_ray_query` or Metal intersection functions. A wrapper that hides exactly the feature this engine is built on is pointless. `IRenderBackend` already exists; it needs finishing, not replacing |
| Textures | **stb_image** + **KTX-Software / Basis** | stb for import, KTX2+Basis to transcode into BCn/ASTC/ETC for the actual GPU |
| Fonts | **msdf-atlas-gen** + **FreeType** | SDF atlas: crisp text at any scale from one texture |
| Audio | **miniaudio** | single file, mixer, spatial audio, decoders |
| Scripting | **Lua 5.4** + **sol2** | sol2 removes the manual Lua stack; bindings read as ordinary C++ |
| Profiler | **Tracy** | per-frame zones, memory, threads, GPU, live |
| Logging | **our front end + fmt** | the channelled log already works; only the output backend becomes `fmt`. spdlog as a whole is not needed |
| Config / saves | **nlohmann/json** | ergonomics beat throughput for a file read once per launch. `yyjson` if the profiler ever disagrees |
| File dialogs | **nativefiledialog-extended** | a native picker instead of an ImGui imitation |
| Editor | **ImGui** + **ImGuizmo** + **ImPlot** + **ImNodes** | transform gizmos, frame-time plots, node graph for materials |
| UI motion | **ImAnim** (Soufiane KHIAT, MIT) | easing presets, colour-space-aware blending and tween caching already solved; controls answer the pointer instead of snapping |

The editor theme is carried over from Climax Game Engine Toolkit: cool neutral chrome,
one warm accent (C++ blue) that marks state — checkmarks, slider grabs, the active tab,
selection. `framework/Theme.h` owns it, and no panel hardcodes a colour.

Everything is fetched by `cmake/Dependencies.cmake`. Nothing lands in the repository.

---

## Milestones

### M7 — Get the scene out of the shader

The demo scenes were baked into `MetalBackend::SetupScene`. While that held, a second
backend, an editor and procedural materials were all blocked.

Two assumptions fell out of the change, both invisible until a scene could contradict
them: the tracer traced analytic primitives *or* mesh instances but never both, and
`AddMesh` zeroed the sphere, cube and light counts. A mesh is one more thing in a
scene, not the scene.

* **Done:** `engine/render/Scene.h` holds `RenderScene`; `framework/SceneLibrary` builds
  the three built-in scenes; `IRenderBackend::SubmitScene` replaced `SetDemoScene`; the
  backend keeps no scene of its own and `--scene basic|water|lab` picks one at launch

### M7b — Scene as an asset

A scene that exists only as C++ in `apps/sandbox` is a demo. An engine loads a scene
it did not compile.

* **Done:** `engine/resource/SceneSerializer` reads and writes `RenderScene` as JSON.
  Enums are names (`"glass"`, not `2`), materials are referenced by name so inserting
  one at the top of a file does not repaint everything below it, vectors stay on one
  line, and floats print as the shortest decimal that reads back identically.
* `--export-scene out.json` writes a built-in out as a starting point;
  `--scene out.json` loads it. No rebuild to change the world.
* Still to come with M10: entities and mesh references in the file. Today it carries
  analytic geometry, materials, lights, environment and the spawn point.

### M8 — Selective ray tracing: quality tiers ⬅ next

The point of the engine. A material declares which effects deserve a ray.

```
EffectMask: REFLECT | REFRACT | SHADOW | AO | CAUSTIC | GI
RayBudget:  rays per pixel this frame is allowed to spend
```

* `RenderTier`: `Baseline` (zero rays — procedural shading plus screen-space
  approximations), `Selective` (rays only for flagged materials), `Full` (today's path)
* One tracing kernel; a tier disables branches instead of swapping shaders
* The ray budget adapts inside the frame: when the frame misses its target, drop
  effects by priority rather than resolution
* **Done when:** Sponza 4K at 1080×720 holds 60 fps on `Selective` against the
  15–30 fps the full path gives today, tiers switch without a reload, and no tier
  introduces noise, ghosting or denoiser softness

### M9 — Procedural material library (the differentiator)

* Grow the existing ten patterns (`PROC_MARBLE`…`PROC_CONCRETE`) into a library:
  Perlin/Simplex/Worley/FBM noise, domain warping, triplanar projection
* A material is data, not a `switch` in the shader: a node graph compiled to code
* Node editor on **ImNodes** with live preview
* **Done when:** a Sponza-scale scene renders with no raster textures at all and
  stays under 200 MB of VRAM

### M10 — ECS on EnTT

* `engine/core/ecs`: a facade over EnTT (registry, views, groups) — application code
  never includes `entt.hpp` directly
* `ISystem` implementations iterate views instead of hand-rolled arrays
* Move the vehicle, the camera and render instances onto components
* **Done when:** `World` holds no entity container of its own

### M11 — Job system on enkiTS

* `engine/core/jobs`: `JobScheduler`, `ParallelFor`, task dependencies
* Parallelise BVH construction (already partly), texture packing, system updates,
  frame setup
* **Done when:** model loading and world update scale across cores, and Tracy shows
  every worker busy rather than idling

### M12 — Shader pipeline

* Move the kernels to **slang**, compiled to SPIR-V and MSL offline, with hot reload
  in debug builds
* **SPIRV-Cross** for backends that need transpilation
* On-disk cache of compiled variants — today every launch compiles `.metal` from source
* **Done when:** one shader source runs on Metal and Vulkan, and startup does not wait
  on a compiler

### M13 — Vulkan backend

* `backends/render_vulkan` against the current `IRenderBackend`
* **VMA** for all device memory
* `VK_KHR_ray_query` where present, compute fallback where not
* **Done when:** the same scene renders on Linux with no change to application code

### M14 — Resources and compression

* Asynchronous loading through the job system
* **KTX-Software + Basis Universal**: transcode to BCn/ASTC/ETC per GPU
* Cache of prepared assets next to the source
* **Done when:** a second launch with a large model starts from cache in under a second

### M15 — Text and editor shell

* **msdf-atlas-gen** + FreeType → SDF atlas, crisp text at any DPI
* **ImGuizmo** (transforms), **ImPlot** (frame plots),
  **nativefiledialog-extended** replacing ImGuiFileDialog
* **Done when:** an object can be dragged with a gizmo and the result appears in frame

### M16 — Audio

* `engine/audio` plus `backends/audio_miniaudio`, shaped like the physics split
* Spatial audio, mixer, music streaming
* **Done when:** a sound source is an ECS component with a world position

### M17 — Scripting

* **Lua 5.4 + sol2** in `engine/script`
* Hot reload, game logic and scene setup from script
* Sandboxed: a script sees only the API explicitly registered for it
* **Done when:** the demo scene is described by a script and edits apply without a rebuild

### M18 — Tooling

* **Tracy**: zones around frame phases, GPU timings, allocators
* **fmt** as the backend of the existing log front end
* **nlohmann/json** for config and saves, replacing `key=value`
* **Done when:** a frame-time spike is attributable to a system in Tracy

### M19 — Bullet as a second physics backend

Not for Bullet's sake, but to prove the physics abstraction is one. Finish
`CreateBody` in the Jolt backend along the way.

---

## Order and dependencies

```
M7 (scene out of shader)
 ├── M7b (scene as an asset) ── M10 (ECS) ── M15 (editor) ── M17 (scripting)
 ├── M8 (quality tiers) ─────── M9 (procedural materials)
 ├── M12 (shader pipeline) ──── M13 (Vulkan + VMA)
 └── M11 (jobs) ── M14 (resources) ── M16 (audio)
M18 (tooling) — in parallel, any time
M19 (Bullet) — after M10
```

Two different goals run through this list, and they are worth keeping apart:

* **What makes it an engine** — M7, M7b, M10, M15, M17. Someone other than the author
  can build a world in it without touching C++.
* **What makes it worth choosing** — M8, M9. Ray-traced graphics on hardware that has
  no ray tracing units.

The engine half comes first. A renderer nobody can author content for is a demo,
however fast it runs.
