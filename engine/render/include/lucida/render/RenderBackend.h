// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Render backend interface (GEA 1.6.11, low-level renderer boundary).
//
// No SDL, no Metal, no Vulkan types cross this header. The surface arrives as
// an opaque native pointer produced by the platform module, which is what lets
// Metal, Vulkan and GL implementations coexist without the engine knowing.

#include "lucida/core/container/Handle.h"
#include "lucida/core/platform/Time.h"
#include "lucida/render/Camera.h"
#include "lucida/render/MeshData.h"
#include "lucida/render/Scene.h"

#include <cstdint>
#include <vector>

namespace lucida {

LUCIDA_DECLARE_HANDLE(MeshHandle);
LUCIDA_DECLARE_HANDLE(InstanceHandle);

// native_layer is CAMetalLayer* on macOS, and whatever the platform module
// hands over elsewhere. The backend casts; nobody else touches it.
struct SurfaceDesc {
    void* native_layer = nullptr;
    void* native_window = nullptr;
    i32   width  = 0;
    i32   height = 0;
};

struct RenderSettings {
    i32  samples      = 1;
    i32  max_depth    = 5;
    i32  debug_mode   = 0;
    f32  render_scale = 1.0f;   // 1.0 native, 0.5 quarter the pixels
    bool fog          = false;
    bool vsync        = true;
};

struct RenderStats {
    f32 cpu_frame_ms = 0.0f;
    f32 gpu_frame_ms = 0.0f;
    i32 ray_count    = 0;
    i32 tri_count    = 0;
};

// Immediate-mode overlay hook. The UI module owns the widgets; only the two
// calls that need the graphics API live behind this interface.
class IOverlayHost {
public:
    virtual ~IOverlayHost() = default;

    // Called after the UI module has created the ImGui context, never before:
    // the graphics backend writes into that context.
    virtual void OverlayInit() = 0;
    virtual void OverlayNewFrame() = 0;
    virtual void OverlayShutdown() = 0;
};

class IRenderBackend : public IOverlayHost {
public:
    ~IRenderBackend() override = default;

    virtual bool Init(const SurfaceDesc& surface) = 0;
    virtual void Shutdown() = 0;
    virtual void Resize(i32 width, i32 height) = 0;

    virtual void Render(const FrameTime& time) = 0;

    virtual void SetCamera(const CameraState& camera) = 0;
    virtual void ApplySettings(const RenderSettings& settings) = 0;
    virtual RenderSettings Settings() const = 0;
    virtual RenderStats    Stats() const = 0;

    // Two-level acceleration structure. AddMesh uploads a BLAS once; AddInstance
    // places it; SetInstanceTransform moves it per frame without touching a BVH.
    virtual MeshHandle     AddMesh(const MeshData& mesh) = 0;
    virtual InstanceHandle AddInstance(MeshHandle mesh, const Mat4& local_to_world) = 0;
    virtual void SetInstanceTransform(InstanceHandle instance, const Mat4& local_to_world) = 0;
    virtual void ClearInstances() = 0;
    virtual void ClearMeshes() = 0;

    // Replaces whatever scene the backend held. Analytic geometry, materials,
    // lights and environment arrive together; meshes come separately through
    // AddMesh/AddInstance because they are uploaded once and reused.
    virtual void SubmitScene(const RenderScene& scene) = 0;
    virtual void SetMeshOrigin(const Vec3& origin) = 0;

    // Last traced frame as 8-bit RGBA, for screenshots and headless checks.
    virtual bool ReadbackFrame(std::vector<u8>& rgba, i32& width, i32& height) = 0;
};

} // namespace lucida
