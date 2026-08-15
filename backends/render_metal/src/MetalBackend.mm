// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
// Metal ray tracing backend. Ported from RayTracer_Unified; the tracing,
// fog and upscale paths are unchanged. What was cut is everything that did not
// belong in a renderer: the SDL window, the keyboard, and the player
// controller now live in the platform, input and app layers.

#include "lucida/render/RenderBackend.h"
#include "lucida/backend/MetalBackend.h"

#include "lucida/core/diag/Log.h"
#include "lucida/core/platform/Time.h"

#include <glm/gtc/matrix_transform.hpp>
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
#include <map>
#include <sstream>
#include <string>

// MetalFX only available on macOS 13+
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
#import <MetalFX/MetalFX.h>
#endif

namespace lucida {
namespace {

// ---------------------------------------------------------- shader loader ---

// Directory of the running executable. Replaces SDL_GetBasePath so the backend
// carries no dependency on the windowing library.
static std::string ExecutableDir() {
  char buf[4096];
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) != 0) return {};
  std::string path(buf);
  const auto slash = path.find_last_of('/');
  return slash == std::string::npos ? std::string{} : path.substr(0, slash + 1);
}

static std::string ReadShader(const std::string &rel_path) {
  const std::string base = ExecutableDir();
  for (const auto &prefix : {base, base + "../", std::string("")}) {
    std::ifstream f(prefix + rel_path);
    if (f.is_open()) {
      std::stringstream ss;
      ss << f.rdbuf();
      return ss.str();
    }
  }
  LUCIDA_ERROR(Render, "shader not found: %s", rel_path.c_str());
  return "";
}

// --------------------------------------------------------- Metal Renderer ---

// Fog marches at this fraction of the ray resolution. The medium is
// low-frequency and the composite upsamples with a depth-aware filter, so half
// resolution is visually indistinguishable at a quarter of the traversal cost.
static constexpr float kFogScale = 0.5f;

// IEEE 754 half -> float, for reading RGBA16Float render targets back.
static float half_to_float(uint16_t h) {
  uint32_t sign = (h >> 15) & 0x1;
  uint32_t exp = (h >> 10) & 0x1f;
  uint32_t mant = h & 0x3ff;
  uint32_t f;
  if (exp == 0) {
    if (mant == 0)
      f = sign << 31;
    else { // subnormal: renormalise
      exp = 127 - 15 + 1;
      while (!(mant & 0x400)) {
        mant <<= 1;
        exp--;
      }
      mant &= 0x3ff;
      f = (sign << 31) | (exp << 23) | (mant << 13);
    }
  } else if (exp == 0x1f) {
    f = (sign << 31) | (0xff << 23) | (mant << 13);
  } else {
    f = (sign << 31) | ((exp - 15 + 127) << 23) | (mant << 13);
  }
  float out;
  memcpy(&out, &f, sizeof(out));
  return out;
}

// Radical-inverse Halton sample, used for the temporal sub-pixel jitter.
static float halton(int index, int base) {
  float f = 1.0f, r = 0.0f;
  while (index > 0) {
    f /= float(base);
    r += f * float(index % base);
    index /= base;
  }
  return r;
}

// Builds the projection the ray generator implies, so world positions can be
// reprojected into the previous frame. Rows are laid out to match:
//   clip.x = dot(P-cam, right) / (aspect*tanHalf)
//   clip.y = dot(P-cam, up)    / tanHalf
//   clip.w = dot(P-cam, forward)
static glm::mat4 MakeViewProj(const Vec3 &cam, const Vec3 &fwd,
                              const Vec3 &right, const Vec3 &up, float aspect,
                              float tan_half) {
  float sx = aspect * tan_half, sy = tan_half;
  glm::mat4 m(0.0f);
  // glm is column-major: m[col][row].
  m[0][0] = right.x / sx;
  m[1][0] = right.y / sx;
  m[2][0] = right.z / sx;
  m[3][0] = -glm::dot(right, cam) / sx;
  m[0][1] = up.x / sy;
  m[1][1] = up.y / sy;
  m[2][1] = up.z / sy;
  m[3][1] = -glm::dot(up, cam) / sy;
  m[0][3] = fwd.x;
  m[1][3] = fwd.y;
  m[2][3] = fwd.z;
  m[3][3] = -glm::dot(fwd, cam);
  return m;
}

class MetalBackend final : public IRenderBackend {
  // Surface. The layer is created by the platform module and handed over as an
  // opaque pointer; this class never sees the window it belongs to.
  CAMetalLayer *m_layer = nil;
  id<MTLDevice> m_device = nil;
  id<MTLCommandQueue> m_queue = nil;

  // Pipeline states (one per demo version)
  id<MTLComputePipelineState> m_pipeline02 = nil;
  id<MTLComputePipelineState> m_pipeline03 = nil;
  id<MTLComputePipelineState> m_pipeline_fog = nil;

  // Scene GPU buffers (primitives)
  id<MTLBuffer> m_buf_mats = nil;
  id<MTLBuffer> m_buf_spheres = nil;
  id<MTLBuffer> m_buf_planes = nil;
  id<MTLBuffer> m_buf_cubes = nil;
  id<MTLBuffer> m_buf_lights = nil;

  // Triple-buffered uniforms to avoid CPU/GPU races on in-flight frames
  static constexpr int kMaxFramesInFlight = 3;
  id<MTLBuffer> m_buf_uniforms[kMaxFramesInFlight] = {nil, nil, nil};
  int m_frame_index = 0;
  dispatch_semaphore_t m_frame_sema = nil;

  // Mesh GPU buffers
  id<MTLBuffer> m_buf_triangles = nil;   // GPUTriPos: traversal only
  id<MTLBuffer> m_buf_tri_attr = nil;    // GPUTriAttr: read on a confirmed hit
  id<MTLBuffer> m_buf_bvh = nil;
  id<MTLBuffer> m_buf_mesh_mats = nil;

  // ImGui render pass
  MTLRenderPassDescriptor *m_rpdesc = nil;
  id<CAMetalDrawable> m_drawable = nil;

