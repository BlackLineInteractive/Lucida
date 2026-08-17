# Lucida - Roadmap

## Engine Overview

Lucida is an opinionated real-time ray tracing engine combining procedural materials, selective ray tracing, and Data-Oriented Design (DOD).

Core design principles:
- Memory efficiency: procedural textures and compact data formats minimize VRAM footprints.
- Selective tracing: materials declare required ray effects (reflections, refractions, caustics, soft shadows, AO); non-critical effects use fast approximations.
- Unified rendering: identical shader architecture across hardware tiers without relying on dedicated RT cores.
- Deterministic quality: no stochastic noise, accumulation ghosting, or temporal blur.

---

## Hardware Baseline

Reference machine: MacBook Pro 16" (Intel i9-9880H, AMD Radeon Pro 5500M 8 GB - compute-only):

| Scene | Resolution | Pipeline | Frame rate |
| --- | --- | --- | --- |
| Sponza (~5.7 M triangles) | 1080x720 | Full RT | 15-30 fps |
| Demo Scene (primitives, water, fog) | 1920x1080 | Full RT | 60-110 fps |

---

## Dependency Decisions

External libraries are wrapped behind clean interfaces:
- ECS: EnTT (sparse sets, cache coherent SoA layout).
- Physics: Jolt Physics (rigid bodies, character virtuals, constraints, broadphase queries).
- Editor GUI: Dear ImGui (docking branch), ImGuizmo (transform manipulation).
- UI Motion: ImAnim (tweening, easing curves, color space blending).
- Assets & Models: Assimp, stb_image, stb_image_write.
- Windowing: SDL2 (HID actions, display backend, surface creation).
- Math: GLM (vectors, quaternions, matrix operations).
- BVH: bvh v2 (SIMD binned SAH tree construction).

All dependencies are fetched automatically via CMake (`cmake/Dependencies.cmake`).

---

## Engine Track

| Milestone | Area | Status |
| --- | --- | --- |
| M7 | Scene extraction from shaders | Done |
| M7b | JSON scene serialization | Done |
| M10 | ECS on EnTT | Done |
| M20 | Project structure and relative asset resolution | Done |
| M21 | Modular Editor Shell (Inspector, Hierarchy, Viewport, Console, Textures) | Done |
| M22 | Play Mode (ECS World snapshot and restore, Jolt physics active simulation) | Done |
| M26 | Blender-Style Navigation and Mesh Editing (Vertex, Edge, Face, Extrude, Inset) | Done |
| M17 | Lua 5.4 Scripting & Gameplay Systems | In progress |
| M27 | Repeaters & Parametric Modifiers (Array, Curves, Radial, Mirror) | Scheduled |
| M28 | 2D UI/UX In-Game Visual Editor (Canvas, Anchors, Flexbox, Widgets, Theming) | Scheduled |
| M23 | Standalone Build and Export Pipeline | Next |
| M13 | Vulkan Render Backend | Next |
| M24 | Procedural Animation & Active Ragdoll System (LPAS) | Scheduled |

---

## Milestone Breakdown

### M7 - Decouple Scenes from Shaders (Done)
- `RenderScene` decoupled from concrete shader sources.
- Built-in scene generation in `SceneLibrary`.
- Unified `IRenderBackend::SubmitScene`.

### M7b - JSON Scene Asset Pipeline (Done)
- Human-readable JSON scene serialization via `SceneSerializer`.
- Project-relative asset referencing.
- Scene export and import via `--export-scene` and `--scene`.

### M10 - Entity Component System (Done)
- EnTT integration wrapped in `lucida::Registry` and `lucida::Entity`.
- Core transform components: `LocalTransform`, `WorldTransform`, `Parent`, `Visibility`.
- Hierarchical transform updates via `UpdateWorldTransforms`.

### M20 - Project System (Done)
- Portable directory-based projects (`project.json`, `scenes/`, `assets/`, `scripts/`).
- Relative asset resolving and recent project tracking.

