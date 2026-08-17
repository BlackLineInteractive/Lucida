// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
// Radiance Cascades 3D Render Backend

#include "lucida/backend/RadianceCascadesBackend.h"
#include "lucida/core/diag/Log.h"
#include "lucida/core/platform/Time.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "backends/imgui_impl_metal.h"
#include "imgui.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <fstream>
#include <mach-o/dyld.h>
#include <sstream>
#include <string>
#include <vector>

namespace lucida {
namespace {

static std::string ExecutableDir() {
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    std::string path(buf);
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
}

static std::string ReadShader(const std::string& rel_path) {
    const std::string base = ExecutableDir();
    const std::vector<std::string> prefixes = {
        base,
        base + "shaders/",
        base + "../",
        base + "../shaders/",
        base + "../../",
        base + "../../shaders/",
        base + "../../../",
        base + "../../../shaders/",
        base + "../../../backends/render_radiance_cascades/shaders/",
        base + "../Resources/",
        base + "../Resources/shaders/",
        "shaders/",
        "backends/render_radiance_cascades/shaders/",
        "../backends/render_radiance_cascades/shaders/",
        ""
    };
    for (const auto& prefix : prefixes) {
        std::ifstream f(prefix + rel_path);
        if (f.is_open()) {
            std::stringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }
    }
    LUCIDA_ERROR(Render, "[Radiance Cascades] shader not found: %s", rel_path.c_str());
    return "";
}

// Exactly 208 bytes matching rc_shader.metal RCUniforms
struct alignas(16) RCUniforms {
    float screen_width;
    float screen_height;
    float viewport_width;
    float viewport_height;

    float camera_pos[3];
    float time;

    float cam_forward[3];
    float fov_y;

    float cam_right[3];
    float aspect;

    float cam_up[3];
    float tan_half_fov;

    float sun_dir[3];
    float sun_intensity;

    float sun_color[3];
    float sky_intensity;

    float sky_zenith[3];
    float pad0;

    float sky_horizon[3];
    float pad1;

    float sky_ground[3];
    float pad2;

    float ambient[3];
    float pad3;

    int   cascade_count;
    int   num_spheres;
    int   num_planes;
    int   num_cubes;

    int   num_lights;
    int   num_materials;
    int   frame_index;
    int   pad4;
};
static_assert(sizeof(RCUniforms) == 208, "RCUniforms size must be exactly 208 bytes to match Metal struct layout");

class RadianceCascadesBackend final : public IRenderBackend {
public:
    RadianceCascadesBackend() = default;
    ~RadianceCascadesBackend() override { Shutdown(); }

    bool Init(const SurfaceDesc& surface) override {
        LUCIDA_INFO(Render, "initialising Radiance Cascades 3D backend");

        m_device = MTLCreateSystemDefaultDevice();
        if (!m_device) {
            LUCIDA_ERROR(Render, "[Radiance Cascades] no Metal device found");
            return false;
        }

        m_queue = [m_device newCommandQueue];
        m_layer = (__bridge CAMetalLayer*)surface.native_layer;
        if (m_layer) {
            m_layer.device = m_device;
            m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
            m_layer.framebufferOnly = NO;
        }

        m_width  = surface.width;
        m_height = surface.height;

        if (!CompilePipelines()) {
            LUCIDA_ERROR(Render, "[Radiance Cascades] failed to compile Metal compute pipelines");
            return false;
        }

        AllocTargets(TargetWidth(), TargetHeight());

        m_buf_uniforms   = [m_device newBufferWithLength:sizeof(RCUniforms) options:MTLResourceStorageModeShared];
        m_buf_spheres    = [m_device newBufferWithLength:sizeof(GPUSphere) * 512 options:MTLResourceStorageModeShared];
        m_buf_planes     = [m_device newBufferWithLength:sizeof(GPUPlane) * 128 options:MTLResourceStorageModeShared];
        m_buf_cubes      = [m_device newBufferWithLength:sizeof(GPUCube) * 512 options:MTLResourceStorageModeShared];
        m_buf_materials  = [m_device newBufferWithLength:sizeof(GPUMaterial) * 512 options:MTLResourceStorageModeShared];
        m_buf_lights     = [m_device newBufferWithLength:sizeof(GPULight) * 128 options:MTLResourceStorageModeShared];

        LUCIDA_INFO(Render, "[Radiance Cascades] backend initialised on %s", [m_device.name UTF8String]);
        return true;
    }

