# Performance budgets

The goal is not "fast". The goal is **numbers that fail the build when they regress**.
Everything here is measurable with `--bench`, and every claim in the README has to be
traceable to a line in this file.

## Where a small engine actually beats a large one

Be precise about this, because "better than Unreal" is not a plan.

Unreal, Unity and Godot are not slow because their engineers are worse. They are slow
because they are general: every subsystem must handle every case, the shader
permutation space is enormous, and backwards compatibility is a hard requirement. Those
are the costs of breadth, and Lucida is not going to beat them at breadth.

What breadth makes *structurally* hard for them is where a narrow engine wins:

| Weakness of the big engines | Why it is structural | What Lucida targets |
|---|---|---|
| Shader compilation stutter | Thousands of permutations, compiled on demand, cached per driver | One tracing kernel with tiers as branches. Precompiled variants, cached to disk (M12). **Target: zero compilation during play** |
| Long editor startup | Enormous asset databases, plugin graphs, module loading | **Target: editor open to first frame under 2 s on the reference machine** |
| Texture memory | PBR authoring means gigabytes of maps | Procedural materials are the *default*, not the only option — conventional textures stay fully supported. The win is that a scene *can* be built without them. **Target: a procedurally dressed Sponza-scale scene under 200 MB VRAM** |
| Frame-time spikes | GC, streaming hitches, PSO creation mid-frame | No allocation in the frame loop (arena), no mid-frame pipeline creation. **Target: 99th percentile frame under 1.5x median** |
| Build size and cold start | Runtime carries everything | Modules link only what an app uses. **Target: shipped game binary under 20 MB** |

The unusual specifics are the point: an engine that only does ray tracing does not need
a rasteriser, a shadow-map system, a light-probe baker, a reflection-probe system or a
GI baker. Every one of those is a subsystem the big engines must carry and Lucida
deletes. That is where the optimisation budget comes from — not from writing tighter
loops than Epic.

## Mobile: one effect, one ray

Android and iOS get the same engine with the ray budget cut to the bone. The tier
system (M8) is what makes that possible without a separate mobile renderer.

The shape of it: shade everything with the zero-ray Baseline path, and spend the budget
on **one** traced effect per scene — a mirror, a water surface, a glass panel — chosen
by the material's `EffectMask`. One reflective surface traced properly reads as "this
game has ray tracing" far more convincingly than a whole scene traced badly.

| Budget | Mobile target |
|---|---|
| Rays per pixel, scene average | under 0.1 |
| Traced surfaces per scene | 1, occasionally 2 |
| Trace resolution | quarter of the display, upscaled |
| GPU frame | 8 ms at 60 fps on a mid-range 2022 phone |
| VRAM | 150 MB |

Procedural materials matter more here than anywhere else: texture memory and bandwidth
are the binding constraint on a phone, so a material that costs 40 lines of shader
instead of 60 MB of BC7 changes what fits on the device.

Not scheduled yet. Needs the Vulkan backend (M13) for Android and a Metal iOS target,
both of which follow the tier system.

## Reference hardware

MacBook Pro 16", Intel i9-9880H, **AMD Radeon Pro 5500M 8 GB**, macOS 13+.
A mid-range mobile GPU with no ray tracing units; everything runs as compute. Numbers
below are on this machine unless stated.

## Current measurements

| Scene | Resolution | Path | Frame | GPU |
|---|---|---|---|---|
| Sponza 4K, ~5.7 M tris | 1080x720 | full RT, unoptimised | 15-30 fps | - |
| Material lab, 18 procedural spheres | 1971x1065 | full RT | 51.7 fps | 18.6 ms |
| Water and fog | 1971x1065 | full RT | 61.2 fps | 15.6 ms |
| Basic primitives | 1971x1065 | Whitted kernel | 87.5 fps | 11.1 ms |
| Starter project, 1 sphere | 3584x1938 | full RT, no upscale | 56.2 fps | 11.3 ms |

## Budgets to hold

Frame, on the reference machine, at 1080x720 with the Selective tier (M8):

| Stage | Budget | Notes |
|---|---|---|
| Input and event pump | 0.2 ms | |
| Fixed simulation (physics, systems) | 2.0 ms | at 60 Hz, one tick per frame |
| Render sync and scene submit | 0.5 ms | uploads only what changed |
| GPU trace | 12.0 ms | the tier system exists to hold this line |
| Present and overlay | 1.0 ms | |
| **Total** | **16.6 ms** | 60 fps |

Memory:

| Budget | Limit |
|---|---|
| VRAM, Sponza-scale scene | 200 MB |
| Frame arena | 8 MB per buffer, two buffers |
| Editor resident set | 300 MB |

## Known costs, deliberately carried for now

* **The viewport traces at window resolution, not panel resolution.** In the editor the
  panel is a fraction of the window, so a small viewport currently costs as much as a
  full-screen one — exactly backwards. Fixing this is the next item in M21 and is worth
  more than any micro-optimisation on the list.
* **Two full-screen passes to present in game mode**: the tracer writes the present
  texture, then a compute copy moves it to the drawable. The copy exists so screenshots
  capture what the screen shows. It can be collapsed into one pass when not capturing.

## Rules that keep the budgets

1. **No allocation in the frame loop.** Per-frame scratch comes from the arena, which is
   reset with one pointer write. A `new` inside a system is a bug.
2. **No pipeline or shader creation after startup**, except an explicit hot-reload in
   debug builds. Compilation during play is the single most visible failure mode of the
   large engines and the easiest to avoid by being small.
3. **Uploads are conditional.** `RollInstanceMotion` marks the instance buffer dirty
   only when a transform actually changed. Unconditional uploads are how a still scene
   ends up costing as much as a moving one.
4. **Virtual calls at subsystem boundaries, never per element.** Once per frame is free;
   once per triangle is not.
5. **Hot and cold data are separated.** `GPUTriPos` is 48 bytes because the traversal
   loop reads nothing else; `GPUTriAttr` is only touched on a confirmed hit.
6. **Quality degrades by dropping effects, never by adding noise.** A cheaper tier may
   remove a reflection; it may not introduce temporal artefacts to buy the time back.

## How this is enforced

Today, by hand: `--bench N --shot out.png` prints frame, GPU and profiler slots.

Planned, and the reason this document exists:

* A benchmark scene set committed to the repository, run in CI on every change
* Results compared against the table above, with a tolerance; a regression beyond it
  fails the build
* Frame capture compared against a reference image, so a "faster" change that quietly
  broke the image is caught in the same pass
* Tracy (M18) for the per-system breakdown when a budget is missed