  // Render targets for Multi-pass & MetalFX
  id<MTLTexture> m_tex_gbuffer = nil; // RGBA16F (ray-res color)
  id<MTLTexture> m_tex_depth = nil;   // R32F  (ray-res depth)
  id<MTLTexture> m_tex_motion = nil;  // RG16F (ray-res motion)
  id<MTLTexture> m_tex_fog = nil; // RGBA16F (fog-res inscatter + transmittance)
  id<MTLTexture> m_tex_fog_depth = nil;
  id<MTLTexture> m_tex_ao = nil;      // RGBA16F at fog res: (indirect.rgb, ao)
  id<MTLTexture> m_tex_gi_norm = nil; // RGBA16F at fog res: shading normal
  id<MTLTexture> m_tex_mesh_arrays = nil;
  id<MTLTexture> m_tex_mesh_orm = nil;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
  id<MTLFXTemporalScaler> m_temporal_scaler = nil;
#endif

  // State
  ShadingModel m_model = ShadingModel::WhittedGI;
  SceneEnvironment m_environment;
  bool m_fog = true;
  bool m_jitter = false;
  int m_samples = 1;
  bool m_mesh_loaded = false;
  float m_render_scale = 0.5f;
  bool m_vsync = true;
  int m_render_w = 0; // drawable (output) size
  int m_render_h = 0;
  int m_ray_w = 0; // internal ray-tracing resolution
  int m_ray_h = 0;
  int m_fog_w = 0; // fog march resolution
  int m_fog_h = 0;
  int m_num_triangles = 0;
  int m_num_bvh_nodes = 0;
  int m_num_mesh_mats = 0;
  int m_mesh_tex_dim = 1;
  int m_orm_tex_dim = 1;

  // MetalFX history is only invalidated on genuine discontinuities (resize,
  // scene swap, mesh load) — never on ordinary camera movement, which is
  // exactly when the accumulation is most valuable.
  bool m_reset_history = true;
  uint64_t m_frame_counter = 0;
  glm::mat4 m_prev_view_proj = glm::mat4(1.0f);
  bool m_have_prev_vp = false;

  GPUUniforms m_uniforms = {};
  Vec3 m_cam_pos = {0, 0.0, 2.0};
  double m_yaw = -M_PI / 2.0;
  double m_pitch = 0.0;

  // ---- Two-level acceleration structure.
  // Every mesh's BLAS is appended to shared CPU-side pools and re-uploaded when
  // the set changes; instances only carry a transform, so moving one costs a
  // 144-byte write rather than a BVH rebuild.
  struct MeshSlot {
    int node_base = 0, node_count = 0;
    int tri_base = 0, tri_count = 0;
    int mat_base = 0, mat_count = 0;
    Vec3 aabb_min{0.0f}, aabb_max{0.0f};   // local space
  };
  std::vector<MeshSlot>    m_mesh_slots;
  std::vector<GPUBVHNode>  m_pool_nodes;
  std::vector<GPUTriPos>   m_pool_tripos;
  std::vector<GPUTriAttr>  m_pool_triattr;
  std::vector<GPUMaterial> m_pool_mats;
  std::vector<GPUInstance> m_instances;
  std::vector<int>         m_instance_mesh;   // which slot each instance uses
  // local->world as of the previously rendered frame. Kept beside the instances
  // rather than inside them because it advances once per *frame*, while
  // SetInstanceTransform may be called any number of times in between.
  std::vector<std::array<float, 12>> m_instance_last_l2w;
  id<MTLBuffer>            m_buf_instances = nil;
  bool                     m_instances_dirty = true;

  // Advances every instance's previous transform by one frame. Runs before the
  // upload, so what the GPU sees is exactly "where this was when it was last
  // drawn" — including the frame an object stops, where prev must catch up to
  // current or the upscaler keeps smearing a stationary object.
  void RollInstanceMotion() {
    for (size_t i = 0; i < m_instances.size(); i++) {
      GPUInstance &gi = m_instances[i];
      float *last = m_instance_last_l2w[i].data();
      constexpr size_t kBytes = sizeof(float) * 12;

      if (memcmp(gi.prev_local_to_world, last, kBytes) != 0) {
        memcpy(gi.prev_local_to_world, last, kBytes);
        m_instances_dirty = true;
      }
      memcpy(last, gi.local_to_world, kBytes);
    }
  }

  // Fills an instance's transforms and world AABB from a local->world matrix.
  void WriteInstance(GPUInstance &gi, const MeshSlot &slot, const glm::mat4 &l2w) {
    glm::mat4 w2l = glm::inverse(l2w);
    for (int r = 0; r < 3; r++)
      for (int c = 0; c < 4; c++) {
        gi.local_to_world[r * 4 + c] = l2w[c][r];   // glm is column-major
        gi.world_to_local[r * 4 + c] = w2l[c][r];
      }
    // World AABB: transform all eight corners of the local box.
    Vec3 mn(1e30f), mx(-1e30f);
    for (int k = 0; k < 8; k++) {
      glm::vec4 c(( k & 1) ? slot.aabb_max.x : slot.aabb_min.x,
                  ((k >> 1) & 1) ? slot.aabb_max.y : slot.aabb_min.y,
                  ((k >> 2) & 1) ? slot.aabb_max.z : slot.aabb_min.z, 1.0f);
      glm::vec4 w = l2w * c;
      mn = glm::min(mn, Vec3(w)); mx = glm::max(mx, Vec3(w));
    }
    // A hair of padding so a ray grazing the surface is never culled by rounding.
    const float eps = 1e-3f;
    SetVec3(gi.aabb_min, mn - Vec3(eps));
    SetVec3(gi.aabb_max, mx + Vec3(eps));
    gi.node_base  = slot.node_base;
    gi.tri_base   = slot.tri_base;
    gi.node_count = slot.node_count;
    gi.mat_base   = slot.mat_base;
    gi.flags = 0; gi.pad0 = 0;
  }