    void Shutdown() override {
        if (!m_device) return;
        m_pipeline_present = nil;
        m_tex_present = nil;
        m_buf_uniforms = nil;
        m_buf_spheres = nil;
        m_buf_planes = nil;
        m_buf_cubes = nil;
        m_buf_materials = nil;
        m_buf_lights = nil;
        m_drawable = nil;
        m_rpdesc = nil;
        m_queue = nil;
        m_device = nil;
    }

    void Resize(i32 width, i32 height) override {
        if (width <= 0 || height <= 0) return;
        if (m_width == width && m_height == height) return;
        m_width = width;
        m_height = height;
        AllocTargets(TargetWidth(), TargetHeight());
    }

    void SetViewportAsPanel(bool enabled) override {
        m_viewport_as_panel = enabled;
    }

    void SetViewportSize(i32 width, i32 height) override {
        m_viewport_width  = width;
        m_viewport_height = height;
        if (m_viewport_as_panel && (width != m_target_w || height != m_target_h)) {
            AllocTargets(TargetWidth(), TargetHeight());
        }
    }

    void* ViewportTexture() const override {
        return (__bridge void*)m_tex_present;
    }

    bool ReadbackFrame(std::vector<u8>& rgba, i32& width, i32& height) override {
        if (!m_tex_present || !m_device || !m_queue) return false;
        width = m_target_w;
        height = m_target_h;

        const size_t row_bytes = size_t(width) * 4;
        const size_t bytes = row_bytes * size_t(height);
        id<MTLBuffer> staging = [m_device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        if (!staging) return false;

        id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
        [blit copyFromTexture:m_tex_present
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(width, height, 1)
                     toBuffer:staging
            destinationOffset:0
       destinationBytesPerRow:row_bytes
     destinationBytesPerImage:bytes];
        [blit endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];

        rgba.resize(bytes);
        const uint8_t* src = static_cast<const uint8_t*>([staging contents]);
        for (size_t i = 0; i < bytes; i += 4) {
            rgba[i + 0] = src[i + 0]; // RGBA8 format
            rgba[i + 1] = src[i + 1];
            rgba[i + 2] = src[i + 2];
            rgba[i + 3] = src[i + 3];
        }
        return true;
    }

    void SetCamera(const CameraState& camera) override {
        m_camera = camera;
    }

    void ApplySettings(const RenderSettings& settings) override {
        const bool scale_changed = std::abs(m_settings.render_scale - settings.render_scale) > 0.001f;
        m_settings = settings;
        if (scale_changed) {
            AllocTargets(TargetWidth(), TargetHeight());
        }
    }

    RenderSettings Settings() const override { return m_settings; }
    RenderStats Stats() const override { return m_stats; }

    MeshHandle AddMesh(const MeshData& mesh) override {
        (void)mesh;
        return MeshHandle{0};
    }
    InstanceHandle AddInstance(MeshHandle mesh, const Mat4& local_to_world) override {
        (void)mesh; (void)local_to_world;
        return InstanceHandle{0};
    }
    void SetInstanceTransform(InstanceHandle instance, const Mat4& local_to_world) override {
        (void)instance; (void)local_to_world;
    }
    void ClearInstances() override {}
    void ClearMeshes() override {}
    void SetMeshOrigin(const Vec3& origin) override { (void)origin; }

    void SubmitScene(const RenderScene& scene) override {
        m_scene = scene;

        // Upload spheres
        const size_t num_s = std::min(scene.spheres.size(), size_t(512));
        if (num_s > 0 && m_buf_spheres) {
            memcpy(m_buf_spheres.contents, scene.spheres.data(), num_s * sizeof(GPUSphere));
        }

        // Upload planes
        const size_t num_p = std::min(scene.planes.size(), size_t(128));
        if (num_p > 0 && m_buf_planes) {
            memcpy(m_buf_planes.contents, scene.planes.data(), num_p * sizeof(GPUPlane));
        }

        // Upload cubes
        const size_t num_c = std::min(scene.cubes.size(), size_t(512));
        if (num_c > 0 && m_buf_cubes) {
            memcpy(m_buf_cubes.contents, scene.cubes.data(), num_c * sizeof(GPUCube));
        }

        // Upload materials
        const size_t num_m = std::min(scene.materials.size(), size_t(512));
        if (num_m > 0 && m_buf_materials) {
            memcpy(m_buf_materials.contents, scene.materials.data(), num_m * sizeof(GPUMaterial));
        }

        // Upload lights
        const size_t num_l = std::min(scene.lights.size(), size_t(128));
        if (num_l > 0 && m_buf_lights) {
            memcpy(m_buf_lights.contents, scene.lights.data(), num_l * sizeof(GPULight));
        }
    }

    void Render(const FrameTime& time) override {
        if (!m_device || !m_pipeline_present) return;

        @autoreleasepool {
            const u32 tw = TargetWidth();
            const u32 th = TargetHeight();
            if (tw == 0 || th == 0) return;

            AllocTargets(tw, th);
            if (!m_tex_present) return;

            // Fill uniform buffer
            RCUniforms u{};
            u.screen_width    = float(m_width);
            u.screen_height   = float(m_height);
            u.viewport_width  = float(tw);
            u.viewport_height = float(th);

            u.camera_pos[0] = m_camera.position.x;
            u.camera_pos[1] = m_camera.position.y;
            u.camera_pos[2] = m_camera.position.z;
            u.time = time.elapsed;

            // Forward, Right, Up vectors consistent with Lucida's coordinate system
            const Vec3 fwd(std::cos(m_camera.yaw) * std::cos(m_camera.pitch),
                           std::sin(m_camera.pitch),
                           std::sin(m_camera.yaw) * std::cos(m_camera.pitch));
            const Vec3 rgt = glm::normalize(glm::cross(fwd, Vec3(0, 1, 0)));
            const Vec3 up  = glm::normalize(glm::cross(rgt, fwd));

            u.cam_forward[0] = fwd.x; u.cam_forward[1] = fwd.y; u.cam_forward[2] = fwd.z;
            u.fov_y          = m_camera.fov_y > 0.01f ? m_camera.fov_y : glm::radians(60.0f);
            u.cam_right[0]   = rgt.x; u.cam_right[1]   = rgt.y; u.cam_right[2]   = rgt.z;
            u.aspect         = (th > 0) ? (float(tw) / float(th)) : (16.0f / 9.0f);
            u.cam_up[0]      = up.x;  u.cam_up[1]      = up.y;  u.cam_up[2]      = up.z;
            u.tan_half_fov   = std::tan(u.fov_y * 0.5f);

            // Sun from environment or default
            Vec3 sun_dir = glm::normalize(Vec3(0.5f, 1.0f, 0.4f));
            u.sun_dir[0] = sun_dir.x; u.sun_dir[1] = sun_dir.y; u.sun_dir[2] = sun_dir.z;
            u.sun_intensity = 2.2f;
            u.sun_color[0] = 1.0f; u.sun_color[1] = 0.95f; u.sun_color[2] = 0.88f;
            u.sky_intensity = 1.0f;

            const auto& env = m_scene.environment;
            u.sky_zenith[0]  = env.sky_zenith.x;  u.sky_zenith[1]  = env.sky_zenith.y;  u.sky_zenith[2]  = env.sky_zenith.z;
            u.sky_horizon[0] = env.sky_horizon.x; u.sky_horizon[1] = env.sky_horizon.y; u.sky_horizon[2] = env.sky_horizon.z;
            u.sky_ground[0]  = env.sky_ground.x;  u.sky_ground[1]  = env.sky_ground.y;  u.sky_ground[2]  = env.sky_ground.z;
            u.ambient[0]     = env.ambient.x;     u.ambient[1]     = env.ambient.y;     u.ambient[2]     = env.ambient.z;

            u.cascade_count = 4;
            u.num_spheres   = static_cast<int>(m_scene.spheres.size());
            u.num_planes    = static_cast<int>(m_scene.planes.size());
            u.num_cubes     = static_cast<int>(m_scene.cubes.size());
            u.num_lights    = static_cast<int>(m_scene.lights.size());
            u.num_materials = static_cast<int>(m_scene.materials.size());
            u.frame_index   = m_frame_index++;

            memcpy(m_buf_uniforms.contents, &u, sizeof(RCUniforms));

            id<MTLCommandBuffer> cmd = [m_queue commandBuffer];

            // 1. Dispatch Radiance Cascades compute pass
            id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
            [enc setComputePipelineState:m_pipeline_present];
            [enc setTexture:m_tex_present atIndex:0];
            [enc setBuffer:m_buf_uniforms offset:0 atIndex:0];
            [enc setBuffer:m_buf_spheres offset:0 atIndex:1];
            [enc setBuffer:m_buf_planes offset:0 atIndex:2];
            [enc setBuffer:m_buf_cubes offset:0 atIndex:3];
            [enc setBuffer:m_buf_materials offset:0 atIndex:4];
            [enc setBuffer:m_buf_lights offset:0 atIndex:5];

            MTLSize group = MTLSizeMake(16, 16, 1);
            MTLSize grid = MTLSizeMake((tw + 15) / 16, (th + 15) / 16, 1);
            [enc dispatchThreadgroups:grid threadsPerThreadgroup:group];
            [enc endEncoding];

            // 2. Render ImGui Overlay onto drawable
            if (m_drawable) {
                id<MTLRenderCommandEncoder> render_enc = [cmd renderCommandEncoderWithDescriptor:m_rpdesc];
                ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cmd, render_enc);
                [render_enc endEncoding];

                [cmd presentDrawable:m_drawable];
                m_drawable = nil;
            }

            [cmd commit];
        }
    }

