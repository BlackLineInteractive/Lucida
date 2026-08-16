<p align="center">
  <img src="media/lucida.jpg" alt="Lucida Engine" width="100%">
</p>

<p align="center">
  <em>lucida</em> - the brightest star in a constellation.
</p>

<p align="center">
  <a href="LICENSE"><img alt="Licence: GPL-3.0-or-later" src="https://img.shields.io/badge/licence-GPL--3.0--or--later-blue.svg"></a>
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-blue.svg">
  <img alt="Platforms" src="https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey.svg">
  <img alt="Renderer" src="https://img.shields.io/badge/renderer-Metal%20ray%20tracing-orange.svg">
  <a href="https://t.me/blacklineinteractive"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-2CA5E0?logo=telegram&logoColor=white"></a>
  <a href="https://youtube.com/@blacklineinteractive"><img alt="YouTube" src="https://img.shields.io/badge/YouTube-FF0000?logo=youtube&logoColor=white"></a>
</p>

---

A modular real-time ray tracing **Graphics and Game Engine**, aimed narrowly at **procedural materials and selective ray tracing** - spending rays only where they are visible so ray-traced games run on ordinary hardware.

Architecture follows three sources strictly: **Game Engine Architecture** (Gregory) for layering and subsystem inventory, **Game Programming Patterns** (Nystrom) for the patterns inside those layers, **Data-Oriented Design** (Fabian) for memory layout.

* Rules and layering: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
* Where it is going, and why each library was chosen: [docs/ROADMAP.md](docs/ROADMAP.md)
* Frame and memory budgets, and where a small engine beats a large one: [docs/PERFORMANCE.md](docs/PERFORMANCE.md)

<p align="center">
  <img src="media/0.3.png" alt="Lucida Engine 0.3 Viewport" width="100%">
</p>

<p align="center">
  <img src="media/0.3(2).png" alt="Lucida Engine 0.3 Editor" width="100%">
</p>

<p align="center">
  <img src="media/0.2.png" alt="Lucida Engine 0.2 Viewport" width="100%">
</p>

<p align="center">
  <img src="media/0.2(2).png" alt="Lucida Engine 0.2 Editor" width="100%">
</p>

## Where it stands

On a MacBook Pro 16" (i9-9880H, **AMD Radeon Pro 5500M 8 GB** - a mid-range mobile GPU
with no ray tracing hardware, everything in compute):

| Scene | Resolution | Path | Frame rate |
| --- | --- | --- | --- |
| Sponza 4K (~5.7 M triangles) | 1080×720 | full RT, unoptimised | **15-30 fps** |
| Demo 0.3 (primitives, water, fog) | 1971×1065 | full RT | 57.8 fps |

This method **cannot** produce noise or ghosting - not "does not yet". Tracing is
deterministic Whitted-style with analytic soft shadows and AO rather than stochastic
sampling, so there is no variance to denoise and no history to accumulate. The image is
final at frame one, and every optimisation on the roadmap has to keep it that way.

(The torn ghosting that used to show on moving objects was never the tracer: the MetalFX
upscaler was given motion vectors that described camera movement only. Motion is now
computed per instance - see
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
render backend is production-ready today - Metal. The GL and Vulkan sources sit in
`backends/render_gl` and `backends/render_vulkan` in the form inherited from
RayTracer_Unified and are not yet wired to the current interface (see roadmap M12-M13).

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

### Projects

A game is a folder, not a config file. Create one, then open it:

```bash
./build/lucida_sandbox --new-project MyGame    # scaffolds the layout below
./build/lucida_sandbox --project MyGame        # opens it
```

```
MyGame/
├── project.json      name, startup scene, window and render defaults
├── scenes/           .json scenes
├── assets/           models and textures
└── scripts/          Lua, once scripting lands
```

Everything inside is stored project-relative, so the folder can be zipped, moved to
another machine and opened there.

### Direct launch

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

### Controls & Shortcuts

| Keys / Mouse | Action |
| --- | --- |
| RMB + Mouse | Look around in Viewport / Game mode |
| RMB + W A S D | Fly camera movement |
| RMB + Q / E | Fly down / up |
| RMB + Shift | Sprint speed boost |
| RMB + Scroll | Adjust camera fly speed interactively |
| F | Focus camera on selected object |
| 1 / 2 / 3 (or T / R / S) | Switch Gizmo mode (Translate / Rotate / Scale) |
| Cmd+Z (Ctrl+Z) | Global Undo (Transforms, Materials, Hierarchy, Primitives, Deletion) |
| Cmd+Shift+Z / Cmd+Y | Global Redo |
| Cmd+D (Ctrl+D) | Duplicate selected entity with full state |
| Delete / Backspace | Delete selected entity (restorable via Undo) |
| Tab / Esc | Toggle UI overlay and cursor capture |
| F11 | Toggle Fullscreen |

### Key Engine Features

- **Unified Metal Compute Ray Tracer**: Deterministic Whitted RT with analytic soft shadows, directional sun casting, golden-angle hemisphere AO, multi-frequency trochoidal water waves, and volumetric fog.
- **Hardware Upscaling**: Sub-pixel Halton jittering with per-instance Motion Vectors and Apple MetalFX Temporal Upscaling.
- **Live PBR & Material Inspector**: 1-click automotive and metallic presets, live Albedo/Metallic/Roughness/Emission/IOR editing, and normal map support.
- **Complete Scene Hierarchy**: Multi-part 3D model node tree extraction, drag-and-drop parenting, and 3D non-uniform scaling with uniform lock toggle.
- **Global Undo / Redo Architecture**: Command pattern with `EntitySnapshot` component serialization covering all viewport and inspector interactions.
- **Real-Time Lighting & Post-Processing**: Live sun positioning, sky irradiance, ACES/Reinhard/Filmic tonemapping, gamma correction, and Bayer dithering.

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
ImGui - choosing backends is the application's job.

---

## Licence

Lucida Engine is free software under the **GNU General Public License v3.0 or later** -
see [LICENSE](LICENSE). You may use, study, modify and redistribute it; derived works
that you distribute must carry the same freedoms.

Every dependency is permissively licensed (MIT, BSD-3, Zlib) and fetched at configure
time rather than vendored. The full list, with licences and GPL compatibility notes, is
in [THIRD-PARTY.md](THIRD-PARTY.md).

```
Copyright (C) 2026 BlackLine Interactive
```

---

## Follow the work

Development notes, builds and breakdowns:

<p align="left">
  <a href="https://t.me/blacklineinteractive"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-2CA5E0?style=for-the-badge&logo=telegram&logoColor=white"></a>
  <a href="https://youtube.com/@blacklineinteractive"><img alt="YouTube" src="https://img.shields.io/badge/YouTube-FF0000?style=for-the-badge&logo=youtube&logoColor=white"></a>
</p>

Issues and pull requests are welcome. If you are opening a pull request, read
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) first - the dependency rules there are
enforced by CMake, and a change that breaks them will not link.
