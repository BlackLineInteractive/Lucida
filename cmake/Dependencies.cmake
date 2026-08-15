# All third-party code is fetched at configure time. Nothing vendored, nothing
# committed: `git clone && cmake -B build` is the whole setup on every platform.
#
# Two mechanisms, picked per dependency:
#   lucida_fetch_headers() — header-only or no usable CMake (glm, bvh, imgui).
#     Downloads a release tarball and makes an INTERFACE target. Avoids
#     configuring a foreign project just to get include paths.
#   FetchContent_MakeAvailable() — real builds (SDL2, assimp, Jolt, Bullet),
#     each preceded by find_package so a system install wins and CI stays fast.

include(FetchContent)

set(LUCIDA_DEPS_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "Where fetched dependencies land")
set(FETCHCONTENT_BASE_DIR "${LUCIDA_DEPS_DIR}")

option(LUCIDA_PREFER_SYSTEM_DEPS "Use system SDL2/assimp when found" ON)

# lucida_fetch_headers(<name> URL <url> STRIP <dir-inside-archive> INCLUDE <subdir>)
# Creates the INTERFACE target lucida_<name> exposing <src>/<subdir>.
function(lucida_fetch_headers name)
    cmake_parse_arguments(ARG "" "URL;INCLUDE;SHA256" "" ${ARGN})

    set(dst "${LUCIDA_DEPS_DIR}/${name}")
    set(stamp "${dst}/.lucida-fetched")

    if(NOT EXISTS "${stamp}")
        message(STATUS "[deps] fetching ${name}")
        set(archive "${LUCIDA_DEPS_DIR}/${name}.tar.gz")
        if(ARG_SHA256)
            file(DOWNLOAD "${ARG_URL}" "${archive}" STATUS dl EXPECTED_HASH SHA256=${ARG_SHA256})
        else()
            file(DOWNLOAD "${ARG_URL}" "${archive}" STATUS dl)
        endif()
        list(GET dl 0 code)
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "[deps] ${name}: download failed (${dl}) from ${ARG_URL}")
        endif()

        file(REMOVE_RECURSE "${dst}")
        file(MAKE_DIRECTORY "${dst}")
        file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dst}")

        # GitHub tarballs wrap everything in <repo>-<tag>/; lift it out so the
        # include path does not depend on the tag string.
        file(GLOB inner LIST_DIRECTORIES true "${dst}/*")
        list(LENGTH inner count)
        if(count EQUAL 1)
            file(GLOB moved LIST_DIRECTORIES true "${inner}/*")
            foreach(item ${moved})
                get_filename_component(leaf "${item}" NAME)
                file(RENAME "${item}" "${dst}/${leaf}")
            endforeach()
            file(REMOVE_RECURSE "${inner}")
        endif()
        file(REMOVE "${archive}")
        file(WRITE "${stamp}" "${ARG_URL}")
    endif()

    set(inc "${dst}")
    if(ARG_INCLUDE)
        set(inc "${dst}/${ARG_INCLUDE}")
    endif()

    add_library(lucida_${name} INTERFACE)
    target_include_directories(lucida_${name} SYSTEM INTERFACE "${inc}")
    set(lucida_${name}_SOURCE_DIR "${dst}" PARENT_SCOPE)
endfunction()

# --- math and acceleration structures ----------------------------------------
# Neither is written by hand: glm is the de-facto standard, bvh v2 ships a
# multithreaded binned-SAH builder that beats anything worth writing here.

lucida_fetch_headers(glm URL "https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz")
lucida_fetch_headers(bvh URL "https://github.com/madmann91/bvh/archive/refs/heads/master.tar.gz"
                          INCLUDE "src")