  void UploadPools() {
    auto up = [&](const void *d, size_t bytes) -> id<MTLBuffer> {
      return MakePrivateBuffer(bytes ? d : nullptr, std::max<size_t>(bytes, 16));
    };
    m_buf_bvh       = up(m_pool_nodes.data(),   m_pool_nodes.size()   * sizeof(GPUBVHNode));
    m_buf_triangles = up(m_pool_tripos.data(),  m_pool_tripos.size()  * sizeof(GPUTriPos));
    m_buf_tri_attr  = up(m_pool_triattr.data(), m_pool_triattr.size() * sizeof(GPUTriAttr));
    m_buf_mesh_mats = up(m_pool_mats.data(),    m_pool_mats.size()    * sizeof(GPUMaterial));
  }

  void UploadInstances() {
    m_buf_instances = MakePrivateBuffer(
        m_instances.empty() ? nullptr : m_instances.data(),
        std::max<size_t>(m_instances.size() * sizeof(GPUInstance), sizeof(GPUInstance)));
    m_uniforms.num_instances = int(m_instances.size());
    m_instances_dirty = false;
  }


  int m_total_rays = 0;

  // GPU timing (exponential moving average over completed command buffers)
  std::atomic<double> m_gpu_ms_ema{0.0};
  std::atomic<uint64_t> m_gpu_samples{0};

  // Upload `bytes` of `data` into a GPU-private (VRAM-resident) buffer.
  // On a discrete GPU, StorageModeShared buffers stay in host memory and every
  // read crosses PCIe — fatal for BVH/triangle traversal, which is nothing but
  // scattered reads. Private storage puts them in VRAM.
  id<MTLBuffer> MakePrivateBuffer(const void *data, size_t bytes) {
    bytes = std::max<size_t>(bytes, 16);
    id<MTLBuffer> dst =
        [m_device newBufferWithLength:bytes
                              options:MTLResourceStorageModePrivate];
    if (!data)
      return dst;

    id<MTLBuffer> staging =
        [m_device newBufferWithBytes:data
                              length:bytes
                             options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromBuffer:staging
             sourceOffset:0
                 toBuffer:dst
        destinationOffset:0
                     size:bytes];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];
    return dst;
  }

  // Compiles one shader file into a library, cached so a file holding several
  // kernels (v0.3 has both the tracer and the fog march) is only built once.
  id<MTLLibrary> CompileLibrary(const std::string &path, NSError **err) {
    auto it = m_libraries.find(path);
    if (it != m_libraries.end())
      return it->second;

    std::string src = ReadShader(path);
    if (src.empty())
      return nil;
    NSString *ns_src = [NSString stringWithUTF8String:src.c_str()];
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.fastMathEnabled = YES;
    id<MTLLibrary> lib = [m_device newLibraryWithSource:ns_src
                                                options:options
                                                  error:err];
    if (lib)
      m_libraries[path] = lib;
    return lib;
  }

  id<MTLComputePipelineState>
  CompileKernel(const std::string &path, const char *fn_name, NSError **err) {
    id<MTLLibrary> lib = CompileLibrary(path, err);
    if (!lib)
      return nil;
    id<MTLFunction> fn =
        [lib newFunctionWithName:[NSString stringWithUTF8String:fn_name]];
    if (!fn) {
      LUCIDA_ERROR(Render, "kernel '%s' not found in %s", fn_name, path.c_str());
      return nil;
    }
    return [m_device newComputePipelineStateWithFunction:fn error:err];
  }

  std::map<std::string, id<MTLLibrary>> m_libraries;

  // Backing-store size of the layer, in pixels. Same arithmetic SDL did, minus
  // the dependency: bounds are points, contentsScale converts to pixels.
  void SyncLayerSize() {
    const CGSize bounds = m_layer.bounds.size;
    const CGFloat scale = m_layer.contentsScale > 0.0 ? m_layer.contentsScale : 1.0;
    const int dw = std::max(1, (int)(bounds.width * scale));
    const int dh = std::max(1, (int)(bounds.height * scale));
    if (dw != m_render_w || dh != m_render_h) {
      m_layer.drawableSize = CGSizeMake(dw, dh);
      m_render_w = dw;
      m_render_h = dh;
      CreateRenderTargets();
      LUCIDA_INFO(Render, "drawable %dx%d", dw, dh);
    }
  }

  void CreateRenderTargets() {
    if (m_render_w <= 0 || m_render_h <= 0)
      return;

    // Ray resolution actually follows m_render_scale now. It used to be
    // hardcoded to m_render_w/2 in the dispatch while the textures were
    // sized by m_render_scale, so any scale other than 0.5 either rendered
    // outside the target or left part of it stale.
    m_ray_w = std::max(1, (int)(m_render_w * m_render_scale));
    m_ray_h = std::max(1, (int)(m_render_h * m_render_scale));
    m_fog_w = std::max(1, (int)(m_ray_w * kFogScale));
    m_fog_h = std::max(1, (int)(m_ray_h * kFogScale));

    auto mktex = [&](MTLPixelFormat fmt, int w, int h) -> id<MTLTexture> {
      MTLTextureDescriptor *desc =
          [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                             width:w
                                                            height:h
                                                         mipmapped:NO];
      desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      desc.storageMode = MTLStorageModePrivate;
      return [m_device newTextureWithDescriptor:desc];
    };

    m_tex_gbuffer = mktex(MTLPixelFormatRGBA16Float, m_ray_w, m_ray_h);
    m_tex_depth = mktex(MTLPixelFormatR32Float, m_ray_w, m_ray_h);
    m_tex_motion = mktex(MTLPixelFormatRG16Float, m_ray_w, m_ray_h);
    m_tex_fog = mktex(MTLPixelFormatRGBA16Float, m_fog_w, m_fog_h);
    m_tex_fog_depth = mktex(MTLPixelFormatR32Float, m_fog_w, m_fog_h);
    m_tex_ao = mktex(MTLPixelFormatRGBA16Float, m_fog_w, m_fog_h);
    m_tex_gi_norm = mktex(MTLPixelFormatRGBA16Float, m_fog_w, m_fog_h);

    // Resolution changed: the scaler's history no longer corresponds to
    // anything, so this is one of the few places a reset is warranted.
    m_reset_history = true;
    m_have_prev_vp = false;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
    if (@available(macOS 13.0, *)) {
      MTLFXTemporalScalerDescriptor *sdesc =
          [MTLFXTemporalScalerDescriptor new];
      sdesc.inputWidth = m_ray_w;
      sdesc.inputHeight = m_ray_h;
      sdesc.outputWidth = m_render_w;
      sdesc.outputHeight = m_render_h;
      sdesc.colorTextureFormat = MTLPixelFormatRGBA16Float;
      sdesc.depthTextureFormat = MTLPixelFormatR32Float;
      sdesc.motionTextureFormat = MTLPixelFormatRG16Float;
      sdesc.outputTextureFormat = m_layer.pixelFormat;
      sdesc.autoExposureEnabled = NO;
      m_temporal_scaler = [sdesc newTemporalScalerWithDevice:m_device];
      if (!m_temporal_scaler) {
        LUCIDA_WARN(Render, "MetalFX temporal scaler unavailable for %dx%d -> %dx%d",
                    m_ray_w, m_ray_h, m_render_w, m_render_h);
      }
      // Motion vectors are written in input-texture pixels, and depth is
      // a conventional near=0 / far=1 buffer.
      m_temporal_scaler.motionVectorScaleX = 1.0f;
      m_temporal_scaler.motionVectorScaleY = 1.0f;
      m_temporal_scaler.depthReversed = NO;
    }
#endif
    LUCIDA_INFO(Render, "targets: output %dx%d  rays %dx%d  fog %dx%d",
                m_render_w, m_render_h, m_ray_w, m_ray_h, m_fog_w, m_fog_h);
  }

