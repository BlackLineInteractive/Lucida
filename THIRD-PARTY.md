# Third-party components

Lucida Engine is licensed under the **GNU General Public License v3.0 or later**
(see [LICENSE](LICENSE)). Nothing third-party is vendored in this repository — every
dependency below is fetched at configure time by `cmake/Dependencies.cmake` and keeps
its own licence.

## In use today

| Component | Licence | Compatible with GPL-3.0 | Role |
|---|---|---|---|
| [glm](https://github.com/g-truc/glm) | MIT (Happy Bunny variant) | yes | vector and matrix maths |
| [bvh v2](https://github.com/madmann91/bvh) | MIT | yes | multithreaded binned-SAH BVH builder |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | MIT | yes | rigid bodies, vehicle simulation |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT | yes | editor and debug UI |
| [ImAnim](https://github.com/soufianekhiat/ImAnim) | MIT | yes | UI tweening and easing |
| [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) | MIT | yes | file picker (to be replaced, roadmap M15) |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | MIT or public domain | yes | image import and export |
| [SDL2](https://github.com/libsdl-org/SDL) | Zlib | yes | window, input, surface |
| [Assimp](https://github.com/assimp/assimp) | BSD-3-Clause | yes | model import |

Every licence above is permissive, so the combined work may be distributed under the
GPL-3.0. The permissive notices must still be preserved in binary distributions — the
table above plus the upstream licence files satisfy that.

## Planned (roadmap)

| Component | Licence | Note |
|---|---|---|
| EnTT | MIT | ECS |
| enkiTS | Zlib | job system |
| slang | Apache-2.0 | Apache-2.0 is one-way compatible with GPL-3.0, which is the direction we need |
| Vulkan Memory Allocator | MIT | device memory |
| KTX-Software / Basis Universal | Apache-2.0 | texture transcoding |
| FreeType | FTL or GPL-2.0+ | **choose FTL**; the GPL-2.0-only option would clash with GPL-3.0 |
| msdf-atlas-gen | MIT | SDF font atlases |
| miniaudio | MIT-0 or public domain | audio |
| Lua | MIT | scripting |
| sol2 | MIT | Lua bindings |
| Tracy | BSD-3-Clause | profiler |
| nlohmann/json | MIT | scenes, config, saves |
| Bullet | Zlib | alternative physics backend |

Two licences on that list need care when the time comes:

* **FreeType** is dual-licensed FTL / GPL-2.0-or-later. Take the FTL option — GPL-2.0
  *only* would be incompatible with GPL-3.0.
* **Apache-2.0** components (slang, KTX) may be combined into a GPL-3.0 work, but not
  the other way round. That is the direction Lucida needs, so it is fine.

## Assets

The demo scenes are generated procedurally in code and carry no third-party assets. Any
model loaded through `--mesh` remains under its own licence and is not part of this
repository (`*.glb`, `*.gltf`, `*.fbx` and `*.obj` are gitignored).
