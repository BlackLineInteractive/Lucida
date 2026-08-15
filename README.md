# Lucida Engine

A modular real-time ray tracing engine, aimed narrowly at **procedural materials and
selective ray tracing** — spending rays only where they are visible so ray-traced games
run on ordinary hardware.

Architecture follows three sources strictly: **Game Engine Architecture** (Gregory) for
layering and subsystem inventory, **Game Programming Patterns** (Nystrom) for the
patterns inside those layers, **Data-Oriented Design** (Fabian) for memory layout.

* Rules and layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
* Where it is going, and why each library was chosen: [docs/ROADMAP.md](docs/ROADMAP.md)

## Where it stands

On a MacBook Pro 16" (i9-9880H, **AMD Radeon Pro 5500M 8 GB** — a mid-range mobile GPU
with no ray tracing hardware, everything in compute):

| Scene | Resolution | Path | Frame rate |
| --- | --- | --- | --- |
| Sponza 4K (~5.7 M triangles) | 1080×720 | full RT, unoptimised | **15–30 fps** |
| Demo 0.3 (primitives, water, fog) | 1971×1065 | full RT | 57.8 fps |

This method **cannot** produce noise or ghosting — not "does not yet". Tracing is
deterministic Whitted-style with analytic soft shadows and AO rather than stochastic
sampling, so there is no variance to denoise and no history to accumulate. The image is
final at frame one, and every optimisation on the roadmap has to keep it that way.

(The torn ghosting that used to show on moving objects was never the tracer: the MetalFX
upscaler was given motion vectors that described camera movement only. Motion is now
computed per instance — see
[ARCHITECTURE.md](docs/ARCHITECTURE.md#fixed-in-m7-torn-ghosting-on-moving-objects).)

---

## Build

Dependencies are **not vendored**. CMake fetches them at configure time (glm, bvh v2,
Dear ImGui, ImGuiFileDialog, stb, Jolt); SDL2 and assimp are used from the system when
found, and built from source otherwise.

```bash
cmake -B build
cmake --build build -j
./build/lucida_sandbox
```

### Platforms

| Platform | Architectures | Renderer | State |
| --- | --- | --- | --- |
| macOS 13+ | x86_64, arm64 | Metal + MetalFX | complete tracing path |
| Linux (Debian/Ubuntu) | x86_64, arm64 | OpenGL 4.3 compute | backend not yet ported to `IRenderBackend` |
| Windows 10+ | x86_64, x86 | OpenGL 4.3 / Vulkan | same |

Core, runtime, resource, physics, input and framework build on all three. Exactly one
render backend is production-ready today — Metal. The GL and Vulkan sources sit in
`backends/render_gl` and `backends/render_vulkan` in the form inherited from
RayTracer_Unified and are not yet wired to the current interface (see roadmap M12–M13).

**Debian / Ubuntu**

```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libassimp-dev
```

**Windows (MSVC)**

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config RelWithDebInfo
```

**macOS universal (x86_64 + arm64)**

```bash
cmake -B build -DLUCIDA_MACOS_UNIVERSAL=ON
```

### CMake options

| Option | Default | Effect |
| --- | --- | --- |
| `LUCIDA_RENDER_METAL` | ON on Apple | Metal backend |
| `LUCIDA_RENDER_GL` | ON off Apple | OpenGL 4.3 compute |
| `LUCIDA_RENDER_VULKAN` | OFF | Vulkan (stub) |
| `LUCIDA_PHYSICS_JOLT` | ON | Jolt |
| `LUCIDA_PHYSICS_BULLET` | OFF | Bullet (mutually exclusive with Jolt) |
| `LUCIDA_PREFER_SYSTEM_DEPS` | ON | Use system SDL2/assimp when present |
| `LUCIDA_MACOS_UNIVERSAL` | OFF | Universal macOS binary |

---

## Running

```bash
./build/lucida_sandbox --scene lab             # basic | water | lab
./build/lucida_sandbox --scene world.json      # or a scene file
./build/lucida_sandbox --scene lab --export-scene world.json  # write one out to edit
./build/lucida_sandbox --mesh model.glb        # load a model at startup
./build/lucida_sandbox --bench 90              # 90 frames, print timings
./build/lucida_sandbox --bench 90 --shot f.png # same, plus a frame capture
./build/lucida_sandbox --verbose               # debug-level logging
```

`--bench` needs nobody watching: it is how you check the renderer is alive without
looking at a window.

### Controls

| Keys | Action |
| --- | --- |
| W A S D | move |
| Shift | sprint |
| Space / Ctrl | jump and crouch (walk) or up and down (fly) |
| F | toggle walk / fly |
| Tab, Esc | menu and cursor |
| V | fog |
| F11 | fullscreen |

---

## Layout

```
Lucida/
 cmake/Dependencies.cmake   third-party fetching
 engine/
    core/       memory, containers, maths, logging, events, services
    runtime/    fixed-step game loop, world, systems
    render/     camera, mesh data, IRenderBackend interface
    resource/   model import, texture arrays, BLAS construction
    physics/    IPhysicsBackend
    input/      HID layer: actions, not scancodes
 backends/
    render_metal/    Metal + MetalFX
    platform_sdl2/   window, input, surface
    physics_jolt/    Jolt
 framework/      UI/UX: ImGui shell, camera controller
 apps/sandbox/   the demo that binds concrete backends together
```

The rule that holds it together: **a module sees only what its
`target_link_libraries` lists.** No `engine/*` file includes SDL2, Metal, Jolt or
ImGui — choosing backends is the application's job.

---

## Licence

Lucida Engine is free software under the **GNU General Public License v3.0 or later** —
see [LICENSE](LICENSE). You may use, study, modify and redistribute it; derived works
that you distribute must carry the same freedoms.

Every dependency is permissively licensed (MIT, BSD-3, Zlib) and fetched at configure
time rather than vendored. The full list, with licences and GPL compatibility notes, is
in [THIRD-PARTY.md](THIRD-PARTY.md).

```
Copyright (C) 2026 BlackLine Interactive
```