### M21 - Modular Editor Architecture (Done)
- Dockable ImGui layout with responsive viewport rendering.
- Decomposed modules: `EditorViewport`, `EditorInspector`, `EditorHierarchy`, `EditorConsole`, `EditorTextureBrowser`, `EditorMeshModeling`.
- ImGuizmo integration with local/world coordinate spaces and grid snapping.
- Command-based Undo / Redo architecture (`CommandStack`, `EntitySnapshot`).
- Real-time performance profiling and statistics HUD.

### M22 - Play Mode & Simulation Isolation (Done)
- Non-destructive ECS state snapshots on Play.
- Full state restoration on Stop (transforms, velocities, hierarchies).
- Jolt physics step controls: Play, Pause, Single-frame step, Time-scale adjustment.

### M26 - Mesh Modeling, Multi-Selection & Navigation (Done)
- Blender-style 3D Orientation Gizmo with orbit and axis snapping.
- Viewport navigation: Shift+MMB pan, MMB orbit, RMB fly with cursor capture.
- Multi-selection: Select All (`A`), Deselect All (`Alt+A`), Shift-click multi-selection.
- Viewport 2D Selection Tools: Point Picking, Marquee Box Selection, Freehand Lasso Polygon Selection.
- Object Grouping (`Ctrl+G`) / Ungrouping (`Ctrl+Alt+G`) and Mesh Joining (`Ctrl+J`).
- Interactive mesh editor: Vertex, Edge, and Face selection with Extrude, Inset, Subdivide, and Normal recalculation.
- Localization architecture (Eng, UA, Rus, De, Fr, Es).

### M27 - Repeaters & Parametric Modifiers (Scheduled)
- Parametric object replication and procedural geometric distribution:
  - Linear & 3D Grid Array (Масив / Array): uniform offset, count, per-instance scale/rotation jitter.
  - Curve & Spline Distribution (Криві / Curves): follow Bezier/Catmull-Rom paths with tangent alignment.
  - Radial / Polar Ring Array (Радіально / Radial): circular repetition around custom pivot with angular step.
  - Mirror & Symmetry (Віддзеркалення / Mirror): reflection across X/Y/Z planes with optional vertex welding.

### M28 - 2D In-Game UI/UX Visual Editor (Scheduled)
- Visual 2D Canvas Editor for in-game HUDs, Menus, Dialogs, and Inventories:
  - WYSIWYG 2D canvas editing with drag-and-drop hierarchy.
  - Responsive layout engine: 9-point anchoring, stretch margins, aspect ratio fitter, Flexbox and Grid auto-layout.
  - Core UI widgets: Canvas, Panel, Image (9-slice / Sprite), Text/Label (SDF fonts), Button, Slider, InputField, ScrollView, ProgressBar, Toggle.
  - Interactive UI state machine: Normal, Hovered, Pressed, Selected, Disabled visual state transitions.
  - UI animation timeline: keyframe and tween-driven interface transitions and screen fades.

### M17 - Scripting Layer (In Progress)
- Lua 5.4 binding with `sol2`.
- Component reflection and gameplay lifecycle callbacks (`OnStart`, `OnUpdate`, `OnCollide`).
- Sandboxed execution environment.

### M23 - Standalone Export Pipeline
- Headless / standalone player build target.
- Asset bundle packing into single-file container.
- Platform distribution packages.

### M13 - Vulkan Compute Backend
- `backends/render_vulkan` implementation of `IRenderBackend`.
- VMA device memory allocation.
- `VK_KHR_ray_query` compute pipeline for Linux and Windows.

### M24 - Procedural Animation & Active Ragdoll System (LPAS)
- Articulated multi-body physical ragdolls driven by Jolt constraints.
- Dynamic balance control and center-of-mass stabilization.
- Procedural stumble, fall bracing, and step adjustments.
