#include "Renderer.h"
#ifdef USE_OPENGL

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include <SDL2/SDL.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

// OpenGL Function Pointers for Compute Shaders and SSBOs
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute_ = nullptr;
PFNGLBINDBUFFERBASEPROC glBindBufferBase_ = nullptr;
PFNGLSHADERSTORAGEBLOCKBINDINGPROC glShaderStorageBlockBinding_ = nullptr;
PFNGLCREATESHADERPROGRAMVPROC glCreateShaderProgramv_ = nullptr;
PFNGLMEMORYBARRIERPROC glMemoryBarrier_ = nullptr;
PFNGLBINDIMAGETEXTUREPROC glBindImageTexture_ = nullptr;

std::string ReadShader(const std::string& path) {
    char* basePathStr = SDL_GetBasePath();
    std::string basePath = basePathStr ? basePathStr : "";
    if (basePathStr) SDL_free(basePathStr);

    std::string fullPath = basePath + path;
    std::ifstream f(fullPath);
    if (!f.is_open()) {
        fullPath = basePath + "../" + path;
        f.open(fullPath);
    }
    if (!f.is_open()) {
        std::cerr << "Failed to find shader file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

GLuint CompileComputeShader(const std::string& source) {
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Compute Shader Compilation Failed: " << infoLog << std::endl;
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Compute Program Link Failed: " << infoLog << std::endl;
        return 0;
    }
    
    glDeleteShader(shader);
    return program;
}

class RendererGL : public IRenderer {
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context;
    
    GLuint computeProg02 = 0;
    GLuint computeProg03 = 0;
    
    GLuint ssbo_mats = 0;
    GLuint ssbo_spheres = 0;
    GLuint ssbo_planes = 0;
    GLuint ssbo_cubes = 0;
    GLuint ssbo_lights = 0;
    GLuint ubo_uniforms = 0;
    
    GLuint outTexture = 0;
    
    int current_version = 1;
    bool fog_enabled = true;
    Vec3 cam_pos;
    double cam_yaw = -M_PI / 2.0, cam_pitch = 0.0;
    
    int render_width, render_height;
    GPUUniforms uniforms = {};
    bool jitter_on      = false;
    int  samples_per_pixel = 1;
    int  total_rays = 0;
    int  total_tris = 0;

    void SetupScene(int version) {
        std::vector<GPUMaterial> gpu_mats;
        auto add_mat = [&](const Material& m)->int {
            GPUMaterial gm;
            set_vec3(gm.albedo, m.albedo); gm.roughness = (float)m.roughness;
            set_vec3(gm.emission, m.emission); gm.metallic = (float)m.metallic;
            set_vec3(gm.albedo2, m.albedo2); gm.refractive_index = (float)m.refractive_index;
            gm.type = m.type; gm.pad1=gm.pad2=gm.pad3=0;
            gpu_mats.push_back(gm); return (int)gpu_mats.size()-1;
        };

        Material floor_mat(CHECKERBOARD, {0.8,0.8,0.8}, {0,0,0}, 0.8, 0.0, 1.0, {0.2,0.2,0.2});
        Material chrome_mat(METAL, {0.9,0.9,0.95}, {0,0,0}, 0.05, 1.0);
        Material glass_mat(GLASS, {0.98,0.99,1.0}, {0,0,0}, 0.0, 0.0, 1.5);
        Material red_mat(DIFFUSE, {0.8,0.15,0.1}, {0,0,0}, 0.9, 0.0);
        Material blue_emit(EMISSIVE, {0,0,0}, {0.3,0.5,2.0}, 1.0, 0.0);
        Material water_mat(WATER, {0.0, 0.3, 0.4}, {0,0,0}, 0.0, 0.0, 1.33, {0,0,0});

        int i_floor = add_mat(floor_mat); int i_chrome = add_mat(chrome_mat);
        int i_glass = add_mat(glass_mat); int i_red = add_mat(red_mat); 
        int i_blue = add_mat(blue_emit); int i_water = add_mat(water_mat);

        std::vector<GPUPlane> gpu_planes;
        if (version == 1) {
            gpu_planes = {{{0,1,0}, -1.0f, i_floor, 0,0,0}, {{0,1,0}, -0.85f, i_water, 0,0,0}};
        } else {
            gpu_planes = {{{0,1,0}, -1.0f, i_floor, 0,0,0}};
        }

        std::vector<GPUSphere> gpu_spheres = {
            {{-2.0f, 0.0f, -5.0f}, 1.0f, i_chrome, 0,0,0},
            {{ 0.0f, 0.2f, -4.5f}, 1.2f, i_glass, 0,0,0},
            {{ 1.5f, 0.5f, -3.5f}, 0.3f, i_blue, 0,0,0}
        };
        std::vector<GPUCube> gpu_cubes = {{{1.5f, -0.5f, -6.0f}, 0, {0.5f,0.5f,0.5f}, i_red}};
        std::vector<GPULight> gpu_lights = {
            {{-5.0f, 8.0f, -2.0f}, 50.0f, {1.0f, 0.95f, 0.9f}, 2.0f},
            {{ 1.5f, 0.5f, -3.5f}, 15.0f, {0.3f, 0.5f, 1.0f}, 0.2f}
        };

        auto updateBuffer = [&](GLuint& buf, const void* data, size_t size) {
            if (buf == 0) glGenBuffers(1, &buf);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
            glBufferData(GL_SHADER_STORAGE_BUFFER, std::max(size, (size_t)16), data, GL_STATIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        };

        updateBuffer(ssbo_mats, gpu_mats.data(), gpu_mats.size() * sizeof(GPUMaterial));
        updateBuffer(ssbo_spheres, gpu_spheres.data(), gpu_spheres.size() * sizeof(GPUSphere));
        updateBuffer(ssbo_planes, gpu_planes.data(), gpu_planes.size() * sizeof(GPUPlane));
        updateBuffer(ssbo_cubes, gpu_cubes.data(), gpu_cubes.size() * sizeof(GPUCube));
        updateBuffer(ssbo_lights, gpu_lights.data(), gpu_lights.size() * sizeof(GPULight));

        uniforms.num_spheres = gpu_spheres.size();
        uniforms.num_planes = gpu_planes.size();
        uniforms.num_cubes = gpu_cubes.size();
        uniforms.num_octahedrons = 0;
        uniforms.num_lights = gpu_lights.size();
        
        total_rays = render_width * render_height * 4 * 7;
        total_tris = 0;
    }

public:
    bool Init(SDL_Window* win, int width, int height) override {
        window = win;
        render_width = width;
        render_height = height;

#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        gl_context = SDL_GL_CreateContext(window);
        if (!gl_context) {
            std::cerr << "Failed to create OpenGL Context: " << SDL_GetError() << std::endl;
            return false;
        }

        glDispatchCompute_ = (PFNGLDISPATCHCOMPUTEPROC)SDL_GL_GetProcAddress("glDispatchCompute");
        glBindBufferBase_ = (PFNGLBINDBUFFERBASEPROC)SDL_GL_GetProcAddress("glBindBufferBase");
        glShaderStorageBlockBinding_ = (PFNGLSHADERSTORAGEBLOCKBINDINGPROC)SDL_GL_GetProcAddress("glShaderStorageBlockBinding");
        glCreateShaderProgramv_ = (PFNGLCREATESHADERPROGRAMVPROC)SDL_GL_GetProcAddress("glCreateShaderProgramv");
        glMemoryBarrier_ = (PFNGLMEMORYBARRIERPROC)SDL_GL_GetProcAddress("glMemoryBarrier");
        glBindImageTexture_ = (PFNGLBINDIMAGETEXTUREPROC)SDL_GL_GetProcAddress("glBindImageTexture");

        if (!glDispatchCompute_ || !glBindBufferBase_ || !glBindImageTexture_) {
            std::cerr << "OpenGL 4.3 Compute Shaders not supported." << std::endl;
            return false;
        }

        std::string src02 = ReadShader("src/backends/OpenGL/shader_v02.comp");
        computeProg02 = CompileComputeShader(src02);
        std::string src03 = ReadShader("src/backends/OpenGL/shader_v03.comp");
        computeProg03 = CompileComputeShader(src03);

        glGenBuffers(1, &ubo_uniforms);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo_uniforms);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUUniforms), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glGenTextures(1, &outTexture);
        glBindTexture(GL_TEXTURE_2D, outTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
        ImGui_ImplOpenGL3_Init("#version 430 core");
        
        cam_pos = Vec3(0, 1.0, 2.0);

        return true;
    }

    void ProcessInput(const Uint8* keys, int mx, int my, float dt) override {
        cam_yaw += mx * 0.003; 
        cam_pitch -= my * 0.003;
        cam_pitch = std::clamp(cam_pitch, -1.5, 1.5);
        
        Vec3 fwd(cos(cam_yaw)*cos(cam_pitch), sin(cam_pitch), sin(cam_yaw)*cos(cam_pitch));
        Vec3 right = glm::normalize(glm::cross(fwd, Vec3(0,1,0))); 
        Vec3 up = glm::normalize(glm::cross(right, fwd));
        Vec3 flat_fwd = glm::length(Vec3(fwd.x, 0.0f, fwd.z)) > 1e-4f ? glm::normalize(Vec3(fwd.x, 0.0f, fwd.z)) : fwd;
        
        bool sprint = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        float speed = (sprint ? 12.0f : 5.0f) * dt;
        
        Vec3 move(0.0f);
        if (keys[SDL_SCANCODE_W]) move += flat_fwd; 
        if (keys[SDL_SCANCODE_S]) move -= flat_fwd; 
        if (keys[SDL_SCANCODE_A]) move -= right; 
        if (keys[SDL_SCANCODE_D]) move += right; 
        if (glm::length(move) > 1e-4f) cam_pos += glm::normalize(move) * speed;

        if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_E]) cam_pos.y += speed;
        if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_C]) cam_pos.y -= speed;
    }

    void ToggleFog() override { fog_enabled = !fog_enabled; }
    void SwitchDemo(int version) override {
        current_version = version;
        SetupScene(version);
    }
    void LoadMesh(const MeshData&) override { /* TODO: implement triangle SSBO upload */ }
    void ClearMesh() override {
        uniforms.enable_triangles = 0;
    }

    void SetMeshOrigin(float x, float y, float z) override {
        uniforms.model_pos[0] = x;
        uniforms.model_pos[1] = y;
        uniforms.model_pos[2] = z;
    }

    void SetSamples(int samples) override { samples_per_pixel = samples; }
    void OnResize(int w, int h) override {
        render_width = w; render_height = h;
        glBindTexture(GL_TEXTURE_2D, outTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void BeginImGuiFrame() override {
        ImGui_ImplOpenGL3_NewFrame();
    }

    void Render(float dt) override {
        Vec3 fwd(cos(cam_yaw)*cos(cam_pitch), sin(cam_pitch), sin(cam_yaw)*cos(cam_pitch));
        Vec3 right = glm::normalize(glm::cross(fwd, Vec3(0,1,0))); 
        Vec3 up = glm::normalize(glm::cross(right, fwd));

        uniforms.max_depth = 7;
        uniforms.tan_half_fov = std::tan((60.0*PI/180.0) / 2.0);
        uniforms.aspect_ratio = (float)render_width / render_height;
        uniforms.screen_width = render_width; 
        uniforms.screen_height = render_height;
        set_vec3(uniforms.ambient_light, {0.3, 0.4, 0.6}); 
        
        uniforms.camera_origin[0] = cam_pos.x; uniforms.camera_origin[1] = cam_pos.y; uniforms.camera_origin[2] = cam_pos.z;
        uniforms.camera_forward[0] = fwd.x; uniforms.camera_forward[1] = fwd.y; uniforms.camera_forward[2] = fwd.z;
        uniforms.camera_right[0] = right.x; uniforms.camera_right[1] = right.y; uniforms.camera_right[2] = right.z;
        uniforms.camera_up[0] = up.x; uniforms.camera_up[1] = up.y; uniforms.camera_up[2] = up.z;
        uniforms.time = (float)(SDL_GetTicks() % 10000000) / 1000.0f;
        uniforms.enable_fog          = fog_enabled ? 1 : 0;
        uniforms.enable_jitter       = jitter_on ? 1 : 0;
        uniforms.samples_per_pixel   = samples_per_pixel;

        glBindBuffer(GL_UNIFORM_BUFFER, ubo_uniforms);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GPUUniforms), &uniforms);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        GLuint prog = current_version == 0 ? computeProg02 : computeProg03;
        glUseProgram(prog);

        glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 0, ssbo_mats);
        glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 1, ssbo_spheres);
        glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 2, ssbo_planes);
        glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 3, ssbo_cubes);
        glBindBufferBase_(GL_SHADER_STORAGE_BUFFER, 5, ssbo_lights);
        glBindBufferBase_(GL_UNIFORM_BUFFER, 6, ubo_uniforms);

        glBindImageTexture_(0, outTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

        glDispatchCompute_((render_width + 15) / 16, (render_height + 15) / 16, 1);
        glMemoryBarrier_(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Render Texture to Screen using a simple quad
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, outTexture);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, 1);
        glTexCoord2f(0, 0); glVertex2f(-1, 1);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SDL_GL_SwapWindow(window);
    }

    void GetStats(float& outFrameTimeMs, int& outRayCount, int& outTriCount, float& outGpuTimeMs) override {
        outRayCount = total_rays;
        outTriCount = total_tris;
        outGpuTimeMs = 0.0f; // Could use GL_TIME_ELAPSED but skipping for simplicity
    }

    void Cleanup() override {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        SDL_GL_DeleteContext(gl_context);
    }
};

IRenderer* CreateRendererGL() {
    return new RendererGL();
}
#endif
