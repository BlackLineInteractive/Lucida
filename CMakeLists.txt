cmake_minimum_required(VERSION 3.21)
project(Lucida VERSION 0.1.0 LANGUAGES C CXX)

# Each engine module is its own library. The dependency graph in
# docs/ARCHITECTURE.md is enforced by target_link_libraries: a module that is
# not listed cannot be reached. That is the modularity, not the folder layout.
#
# Third-party code is fetched at configure time, never committed. See
# cmake/Dependencies.cmake.

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# --- platform and architecture ----------------------------------------------
if(APPLE)
    option(LUCIDA_MACOS_UNIVERSAL "Build a universal x86_64 + arm64 binary" OFF)
    if(LUCIDA_MACOS_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "" FORCE)  # MetalFX needs 13
    endif()
endif()

if(MSVC)
    add_compile_options(/utf-8 /MP /permissive-)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" LUCIDA_ARCH)
message(STATUS "[lucida] ${CMAKE_SYSTEM_NAME} / ${LUCIDA_ARCH}")

# --- backend selection -------------------------------------------------------
# Metal is the complete ray tracing path today; GL 4.3 compute is partial and
# Vulkan is a stub. Defaults follow the platform, not wishful thinking.
option(LUCIDA_RENDER_METAL   "Metal render backend (macOS)"   ${APPLE})
option(LUCIDA_RENDER_RC      "Radiance Cascades 3D backend"   ${APPLE})
if(APPLE)
    option(LUCIDA_RENDER_GL  "OpenGL 4.3 compute backend"     OFF)
else()
    option(LUCIDA_RENDER_GL  "OpenGL 4.3 compute backend"     ON)
endif()
option(LUCIDA_RENDER_VULKAN  "Vulkan render backend (stub)"   OFF)
option(LUCIDA_PHYSICS_JOLT   "Jolt physics backend"           ON)
option(LUCIDA_PHYSICS_BULLET "Bullet physics backend"         OFF)
option(LUCIDA_BUILD_SANDBOX  "Build apps/sandbox"             ON)
option(LUCIDA_BUILD_TESTS    "Build tests"                    OFF)

if(NOT APPLE AND LUCIDA_RENDER_METAL)
    message(WARNING "Metal is macOS only; disabling.")
    set(LUCIDA_RENDER_METAL OFF CACHE BOOL "" FORCE)
endif()
if(LUCIDA_PHYSICS_JOLT AND LUCIDA_PHYSICS_BULLET)
    message(FATAL_ERROR "Pick one physics backend: Jolt OR Bullet.")
endif()

if(APPLE AND (LUCIDA_RENDER_METAL OR LUCIDA_RENDER_RC))
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
endif()

include(Dependencies)

# --- modules -----------------------------------------------------------------
add_subdirectory(engine/core)
add_subdirectory(engine/runtime)
add_subdirectory(engine/render)
add_subdirectory(engine/resource)
add_subdirectory(engine/physics)
add_subdirectory(engine/input)

add_subdirectory(framework)

add_subdirectory(backends/platform_sdl2)
if(LUCIDA_RENDER_METAL)
    add_subdirectory(backends/render_metal)
endif()
if(LUCIDA_RENDER_RC)
    add_subdirectory(backends/render_radiance_cascades)
endif()
if(LUCIDA_PHYSICS_JOLT)
    add_subdirectory(backends/physics_jolt)
endif()

if(LUCIDA_BUILD_SANDBOX)
    add_subdirectory(apps/sandbox)
endif()

if(LUCIDA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "---- Lucida ${PROJECT_VERSION} ----")
message(STATUS "  build   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  render  : Metal=${LUCIDA_RENDER_METAL} GL=${LUCIDA_RENDER_GL} Vulkan=${LUCIDA_RENDER_VULKAN}")
message(STATUS "  physics : Jolt=${LUCIDA_PHYSICS_JOLT} Bullet=${LUCIDA_PHYSICS_BULLET}")