    // --- IOverlayHost --------------------------------------------------------
    void OverlayInit() override {
        ImGui_ImplMetal_Init(m_device);
    }
    void OverlayNewFrame() override {
        m_drawable = [m_layer nextDrawable];
        if (!m_drawable) return;

        if (!m_rpdesc) {
            m_rpdesc = [MTLRenderPassDescriptor renderPassDescriptor];
        }
        m_rpdesc.colorAttachments[0].texture = m_drawable.texture;
        m_rpdesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        m_rpdesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        m_rpdesc.colorAttachments[0].clearColor = MTLClearColorMake(0.05, 0.06, 0.08, 1.0);
        ImGui_ImplMetal_NewFrame(m_rpdesc);
    }
    void OverlayShutdown() override {
        ImGui_ImplMetal_Shutdown();
    }

private:
    u32 TargetWidth() const {
        const u32 base = (m_viewport_as_panel && m_viewport_width > 0) ? m_viewport_width : m_width;
        return std::max(1u, u32(base * std::clamp(m_settings.render_scale, 0.25f, 1.0f)));
    }
    u32 TargetHeight() const {
        const u32 base = (m_viewport_as_panel && m_viewport_height > 0) ? m_viewport_height : m_height;
        return std::max(1u, u32(base * std::clamp(m_settings.render_scale, 0.25f, 1.0f)));
    }