public:
  // ------------------------------------------------- Init
  bool Init(const SurfaceDesc &surface) override {
    m_render_w = surface.width;
    m_render_h = surface.height;

    m_device = MTLCreateSystemDefaultDevice();
    if (!m_device) {
      LUCIDA_ERROR(Render, "no Metal device");
      return false;
    }

    if (!surface.native_layer) {
      LUCIDA_ERROR(Render, "surface has no CAMetalLayer");
      return false;
    }
    m_layer = (__bridge CAMetalLayer *)surface.native_layer;
    m_layer.device = m_device;
    m_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    m_layer.framebufferOnly = NO; // allow compute shader writes
    SyncLayerSize();

    m_queue = [m_device newCommandQueue];
    m_rpdesc = [MTLRenderPassDescriptor new];

    // m_uniforms is zero-initialised, and a max_depth of 0 makes trace_ray
    // bail out of its first iteration and return black. The app happened to
    // paper over this by calling SetMaxDepth() at startup; anything that did
    // not (the benchmark) rendered an empty scene.
    m_uniforms.max_depth = 7;

    NSError *err = nil;
    m_pipeline02 = CompileKernel("shaders/shader_v02.metal",
                                 "raytrace_kernel", &err);
    if (!m_pipeline02) {
      LUCIDA_ERROR(Render, "shader v02: %s",
                   err ? [[err localizedDescription] UTF8String] : "?");
      return false;
    }
    m_pipeline03 = CompileKernel("shaders/shader_v03.metal",
                                 "raytrace_kernel", &err);
    if (!m_pipeline03) {
      LUCIDA_ERROR(Render, "shader v03: %s",
                   err ? [[err localizedDescription] UTF8String] : "?");
      return false;
    }
    m_pipeline_fog = CompileKernel("shaders/shader_v03.metal",
                                   "fog_kernel", &err);
    if (!m_pipeline_fog) {
      LUCIDA_ERROR(Render, "fog kernel: %s",
                   err ? [[err localizedDescription] UTF8String] : "?");
      return false;
    }

    for (int i = 0; i < kMaxFramesInFlight; i++) {
      m_buf_uniforms[i] =
          [m_device newBufferWithLength:sizeof(GPUUniforms)
                                options:MTLResourceStorageModeShared];
    }
    m_frame_sema = dispatch_semaphore_create(kMaxFramesInFlight);

    return true;
  }

  // Only the graphics half of ImGui lives here; the platform half is wired up
  // by the platform module against the same context.
  void OverlayInit() override { ImGui_ImplMetal_Init(m_device); }
  void OverlayShutdown() override { ImGui_ImplMetal_Shutdown(); }

  // ------------------------------------------------- Camera and settings
  void SetCamera(const CameraState &camera) override {
    m_cam_pos = camera.position;
    m_yaw = camera.yaw;
    m_pitch = camera.pitch;
  }

  void ApplySettings(const RenderSettings &s) override {
    m_samples = s.samples;
    m_uniforms.max_depth = s.max_depth;
    m_fog = s.fog;

    if (m_uniforms.debug_mode != s.debug_mode) {
      m_uniforms.debug_mode = s.debug_mode;
      m_reset_history = true;
    }
    if (std::abs(m_render_scale - s.render_scale) > 0.01f) {
      m_render_scale = s.render_scale;
      CreateRenderTargets();
    }
    if (m_vsync != s.vsync) {
      m_vsync = s.vsync;
      m_layer.displaySyncEnabled = s.vsync ? YES : NO;
    }
  }

  RenderSettings Settings() const override {
    RenderSettings s;
    s.samples = m_samples;
    s.max_depth = m_uniforms.max_depth;
    s.debug_mode = m_uniforms.debug_mode;
    s.render_scale = m_render_scale;
    s.fog = m_fog;
    s.vsync = m_vsync;
    return s;
  }

  void SubmitScene(const RenderScene &scene) override {
    auto upload = [&](const void *data, size_t count, size_t stride) -> id<MTLBuffer> {
      // Never hand Metal a zero-length buffer: an empty list still needs a
      // bound slot, and the count in the uniforms is what stops the shader.
      return MakePrivateBuffer(count ? data : nullptr, std::max(count * stride, stride));
    };

    m_buf_mats    = upload(scene.materials.data(), scene.materials.size(), sizeof(GPUMaterial));
    m_buf_spheres = upload(scene.spheres.data(),   scene.spheres.size(),   sizeof(GPUSphere));
    m_buf_planes  = upload(scene.planes.data(),    scene.planes.size(),    sizeof(GPUPlane));
    m_buf_cubes   = upload(scene.cubes.data(),     scene.cubes.size(),     sizeof(GPUCube));
    m_buf_lights  = upload(scene.lights.data(),    scene.lights.size(),    sizeof(GPULight));

    m_uniforms.num_spheres = int(scene.spheres.size());
    m_uniforms.num_planes  = int(scene.planes.size());
    m_uniforms.num_cubes   = int(scene.cubes.size());
    m_uniforms.num_lights  = int(scene.lights.size());

    m_environment = scene.environment;
    m_model       = scene.model;
    m_fog         = scene.environment.fog_enabled;   // settings may override later

    // Analytic-only scene: meshes are submitted separately and turn this back on.
    m_uniforms.enable_triangles = m_mesh_loaded ? 1 : 0;
    m_total_rays = m_ray_w * m_ray_h * 4 * 7;

    m_reset_history = true;
    m_have_prev_vp = false;
    LUCIDA_INFO(Render, "scene '%s': %zu spheres, %zu planes, %zu cubes, %zu lights",
                scene.name.c_str(), scene.spheres.size(), scene.planes.size(),
                scene.cubes.size(), scene.lights.size());
  }

  // ------------------------------------------------- Mesh loading
  // Uploads the base-colour and ORM arrays for a mesh.
  // Note: there is one shared array, so the most recently uploaded mesh wins.
  // Fine for a single textured mesh; a scene mixing two textured meshes needs
  // the arrays merged, which is still outstanding.
  void UploadMeshTextures(const MeshData &mesh) {
    // Uploads one texture array slice-by-slice and builds its mip chain.
    // Without mips every mesh texture was point-sampled at level 0, which is
    // the main reason Sponza shimmered into noise at any distance.
    auto uploadArray = [&](const std::vector<uint8_t> &data, int side,
                           MTLPixelFormat fmt,
                           const char *label) -> id<MTLTexture> {
      if (data.empty() || side <= 0)
        return nil;
      const size_t slice_bytes = size_t(side) * side * 4;
      size_t slices = mesh.materials.size();

      MTLTextureDescriptor *tdesc = [MTLTextureDescriptor new];
      tdesc.textureType = MTLTextureType2DArray;
      tdesc.pixelFormat = fmt;
      tdesc.width = side;
      tdesc.height = side;
      tdesc.arrayLength = slices;
      tdesc.usage = MTLTextureUsageShaderRead;
      tdesc.mipmapLevelCount = 1 + (int)std::floor(std::log2((double)side));
      tdesc.storageMode = MTLStorageModePrivate;

      id<MTLTexture> tex = [m_device newTextureWithDescriptor:tdesc];

      // Private storage needs a staging blit rather than replaceRegion. The
      // staging buffer holds one slice at a time and is reused: copying the
      // whole array in one go meant a second full-size allocation, and for
      // Sponza4k's base-colour array that is another gigabyte of host memory
      // live at the worst possible moment.
      id<MTLBuffer> staging =
          [m_device newBufferWithLength:slice_bytes
                                options:MTLResourceStorageModeShared];
      for (size_t i = 0; i < slices; i++) {
        memcpy([staging contents], data.data() + i * slice_bytes, slice_bytes);
        id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
        [blit copyFromBuffer:staging
                   sourceOffset:0
              sourceBytesPerRow:side * 4
            sourceBytesPerImage:slice_bytes
                     sourceSize:MTLSizeMake(side, side, 1)
                      toTexture:tex
               destinationSlice:i
               destinationLevel:0
              destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];  // staging is reused on the next iteration
      }
      {
        id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
        [blit generateMipmapsForTexture:tex];
        [blit endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];
      }

      LUCIDA_INFO(Render, "%s: %d x %d^2 + %lu mips (%zu MB)", label, slices, side,
                  (unsigned long)tdesc.mipmapLevelCount,
                  (data.size() * 4 / 3) / (1024 * 1024));
      return tex;
    };

    // sRGB for base colour: the source maps are authored in gamma space, and
    // decoding them in the sampler is what keeps the albedo from reading
    // washed out. ORM is data, so it stays linear.
    m_tex_mesh_arrays =
        uploadArray(mesh.texture_array_data, mesh.tex_size,
                    MTLPixelFormatRGBA8Unorm_sRGB, "Base colour");
    m_tex_mesh_orm = uploadArray(mesh.orm_array_data, mesh.orm_size,
                                 MTLPixelFormatRGBA8Unorm, "ORM");
    m_mesh_tex_dim = mesh.tex_size > 0 ? mesh.tex_size : 1;
    m_orm_tex_dim = mesh.orm_size > 0 ? mesh.orm_size : 1;
  }

  MeshHandle AddMesh(const MeshData &mesh) override {
    if (!mesh.valid || mesh.tri_pos.empty()) return MeshHandle{};

    MeshSlot slot;
    slot.node_base = int(m_pool_nodes.size());
    slot.tri_base  = int(m_pool_tripos.size());
    slot.mat_base  = int(m_pool_mats.size());
    slot.node_count = int(mesh.bvh_nodes.size());
    slot.tri_count  = int(mesh.tri_pos.size());
    slot.mat_count  = int(mesh.materials.size());
    slot.aabb_min = mesh.aabb_min;
    slot.aabb_max = mesh.aabb_max;

    m_pool_nodes.insert(m_pool_nodes.end(), mesh.bvh_nodes.begin(), mesh.bvh_nodes.end());
    m_pool_tripos.insert(m_pool_tripos.end(), mesh.tri_pos.begin(), mesh.tri_pos.end());
    m_pool_triattr.insert(m_pool_triattr.end(), mesh.tri_attr.begin(), mesh.tri_attr.end());
    m_pool_mats.insert(m_pool_mats.end(), mesh.materials.begin(), mesh.materials.end());

    m_mesh_slots.push_back(slot);
    UploadPools();
    UploadMeshTextures(mesh);
    // bindScene() gates the mesh buffers on this; without it an instance added
    // through AddMesh alone (a car with no world mesh) bound nothing at all.
    m_mesh_loaded = true;

    m_num_triangles = int(m_pool_tripos.size());
    m_num_bvh_nodes = int(m_pool_nodes.size());
    m_num_mesh_mats = int(m_pool_mats.size());
    m_uniforms.enable_triangles = 1;
    m_uniforms.num_triangles = m_num_triangles;
    m_uniforms.num_bvh_nodes = m_num_bvh_nodes;
    // Deliberately leaves the analytic counts alone. Adding a mesh used to zero
    // spheres, cubes and lights, on the assumption that a mesh replaces the
    // scene — so loading a model silently unlit the world and deleted its
    // primitives. A mesh is one more thing in the scene, not the scene.
    m_reset_history = true;
    m_have_prev_vp = false;

    LUCIDA_INFO(Render, "BLAS %zu: %d tris, %d nodes, %d materials",
                m_mesh_slots.size() - 1, slot.tri_count, slot.node_count, slot.mat_count);
    // Slots are append-only, so generation 1 is enough to separate a live
    // handle from a default-constructed one.
    return MeshHandle{uint32_t(m_mesh_slots.size()) - 1, 1};
  }

  InstanceHandle AddInstance(MeshHandle mesh, const Mat4 &l2w) override {
    if (!mesh.IsValid() || mesh.index >= m_mesh_slots.size()) return InstanceHandle{};
    GPUInstance gi{};
    WriteInstance(gi, m_mesh_slots[mesh.index], l2w);
    // A new instance has no history: seed prev with its current transform so it
    // does not appear to have flown in from the origin on its first frame.
    memcpy(gi.prev_local_to_world, gi.local_to_world, sizeof(gi.local_to_world));

    std::array<float, 12> last{};
    memcpy(last.data(), gi.local_to_world, sizeof(gi.local_to_world));
    m_instance_last_l2w.push_back(last);

    m_instances.push_back(gi);
    m_instance_mesh.push_back(int(mesh.index));
    m_instances_dirty = true;
    m_reset_history = true;
    return InstanceHandle{uint32_t(m_instances.size()) - 1, 1};
  }

  void SetInstanceTransform(InstanceHandle inst, const Mat4 &l2w) override {
    if (!inst.IsValid() || inst.index >= m_instances.size()) return;
    WriteInstance(m_instances[inst.index], m_mesh_slots[m_instance_mesh[inst.index]], l2w);
    m_instances_dirty = true;
  }

  void ClearInstances() override {
    m_instances.clear();
    m_instance_mesh.clear();
    m_instance_last_l2w.clear();
    m_instances_dirty = true;
  }

  void LoadMesh(const MeshData &mesh) {
    if (!mesh.valid || mesh.tri_pos.empty())
      return;

    auto mkbuf = [&](const void *data, size_t bytes) -> id<MTLBuffer> {
      return MakePrivateBuffer(data, bytes);
    };

    // Single-mesh path, expressed through the instance machinery so there is
    // only one code path for the shader to deal with.
    m_mesh_slots.clear();
    m_pool_nodes.clear(); m_pool_tripos.clear();
    m_pool_triattr.clear(); m_pool_mats.clear();
    ClearInstances();
    const MeshHandle slot = AddMesh(mesh);
    if (slot.IsValid())
      AddInstance(slot, glm::translate(glm::mat4(1.0f), mesh.origin));

    UploadMeshTextures(mesh);

    m_num_triangles = int(mesh.tri_pos.size());
    m_num_bvh_nodes = int(mesh.bvh_nodes.size());
    m_num_mesh_mats = int(mesh.materials.size());
    m_mesh_loaded = true;
    m_uniforms.enable_triangles = 1;

    m_uniforms.enable_triangles = 1;
    m_uniforms.num_triangles = m_num_triangles;
    m_uniforms.num_bvh_nodes = m_num_bvh_nodes;
    m_uniforms.num_spheres = 0;
    m_uniforms.num_cubes = 0;
    // The demo point lights are sized for a three-sphere scene; a mesh is lit
    // by the directional sun and sky irradiance in the shader instead.
    m_uniforms.num_lights = 0;
    SetVec3(m_uniforms.model_pos, mesh.origin);

    // The scene changed completely; nothing in the history is reusable.
    m_reset_history = true;
    m_have_prev_vp = false;

    LUCIDA_INFO(Render, "mesh loaded: %d tris, %d BVH nodes", m_num_triangles,
                m_num_bvh_nodes);
  }

  void ClearMeshes() override {
    m_mesh_loaded = false;
    m_buf_triangles = nil;
    m_buf_bvh = nil;
    m_buf_mesh_mats = nil;
    m_buf_tri_attr = nil;
    m_buf_instances = nil;
    m_mesh_slots.clear();
    m_pool_nodes.clear(); m_pool_tripos.clear();
    m_pool_triattr.clear(); m_pool_mats.clear();
    ClearInstances();
    m_uniforms.num_instances = 0;
    m_tex_mesh_arrays = nil;
    m_tex_mesh_orm = nil;
    m_num_triangles = 0;
    m_uniforms.num_lights = 2; // restore the demo lights
    m_reset_history = true;
    m_have_prev_vp = false;
    m_uniforms.enable_triangles = 0;
    m_uniforms.num_triangles = 0;
  }

  void SetMeshOrigin(const Vec3 &origin) override {
    SetVec3(m_uniforms.model_pos, origin);
  }

  // ------------------------------------------------- Resize
  void Resize(int w, int h) override {
    m_render_w = w;
    m_render_h = h;
    SyncLayerSize();
  }

  // ------------------------------------------------- Overlay
  void OverlayNewFrame() override {
    m_drawable = [m_layer nextDrawable];
    if (!m_drawable)
      return;

    m_rpdesc.colorAttachments[0].texture = m_drawable.texture;
    m_rpdesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    m_rpdesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    ImGui_ImplMetal_NewFrame(m_rpdesc);
  }

  // ------------------------------------------------- Render
  void Render(const FrameTime &time) override {
    const float dt = time.real_delta;
    @autoreleasepool {
      if (!m_drawable)
        return;
      id<CAMetalDrawable> drawable = m_drawable;

      // Block until a uniform buffer slot is free (max frames in flight).
      dispatch_semaphore_wait(m_frame_sema, DISPATCH_TIME_FOREVER);
      m_frame_index = (m_frame_index + 1) % kMaxFramesInFlight;
      id<MTLBuffer> uniforms_buf = m_buf_uniforms[m_frame_index];

      // Update uniforms
      Vec3 fwd(cos(m_yaw) * cos(m_pitch), sin(m_pitch),
               sin(m_yaw) * cos(m_pitch));
      Vec3 right = glm::normalize(glm::cross(fwd, Vec3(0, 1, 0)));
      Vec3 up = glm::normalize(glm::cross(right, fwd));

      float tan_half = float(tan((60.0 * M_PI / 180.0) / 2.0));
      float aspect = float(m_render_w) / float(m_render_h);

      // Halton(2,3) sub-pixel jitter in [-0.5, 0.5] pixels. Each frame
      // samples a different point inside the pixel; MetalFX resolves the
      // sequence into real detail rather than just smoothing.
      // Held at zero for now. The jitterOffset sign convention MetalFX expects
      // was never verified against the y-flip in this kernel's ray generation;
      // with the wrong sign the scaler does not cancel the offset and the image
      // visibly trembles while the camera is still. A stable frame beats the
      // extra reconstructed detail until that is confirmed.
      float jx = 0.0f, jy = 0.0f;
      (void)halton;

      m_uniforms.tan_half_fov = tan_half;
      m_uniforms.aspect_ratio = aspect;
      m_uniforms.screen_width = float(m_ray_w);
      m_uniforms.screen_height = float(m_ray_h);
      m_uniforms.fog_width = float(m_fog_w);
      m_uniforms.fog_height = float(m_fog_h);
      m_uniforms.jitter_x = jx;
      m_uniforms.jitter_y = jy;
      m_uniforms.fog_density = m_environment.fog_density;
      m_uniforms.fog_steps = m_environment.fog_steps;
      m_uniforms.frame_index = int(m_frame_counter & 0x7fffffff);
      m_uniforms.mesh_tex_dim = float(m_mesh_tex_dim);
      m_uniforms.orm_tex_dim = float(m_orm_tex_dim);
      m_uniforms.mesh_mat_count = m_num_mesh_mats;
      SetVec3(m_uniforms.ambient_light, m_environment.ambient);
      SetVec3(m_uniforms.camera_origin, m_cam_pos);
      SetVec3(m_uniforms.camera_forward, fwd);
      SetVec3(m_uniforms.camera_right, right);
      SetVec3(m_uniforms.camera_up, up);
      m_uniforms.time = float(std::fmod(time.elapsed, 10000.0));
      m_uniforms.enable_fog = m_fog ? 1 : 0;
      m_uniforms.enable_jitter = m_jitter ? 1 : 0;
      m_uniforms.samples_per_pixel = m_samples;

      // View-projection matching the ray generation above, so the shader
      // can reproject world positions into the previous frame for motion
      // vectors. First frame has no history: feed it the current matrix so
      // motion comes out as zero rather than garbage.
      glm::mat4 vp = MakeViewProj(m_cam_pos, fwd, right, up, aspect, tan_half);
      if (!m_have_prev_vp) {
        m_prev_view_proj = vp;
        m_have_prev_vp = true;
      }
      memcpy(m_uniforms.prev_view_proj, &m_prev_view_proj[0][0],
             sizeof(float) * 16);
      m_prev_view_proj = vp;

      memcpy([uniforms_buf contents], &m_uniforms, sizeof(GPUUniforms));

      // Instance transforms change every frame for anything that moves; the
      // BLAS pools do not.
      RollInstanceMotion();
      if (m_instances_dirty)
        UploadInstances();

      id<MTLCommandBuffer> cmd = [m_queue commandBuffer];

      // Scene buffers are identical for both passes.
      auto bindScene = [&](id<MTLComputeCommandEncoder> ce) {
        [ce setBuffer:m_buf_mats offset:0 atIndex:0];
        [ce setBuffer:m_buf_spheres offset:0 atIndex:1];
        [ce setBuffer:m_buf_planes offset:0 atIndex:2];
        [ce setBuffer:m_buf_cubes offset:0 atIndex:3];
        // atIndex:4 reserved for octahedrons
        [ce setBuffer:m_buf_lights offset:0 atIndex:5];
        [ce setBuffer:uniforms_buf offset:0 atIndex:6];
        if (m_mesh_loaded && m_buf_triangles) {
          [ce setBuffer:m_buf_triangles offset:0 atIndex:7];
          [ce setBuffer:m_buf_bvh offset:0 atIndex:8];
          [ce setBuffer:m_buf_mesh_mats offset:0 atIndex:9];
          [ce setBuffer:m_buf_tri_attr offset:0 atIndex:10];
          [ce setBuffer:m_buf_instances offset:0 atIndex:11];
        }
      };

      // --- Pass 1: volumetric fog, at kFogScale of the ray resolution.
      // This is the pass that used to live inside the tracer and cost ~76%
      // of the frame; marching it separately at reduced resolution is what
      // buys the frame rate back.
      // Always run: this pass produces AO/GI as well as fog.
      bool fog_active = m_model == ShadingModel::WhittedGI && m_pipeline_fog;
      if (fog_active) {
        id<MTLComputeCommandEncoder> fe = [cmd computeCommandEncoder];
        [fe setComputePipelineState:m_pipeline_fog];
        [fe setTexture:m_tex_fog atIndex:0];
        [fe setTexture:m_tex_fog_depth atIndex:1];
        [fe setTexture:m_tex_ao atIndex:2];
        [fe setTexture:m_tex_gi_norm atIndex:3];
        [fe setTexture:(m_tex_mesh_arrays ? m_tex_mesh_arrays : m_tex_fog) atIndex:4];
        bindScene(fe);
        [fe dispatchThreads:MTLSizeMake(m_fog_w, m_fog_h, 1)
            threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
        [fe endEncoding];
      }

      // --- Pass 2: ray tracing at m_ray_w x m_ray_h
      id<MTLComputeCommandEncoder> ce = [cmd computeCommandEncoder];
      // v02 is the legacy Demo 0.2 kernel only; every later demo runs the
      // current shader. Testing for ==1 sent Demo 0.4 down the old path, where
      // none of the procedural material code exists.
      [ce setComputePipelineState:(m_model == ShadingModel::Whitted ? m_pipeline02
                                                  : m_pipeline03)];
      [ce setTexture:m_tex_gbuffer atIndex:0];
      if (m_tex_mesh_arrays) {
        [ce setTexture:m_tex_mesh_arrays atIndex:1];
      } else {
        [ce setTexture:m_tex_gbuffer atIndex:1]; // dummy bind
      }
      [ce setTexture:m_tex_depth atIndex:2];
      [ce setTexture:m_tex_motion atIndex:3];
      [ce setTexture:m_tex_fog atIndex:4];
      [ce setTexture:m_tex_fog_depth atIndex:5];
      [ce setTexture:m_tex_ao atIndex:7];
      [ce setTexture:m_tex_gi_norm atIndex:8];
      [ce setTexture:(m_tex_mesh_orm ? m_tex_mesh_orm : m_tex_mesh_arrays)
             atIndex:6];
      bindScene(ce);
      [ce dispatchThreads:MTLSizeMake(m_ray_w, m_ray_h, 1)
          threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
      [ce endEncoding];

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
      if (@available(macOS 13.0, *)) {
        if (m_temporal_scaler) {
          m_temporal_scaler.colorTexture = m_tex_gbuffer;
          m_temporal_scaler.depthTexture = m_tex_depth;
          m_temporal_scaler.motionTexture = m_tex_motion;
          m_temporal_scaler.outputTexture = drawable.texture;
          // The scaler needs the same sub-pixel offset the rays used,
          // negated, to place the samples correctly in the history.
          m_temporal_scaler.jitterOffsetX = -jx;
          m_temporal_scaler.jitterOffsetY = -jy;
          // Only true after a resize / scene change, never for motion.
          m_temporal_scaler.reset = m_reset_history ? YES : NO;
          [m_temporal_scaler encodeToCommandBuffer:cmd];
          m_reset_history = false;
        }
      }
#endif

      // Temporary diagnostic: is anything actually writing the drawable?
      if ((m_frame_counter % 60) == 1) {
        LUCIDA_INFO(Render, "frame %llu: scaler=%s rays=%dx%d out=%dx%d drawable=%s imgui_lists=%d",
                    (unsigned long long)m_frame_counter,
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 130000
                    m_temporal_scaler ? "yes" : "NO",
#else
                    "unavailable",
#endif
                    m_ray_w, m_ray_h, m_render_w, m_render_h,
                    drawable ? "yes" : "NO",
                    ImGui::GetDrawData() ? ImGui::GetDrawData()->CmdListsCount : -1);
      }

      // --- Render pass (ImGui overlay)
      m_rpdesc.colorAttachments[0].texture = drawable.texture;
      m_rpdesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
      m_rpdesc.colorAttachments[0].storeAction = MTLStoreActionStore;

      id<MTLRenderCommandEncoder> re =
          [cmd renderCommandEncoderWithDescriptor:m_rpdesc];
      ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cmd, re);
      [re endEncoding];

      // Release the uniform-buffer slot once the GPU is done with this frame.
      __block dispatch_semaphore_t sema = m_frame_sema;
      __block std::atomic<double> *ema = &m_gpu_ms_ema;
      __block std::atomic<uint64_t> *nsamp = &m_gpu_samples;
      [cmd addCompletedHandler:^(id<MTLCommandBuffer> cb) {
        if (cb.error) {
          LUCIDA_ERROR(Render, "command buffer: %s",
                       [[cb.error localizedDescription] UTF8String]);
        }
        double ms = (cb.GPUEndTime - cb.GPUStartTime) * 1000.0;
        if (ms > 0.0) {
          double prev = ema->load(std::memory_order_relaxed);
          ema->store(prev <= 0.0 ? ms : prev * 0.9 + ms * 0.1,
                     std::memory_order_relaxed);
          nsamp->fetch_add(1, std::memory_order_relaxed);
        }
        dispatch_semaphore_signal(sema);
      }];

      [cmd presentDrawable:drawable];
      [cmd commit];
      m_drawable = nil;
      m_frame_counter++;
    }
  }

  // Reads the traced (pre-upscale) colour target back to the CPU. This is the
  // image the ray tracer actually produced, which is what you want to inspect
  // when judging shading — MetalFX upscaling is a separate stage on top.
  bool ReadbackFrame(std::vector<uint8_t> &rgba, int &w, int &h) override {
    if (!m_tex_gbuffer || m_ray_w <= 0 || m_ray_h <= 0)
      return false;
    w = m_ray_w;
    h = m_ray_h;

    const size_t row_bytes = size_t(w) * 8; // RGBA16Float
    id<MTLBuffer> dst =
        [m_device newBufferWithLength:row_bytes * h
                              options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd = [m_queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromTexture:m_tex_gbuffer
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:MTLOriginMake(0, 0, 0)
                      sourceSize:MTLSizeMake(w, h, 1)
                        toBuffer:dst
               destinationOffset:0
          destinationBytesPerRow:row_bytes
        destinationBytesPerImage:row_bytes * h];
    [blit endEncoding];
    [cmd commit];
    [cmd waitUntilCompleted];

    const uint16_t *src = (const uint16_t *)[dst contents];
    rgba.resize(size_t(w) * h * 4);
    for (size_t i = 0; i < size_t(w) * h * 4; i++) {
      rgba[i] =
          (uint8_t)(std::clamp(half_to_float(src[i]), 0.0f, 1.0f) * 255.0f +
                    0.5f);
    }
    return true;
  }

  // ------------------------------------------------- Stats
  RenderStats Stats() const override {
    RenderStats s;
    s.cpu_frame_ms = 0.0f;
    s.gpu_frame_ms = float(m_gpu_ms_ema.load(std::memory_order_relaxed));
    s.ray_count = m_total_rays;
    s.tri_count = m_num_triangles;
    return s;
  }

  // ------------------------------------------------- Shutdown
  void Shutdown() override {
    // The layer belongs to the platform module, which destroys it.
  }
};

} // namespace

std::unique_ptr<IRenderBackend> CreateMetalBackend() {
  return std::make_unique<MetalBackend>();
}

} // namespace lucida