# --- UI ----------------------------------------------------------------------
lucida_fetch_headers(imgui_src URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.90.8.tar.gz")
set(LUCIDA_IMGUI_DIR "${lucida_imgui_src_SOURCE_DIR}" CACHE INTERNAL "")

lucida_fetch_headers(imgui_fd URL "https://github.com/aiekick/ImGuiFileDialog/archive/refs/tags/v0.6.7.tar.gz")
set(LUCIDA_IFD_DIR "${lucida_imgui_fd_SOURCE_DIR}" CACHE INTERNAL "")

# --- stb single headers ------------------------------------------------------
set(LUCIDA_STB_DIR "${LUCIDA_DEPS_DIR}/stb")
file(MAKE_DIRECTORY "${LUCIDA_STB_DIR}")
foreach(hdr stb_image.h stb_image_write.h)
    if(NOT EXISTS "${LUCIDA_STB_DIR}/${hdr}")
        file(DOWNLOAD "https://raw.githubusercontent.com/nothings/stb/master/${hdr}"
                      "${LUCIDA_STB_DIR}/${hdr}" STATUS dl)
        list(GET dl 0 code)
        if(NOT code EQUAL 0)
            message(FATAL_ERROR "[deps] stb: download ${hdr} failed (${dl})")
        endif()
    endif()
endforeach()
add_library(lucida_stb INTERFACE)
target_include_directories(lucida_stb SYSTEM INTERFACE "${LUCIDA_STB_DIR}")

# --- SDL2 --------------------------------------------------------------------
set(LUCIDA_SDL2_TARGET "" CACHE INTERNAL "")
if(LUCIDA_PREFER_SYSTEM_DEPS)
    find_package(SDL2 QUIET)
endif()
if(SDL2_FOUND)
    message(STATUS "[deps] SDL2: system")
    if(TARGET SDL2::SDL2)
        set(LUCIDA_SDL2_TARGET SDL2::SDL2 CACHE INTERNAL "")
    else()
        add_library(lucida_sdl2_system INTERFACE)
        target_include_directories(lucida_sdl2_system SYSTEM INTERFACE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(lucida_sdl2_system INTERFACE ${SDL2_LIBRARIES})
        set(LUCIDA_SDL2_TARGET lucida_sdl2_system CACHE INTERNAL "")
    endif()
else()
    message(STATUS "[deps] SDL2: building from source")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    set(SDL_TEST   OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL2)
    set(LUCIDA_SDL2_TARGET SDL2-static CACHE INTERNAL "")
endif()

# --- assimp ------------------------------------------------------------------
set(LUCIDA_ASSIMP_TARGET "" CACHE INTERNAL "")
if(LUCIDA_PREFER_SYSTEM_DEPS)
    find_package(assimp QUIET)
endif()
if(assimp_FOUND)
    message(STATUS "[deps] assimp: system")
    if(TARGET assimp::assimp)
        set(LUCIDA_ASSIMP_TARGET assimp::assimp CACHE INTERNAL "")
    else()
        set(LUCIDA_ASSIMP_TARGET assimp CACHE INTERNAL "")
    endif()
else()
    message(STATUS "[deps] assimp: building from source (this takes a while)")
    set(ASSIMP_BUILD_TESTS        OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL            OFF CACHE BOOL "" FORCE)
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ZLIB         ON  CACHE BOOL "" FORCE)
    # Only the formats the engine actually imports.
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER  OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER  OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON  CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER  ON  CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER  ON  CACHE BOOL "" FORCE)
    FetchContent_Declare(assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG        v5.4.3
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(assimp)
    set(LUCIDA_ASSIMP_TARGET assimp CACHE INTERNAL "")
endif()

# --- physics -----------------------------------------------------------------
if(LUCIDA_PHYSICS_JOLT)
    # Jolt keeps its CMakeLists in Build/, hence SOURCE_SUBDIR.
    set(TARGET_UNIT_TESTS       OFF CACHE BOOL "" FORCE)
    set(TARGET_HELLO_WORLD      OFF CACHE BOOL "" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
    set(TARGET_SAMPLES          OFF CACHE BOOL "" FORCE)
    set(TARGET_VIEWER           OFF CACHE BOOL "" FORCE)
    set(ENABLE_ALL_WARNINGS     OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG        v5.2.0
        GIT_SHALLOW    TRUE
        SOURCE_SUBDIR  Build)
    FetchContent_MakeAvailable(JoltPhysics)
endif()

if(LUCIDA_PHYSICS_BULLET)
    set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
    set(BUILD_CPU_DEMOS     OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTRAS        OFF CACHE BOOL "" FORCE)
    set(BUILD_UNIT_TESTS    OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(bullet3
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG        3.25
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(bullet3)
endif()

# --- Dear ImGui core ---------------------------------------------------------
# One target for the library itself; the per-platform and per-API impl files are
# compiled by the backend that needs them.
add_library(lucida_imgui STATIC
    ${LUCIDA_IMGUI_DIR}/imgui.cpp
    ${LUCIDA_IMGUI_DIR}/imgui_draw.cpp
    ${LUCIDA_IMGUI_DIR}/imgui_tables.cpp
    ${LUCIDA_IMGUI_DIR}/imgui_widgets.cpp
    ${LUCIDA_IMGUI_DIR}/imgui_demo.cpp
)
target_include_directories(lucida_imgui SYSTEM PUBLIC
    ${LUCIDA_IMGUI_DIR}
    ${LUCIDA_IMGUI_DIR}/backends
    ${LUCIDA_IFD_DIR}
)