    void AllocTargets(u32 w, u32 h) {
        if (w == 0 || h == 0 || !m_device) return;
        if (m_tex_present && m_target_w == w && m_target_h == h) return;

        m_target_w = w;
        m_target_h = h;

        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                        width:w
                                                                                       height:h
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModePrivate;
        m_tex_present = [m_device newTextureWithDescriptor:desc];
    }

    bool CompilePipelines() {
        const std::string src = ReadShader("rc_shader.metal");
        if (src.empty()) return false;

        NSError* err = nil;
        MTLCompileOptions* opts = [MTLCompileOptions new];
        opts.languageVersion = MTLLanguageVersion2_4;

        id<MTLLibrary> lib = [m_device newLibraryWithSource:[NSString stringWithUTF8String:src.c_str()]
                                                    options:opts
                                                      error:&err];
        if (!lib) {
            LUCIDA_ERROR(Render, "[Radiance Cascades] shader compile error: %s",
                         err ? [err.localizedDescription UTF8String] : "unknown");
            return false;
        }

        id<MTLFunction> fn_present = [lib newFunctionWithName:@"rc_present_kernel"];
        if (!fn_present) {
            LUCIDA_ERROR(Render, "[Radiance Cascades] function rc_present_kernel not found");
            return false;
        }

        m_pipeline_present = [m_device newComputePipelineStateWithFunction:fn_present error:&err];
        return m_pipeline_present != nil;
    }

    id<MTLDevice> m_device = nil;
    id<MTLCommandQueue> m_queue = nil;
    CAMetalLayer* m_layer = nil;
    id<CAMetalDrawable> m_drawable = nil;
    MTLRenderPassDescriptor* m_rpdesc = nil;

    id<MTLComputePipelineState> m_pipeline_present = nil;
    id<MTLTexture> m_tex_present = nil;

    id<MTLBuffer> m_buf_uniforms = nil;
    id<MTLBuffer> m_buf_spheres = nil;
    id<MTLBuffer> m_buf_planes = nil;
    id<MTLBuffer> m_buf_cubes = nil;
    id<MTLBuffer> m_buf_materials = nil;
    id<MTLBuffer> m_buf_lights = nil;

    CameraState m_camera{};
    RenderSettings m_settings{};
    RenderStats m_stats{};
    RenderScene m_scene{};

    i32 m_width = 1280;
    i32 m_height = 720;
    i32 m_viewport_width = 0;
    i32 m_viewport_height = 0;
    u32 m_target_w = 0;
    u32 m_target_h = 0;
    bool m_viewport_as_panel = false;
    int m_frame_index = 0;
};

} // namespace

std::unique_ptr<IRenderBackend> CreateRadianceCascadesBackend() {
    return std::make_unique<RadianceCascadesBackend>();
}

} // namespace lucida
