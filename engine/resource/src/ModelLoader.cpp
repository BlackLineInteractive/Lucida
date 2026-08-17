// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/ModelLoader.h"

#include "lucida/core/diag/Log.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/tri.h>
#include <bvh/v2/binned_sah_builder.h>
#include <bvh/v2/default_builder.h>
#include <bvh/v2/thread_pool.h>

namespace lucida {

// ------------------------------------------------------------------ helpers --

// Area-average resample. The previous implementation took a single bilinear tap
// per destination pixel, which for a 4K source into a small destination reads
// four texels out of every few hundred and throws the rest away - that is
// aliasing by construction, and it is a large part of why Sponza looked noisy.
// Averaging the full source footprint keeps the detail as detail.
static void ResizeBox(const uint8_t* src, int sw, int sh,
                      uint8_t* dst, int dw, int dh) {
    if (sw == dw && sh == dh) {
        std::memcpy(dst, src, (size_t)dw * dh * 4);
        return;
    }
    // For small palette textures (e.g. 16x16 color palettes), use nearest neighbor
    // so colors and chrome/black boundaries do not blur into mud.
    if (sw <= 64 && sh <= 64) {
        for (int y = 0; y < dh; y++) {
            int sy = (int)((int64_t)y * sh / dh);
            if (sy >= sh) sy = sh - 1;
            const uint8_t* row = src + (size_t)sy * sw * 4;
            uint8_t* drow = dst + (size_t)y * dw * 4;
            for (int x = 0; x < dw; x++) {
                int sx = (int)((int64_t)x * sw / dw);
                if (sx >= sw) sx = sw - 1;
                const uint8_t* p = row + (size_t)sx * 4;
                drow[x*4+0] = p[0];
                drow[x*4+1] = p[1];
                drow[x*4+2] = p[2];
                drow[x*4+3] = p[3];
            }
        }
        return;
    }
    for (int y = 0; y < dh; y++) {
        int y0 = (int)((int64_t)y       * sh / dh);
        int y1 = (int)((int64_t)(y + 1) * sh / dh);
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > sh)  y1 = sh;

        for (int x = 0; x < dw; x++) {
            int x0 = (int)((int64_t)x       * sw / dw);
            int x1 = (int)((int64_t)(x + 1) * sw / dw);
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > sw)  x1 = sw;

            uint32_t acc[4] = {0, 0, 0, 0};
            uint32_t n = 0;
            for (int sy = y0; sy < y1; sy++) {
                const uint8_t* row = src + (size_t)sy * sw * 4;
                for (int sx = x0; sx < x1; sx++) {
                    const uint8_t* p = row + (size_t)sx * 4;
                    acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += p[3];
                    n++;
                }
            }
            uint8_t* d = dst + ((size_t)y * dw + x) * 4;
            if (n == 0) n = 1;
            for (int c = 0; c < 4; c++) d[c] = (uint8_t)(acc[c] / n);
        }
    }
}

static GPUMaterial ConvertMaterial(const aiMaterial* ai_mat, const std::string& semantic_name = "") {
    GPUMaterial m{};
    aiColor4D   col;
    float       fval;
    aiString    name;

    std::string s = semantic_name;
    if (s.empty()) {
        if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_NAME, name)) {
            s = name.C_Str();
        }
    }
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    // Base color / albedo
    m.albedo[0] = 1.0f; m.albedo[1] = 1.0f; m.albedo[2] = 1.0f;
    if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_BASE_COLOR, col)) {
        m.albedo[0] = col.r; m.albedo[1] = col.g; m.albedo[2] = col.b;
    } else if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, col)) {
        m.albedo[0] = col.r; m.albedo[1] = col.g; m.albedo[2] = col.b;
    }

    // Emissive: only true light sources (bulbs, neon, lamps)
    if (s.find("light") != std::string::npos || s.find("lamp") != std::string::npos ||
        s.find("neon") != std::string::npos || s.find("bulb") != std::string::npos ||
        s.find("emiss") != std::string::npos) {
        if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_COLOR_EMISSIVE, col) &&
            (col.r + col.g + col.b) > 0.05f) {
            m.emission[0] = col.r; m.emission[1] = col.g; m.emission[2] = col.b;
            m.type = 3; // EMISSIVE
        }
    }

    // Opacity / Transmission / Glass detection
    float opacity = 1.0f;
    ai_mat->Get(AI_MATKEY_OPACITY, opacity);
    float transmission = 0.0f;
    ai_mat->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission);
    float refracti = 1.52f;
    ai_mat->Get(AI_MATKEY_REFRACTI, refracti);

    // Metallic / roughness factors
    m.metallic  = 0.0f;
    m.roughness = 0.35f;
    if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_METALLIC_FACTOR,  fval)) m.metallic  = fval;
    if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, fval)) m.roughness = fval;

    // Intelligent physical classification based on semantic naming & PBR attributes
    if (m.type != 3) {
        if (opacity < 0.92f || transmission > 0.08f ||
            (s.find("glass") != std::string::npos && s.find("mirror") == std::string::npos) ||
            s.find("window") != std::string::npos ||
            s.find("windshield") != std::string::npos ||
            s.find("lens") != std::string::npos ||
            s.find("lightglass") != std::string::npos ||
            s.find("light_glass") != std::string::npos ||
            s.find("transparent") != std::string::npos) {
            m.type = 2; // GLASS
            m.flags |= MATFLAG_THIN_WALLED; // Thin-walled automotive window glass
            m.refractive_index = (refracti > 1.0f) ? refracti : 1.52f;
            m.roughness = 0.0f;
            m.metallic = 0.0f;
            if (s.find("red") != std::string::npos || s.find("tail") != std::string::npos) {
                m.albedo[0] = 0.88f; m.albedo[1] = 0.04f; m.albedo[2] = 0.04f; // Red taillight glass
            } else if (s.find("amber") != std::string::npos || s.find("turn") != std::string::npos || s.find("orange") != std::string::npos) {
                m.albedo[0] = 0.95f; m.albedo[1] = 0.45f; m.albedo[2] = 0.02f; // Amber turn signal glass
            } else {
                m.albedo[0] = 0.95f; m.albedo[1] = 0.98f; m.albedo[2] = 1.0f; // Clear window glass
            }
        } else if (s.find("glassopaque_mirror") != std::string::npos) {
            m.type = 1; // METAL (Pure Mirror reflection for side mirror glass)
            m.metallic = 1.0f;
            m.roughness = 0.01f;
            m.albedo[0] = 0.95f; m.albedo[1] = 0.95f; m.albedo[2] = 0.95f;
        } else if (s.find("chromes") != std::string::npos || s.find("chrome") != std::string::npos) {
            m.type = 6; // PBR Chrome & Trim (samples Nickel_baseColor palette for black trim + chrome bumper)
            m.metallic = 0.95f;
            m.roughness = 0.05f;
            m.albedo[0] = 1.0f; m.albedo[1] = 1.0f; m.albedo[2] = 1.0f;
        } else if (s.find("nickel") != std::string::npos) {
            m.type = 6; // PBR Nickel / Trim
            m.metallic = 0.40f;
            m.roughness = 0.35f;
            m.albedo[0] = 1.0f; m.albedo[1] = 1.0f; m.albedo[2] = 1.0f;
        } else if (s.find("chassis") != std::string::npos && s.find("color_2") == std::string::npos) {
            m.type = 6; // PBR Chassis / Underbody (Satin black)
            m.metallic = 0.10f;
            m.roughness = 0.60f;
            m.albedo[0] = 0.04f; m.albedo[1] = 0.04f; m.albedo[2] = 0.04f;
        } else if (s.find("color_2") != std::string::npos ||
                   s.find("stripe") != std::string::npos ||
                   s.find("hood") != std::string::npos ||
                   s.find("spoiler") != std::string::npos ||
                   s.find("louver") != std::string::npos) {
            m.type = 6; // PBR Boss 302 Black Graphic / Hood / Spoiler / Louvers
            m.albedo[0] = 0.02f; m.albedo[1] = 0.02f; m.albedo[2] = 0.02f;
            m.roughness = 0.55f;
            m.metallic = 0.0f;
        } else if (s.find("plastic") != std::string::npos ||
                   s.find("trim") != std::string::npos ||
                   s.find("plasr") != std::string::npos ||
                   s.find("plass") != std::string::npos ||
                   s.find("plate") != std::string::npos ||
                   s.find("license") != std::string::npos) {
            m.type = 6; // PBR Plastic / Trim
            m.albedo[0] = 0.025f; m.albedo[1] = 0.025f; m.albedo[2] = 0.025f;
            m.roughness = (s.find("plass") != std::string::npos || s.find("smooth") != std::string::npos) ? 0.35f : 0.70f;
            m.metallic = (s.find("plass") != std::string::npos) ? 0.05f : 0.0f;
        } else if (s.find("interior") != std::string::npos) {
            m.type = 6; // PBR Interior (Cabin / Dashboard / Seats)
            m.roughness = 0.55f;
            m.metallic = 0.0f;
            m.albedo[0] = 1.0f; m.albedo[1] = 1.0f; m.albedo[2] = 1.0f;
        } else if (s.find("engine") != std::string::npos) {
            m.type = 6; // PBR Engine
            m.roughness = 0.35f;
            m.metallic = 0.65f;
            m.albedo[0] = 1.0f; m.albedo[1] = 1.0f; m.albedo[2] = 1.0f;
        } else if (s.find("tire") != std::string::npos ||
                   s.find("rubber") != std::string::npos) {
            m.type = 6; // PBR Tire (Deep matte black rubber)
            m.roughness = 0.85f;
            m.metallic = 0.0f;
            m.albedo[0] = 0.025f; m.albedo[1] = 0.025f; m.albedo[2] = 0.025f;
        } else if (s.find("barrel") != std::string::npos ||
                   s.find("wheel") != std::string::npos ||
                   s.find("rim") != std::string::npos) {
            m.type = 6; // PBR Wheel (Polished chrome lip + gunmetal spokes)
            m.roughness = 0.15f;
            m.metallic = 0.85f;
            m.albedo[0] = 0.70f; m.albedo[1] = 0.70f; m.albedo[2] = 0.70f;
        } else if (s.find("calliper") != std::string::npos || s.find("caliper") != std::string::npos) {
            m.type = 6; // PBR Red Brembo Brake Caliper
            m.roughness = 0.20f;
            m.metallic = 0.30f;
            m.albedo[0] = 0.88f; m.albedo[1] = 0.03f; m.albedo[2] = 0.03f;
        } else if (s.find("brake") != std::string::npos) {
            m.type = 6; // PBR Brake Disc Rotor (Steel / Iron)
            m.roughness = 0.30f;
            m.metallic = 0.85f;
            m.albedo[0] = 0.60f; m.albedo[1] = 0.60f; m.albedo[2] = 0.60f;
        } else if (s.find("badge") != std::string::npos ||
                   s.find("emblem") != std::string::npos ||
                   s.find("logo") != std::string::npos) {
            m.type = 6; // PBR Chrome Running Horse Badge / MUSTANG Emblems
            m.roughness = 0.06f;
            m.metallic = 0.95f;
            m.albedo[0] = 0.95f; m.albedo[1] = 0.95f; m.albedo[2] = 0.95f;
        } else if (s.find("light") != std::string::npos && m.type != 2) {
            m.type = 6; // PBR Headlight Chrome Reflector
            m.roughness = 0.05f;
            m.metallic = 0.95f;
            m.albedo[0] = 0.95f; m.albedo[1] = 0.95f; m.albedo[2] = 0.95f;
        } else if (s.find("carpaint") != std::string::npos ||
                   s.find("paint") != std::string::npos ||
                   s.find("body") != std::string::npos) {
            m.type = 6; // PBR Car Paint (Grabber Blue Boss 302)
            m.roughness = 0.10f; // Glossy clearcoat
            m.metallic  = 0.05f;
            // Iconic 1969 Mustang Grabber Blue
            m.albedo[0] = 0.050f;
            m.albedo[1] = 0.460f;
            m.albedo[2] = 0.900f;
        } else {
            m.type = 6; // PBR
        }
    }

    m.refractive_index = (refracti > 1.0f) ? refracti : 1.5f;
    m.albedo2[0] = m.albedo2[1] = m.albedo2[2] = 0.1f;
    return m;
}

static bool GetBaseColorTexture(const aiMaterial* mat, aiString& out) {
    // glTF 2.0 PBR base color (most common for GLB)
    if (mat->GetTexture(aiTextureType_BASE_COLOR, 0, &out) == AI_SUCCESS) return true;
    // Legacy fallback: traditional diffuse (OBJ, FBX, older glTF)
    if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &out) == AI_SUCCESS) return true;
    // Some Assimp importers surface it under UNKNOWN slot 0
    // (when the material has no other aiTextureType_UNKNOWN textures)
    aiString tmp;
    if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &tmp) == AI_SUCCESS) {
        // Only take it as base color if it doesn't look like an ORM map
        std::string fname = tmp.C_Str();
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
        bool is_orm = (fname.find("orm") != std::string::npos ||
                       fname.find("rough") != std::string::npos ||
                       fname.find("metal") != std::string::npos ||
                       fname.find("occlusion") != std::string::npos ||
                       fname.find("_mr") != std::string::npos ||
                       fname.find("normal") != std::string::npos ||
                       fname.find("norm") != std::string::npos ||
                       fname.find("bump") != std::string::npos);
        if (!is_orm) { out = tmp; return true; }
    }
    // Last resort: ambient/lightmap channel sometimes carries base color
    if (mat->GetTexture(aiTextureType_AMBIENT, 0, &out) == AI_SUCCESS) return true;
    return false;
}

// glTF packs occlusion/roughness/metallic into one image; Assimp surfaces it
// under several keys depending on the importer path.
// Guard strictly against picking up normal/bump/diffuse/specular maps by mistake.
static bool GetORMTexture(const aiMaterial* mat, aiString& out, const char*& which) {
    if (mat->GetTexture(aiTextureType_METALNESS, 0, &out) == AI_SUCCESS)         { which = "METALNESS"; return true; }
    if (mat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &out) == AI_SUCCESS) { which = "ROUGHNESS"; return true; }
    if (mat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &out) == AI_SUCCESS) { which = "OCCLUSION"; return true; }

    aiString normals, basecolor, specular, height;
    bool have_n = mat->GetTexture(aiTextureType_NORMALS, 0, &normals) == AI_SUCCESS;
    bool have_b = GetBaseColorTexture(mat, basecolor);
    bool have_s = mat->GetTexture(aiTextureType_SPECULAR, 0, &specular) == AI_SUCCESS;
    bool have_h = mat->GetTexture(aiTextureType_HEIGHT, 0, &height) == AI_SUCCESS;

    if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &out) == AI_SUCCESS) {
        if (have_n && strcmp(out.C_Str(), normals.C_Str()) == 0)   return false;
        if (have_b && strcmp(out.C_Str(), basecolor.C_Str()) == 0) return false;
        if (have_s && strcmp(out.C_Str(), specular.C_Str()) == 0)  return false;
        if (have_h && strcmp(out.C_Str(), height.C_Str()) == 0)    return false;

        std::string fname = out.C_Str();
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
        if (fname.find("norm") != std::string::npos ||
            fname.find("bump") != std::string::npos ||
            fname.find("ddn")  != std::string::npos ||
            fname.find("diff") != std::string::npos ||
            fname.find("albedo") != std::string::npos ||
            fname.find("color") != std::string::npos ||
            fname.find("base")  != std::string::npos ||
            fname.find("spec") != std::string::npos) {
            return false;
        }

        if (fname.find("orm") != std::string::npos ||
            fname.find("rough") != std::string::npos ||
            fname.find("metal") != std::string::npos ||
            fname.find("metallic") != std::string::npos ||
            fname.find("occlusion") != std::string::npos ||
            fname.find("_mr") != std::string::npos) {
            which = "UNKNOWN_ORM";
            return true;
        }
        return false;
    }
    return false;
}

static bool GetNormalTexture(const aiMaterial* mat, aiString& out) {
    // Proper normal map slot (glTF, FBX)
    if (mat->GetTexture(aiTextureType_NORMALS, 0, &out) == AI_SUCCESS) return true;
    // Some exporters store normals as HEIGHT (OBJ/MTL convention)
    if (mat->GetTexture(aiTextureType_HEIGHT, 0, &out) == AI_SUCCESS) return true;
    // UNKNOWN slot: accept only if name contains "norm" or "bump"
    aiString tmp;
    if (mat->GetTexture(aiTextureType_UNKNOWN, 0, &tmp) == AI_SUCCESS) {
        std::string fname = tmp.C_Str();
        std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
        if (fname.find("norm") != std::string::npos ||
            fname.find("bump") != std::string::npos ||
            fname.find("ddn")  != std::string::npos ||
            fname.find("nrm")  != std::string::npos) {
            out = tmp; return true;
        }
    }
    return false;
}

void BuildBVH(std::vector<GPUTriangle>& tris,
              std::vector<GPUBVHNode>&  nodes,
              int start, int count, int depth) {
    using Scalar = float;
    using Vec3 = bvh::v2::Vec<Scalar, 3>;
    using Bbox = bvh::v2::BBox<Scalar, 3>;
    using Tri = bvh::v2::Tri<Scalar, 3>;

    // Note: the builder only consumes bounding boxes and centroids. An array of
  // bvh::Tri used to be built alongside them and never read - for Sponza4k that
  // was 207 MB allocated and thrown away.
  std::vector<Bbox> bboxes;
  std::vector<Vec3> centers;
  bboxes.reserve(count);
  centers.reserve(count);

  for (int i = start; i < start + count; i++) {
    const auto &t = tris[i];
    Vec3 v0(t.v0[0], t.v0[1], t.v0[2]);
    Vec3 v1(t.v1[0], t.v1[1], t.v1[2]);
    Vec3 v2(t.v2[0], t.v2[1], t.v2[2]);

    Bbox bbox(v0);
    bbox.extend(v1);
    bbox.extend(v2);
    bboxes.push_back(bbox);
    centers.push_back(bbox.get_center());
  }

  // Parallel build. The single-threaded binned-SAH builder took 7.4 s of the
  // 23 s it costs to load Sponza4k's 5.7 M triangles, and it was the single
  // largest phase. The library's DefaultBuilder splits the work across a thread
  // pool; Quality::Low keeps the same binned-SAH heuristic the GPU traversal was
  // tuned against, so only the build is different, not the resulting tree shape.
  using BvhNode = bvh::v2::Node<Scalar, 3>;
  bvh::v2::ThreadPool thread_pool;
  typename bvh::v2::DefaultBuilder<BvhNode>::Config bvh_config;
  bvh_config.quality = bvh::v2::DefaultBuilder<BvhNode>::Quality::Low;
  bvh::v2::Bvh<BvhNode> bvh = bvh::v2::DefaultBuilder<BvhNode>::build(
      thread_pool, bboxes, centers, bvh_config);

  // The builder's scratch is dead once it returns; release it before touching
  // the triangle array again so the two peaks do not overlap.
  std::vector<Bbox>().swap(bboxes);
  std::vector<Vec3>().swap(centers);

  // Apply the BVH's ordering to the triangles in place, by walking the
  // permutation's cycles. Materialising a reordered copy instead cost a second
  // full triangle array - 735 MB on Sponza4k, at the point of peak usage.
  {
    std::vector<bool> done(count, false);
    for (size_t i = 0; i < (size_t)count; i++) {
      if (done[i]) continue;
      size_t j = i;
      GPUTriangle held = tris[start + i];
      for (;;) {
        size_t src = bvh.prim_ids[j];
        done[j] = true;
        if (src == i) {          // cycle closes
          tris[start + j] = held;
          break;
        }
        tris[start + j] = tris[start + src];
        j = src;
      }
    }
  }

  nodes.resize(bvh.nodes.size());
    for (size_t i = 0; i < bvh.nodes.size(); i++) {
        const auto& n = bvh.nodes[i];
        GPUBVHNode gn;
        gn.aabb_min[0] = n.bounds[0];
        gn.aabb_min[1] = n.bounds[2];
        gn.aabb_min[2] = n.bounds[4];
        gn.aabb_max[0] = n.bounds[1];
        gn.aabb_max[1] = n.bounds[3];
        gn.aabb_max[2] = n.bounds[5];
        
        if (n.is_leaf()) {
            gn.left_or_tri = start + n.index.first_id();
            gn.right_or_count = -((int)n.index.prim_count());
        } else {
            gn.left_or_tri = n.index.first_id();
            gn.right_or_count = n.index.first_id() + 1; // Assuming contiguous children
        }
        nodes[i] = gn;
    }
}

// --------------------------------------------------------------- public API --

MeshData LoadModel(const std::string& path, float target_size) {
  // Phase timings: a multi-second load should say where the time went rather
  // than look like a freeze.
  using clock = std::chrono::steady_clock;
  auto t_start = clock::now();
  auto phase = [&](const char *what) {
    auto now = clock::now();
    std::cout << "[ModelLoader] " << what << ": "
              << std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count()
              << " ms" << std::endl;
    t_start = now;
  };
    MeshData result;

    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_CAMERAS | aiComponent_LIGHTS | aiComponent_ANIMATIONS);
    importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 66.0f);

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate           |
        aiProcess_GenNormals            | // preserves author normals
        aiProcess_CalcTangentSpace      | // generates tangents+bitangents for normal maps
        aiProcess_FlipUVs               | // Metal texture space
        aiProcess_JoinIdenticalVertices |
        aiProcess_PreTransformVertices);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[ModelLoader] Failed to load: " << path
                  << "\n  " << importer.GetErrorString() << std::endl;
        return result;
    }

    phase("parse");

  std::string base_dir = std::filesystem::path(path).parent_path().string();
    if (!base_dir.empty()) base_dir += "/";

    size_t num_mats = scene->mNumMaterials;
    if (num_mats == 0) num_mats = 1;

    // Probe the source images to size the arrays. Decoding headers only is cheap
    // compared with decoding pixels, so this costs almost nothing and avoids
    // both upscaling small textures and downscaling 4K ones further than needed.
    int max_src = 0;
    for (unsigned int mi = 0; mi < scene->mNumMaterials; mi++) {
        aiString tp;
        if (!GetBaseColorTexture(scene->mMaterials[mi], tp)) continue;
        int w = 0, h = 0, c = 0;
        if (tp.data[0] == '*') {
            int idx = atoi(&tp.data[1]);
            if (idx >= 0 && idx < (int)scene->mNumTextures) {
                aiTexture* tex = scene->mTextures[idx];
                if (tex->mHeight == 0)
                    stbi_info_from_memory((const stbi_uc*)tex->pcData, tex->mWidth, &w, &h, &c);
                else { w = tex->mWidth; h = tex->mHeight; }
            }
        } else {
            stbi_info((base_dir + tp.C_Str()).c_str(), &w, &h, &c);
        }
        max_src = std::max({max_src, w, h});
    }
    auto pow2_at_most = [](int v, int cap) {
        int p = 256;
        while (p * 2 <= v && p * 2 <= cap) p *= 2;
        return std::min(p, cap);
    };
    const int tex_size  = (max_src > 0) ? pow2_at_most(max_src, kMeshTexSizeMax) : 256;
    const int orm_size  = std::min(tex_size, kMeshOrmSizeMax);
    const int norm_size = std::min(tex_size, kMeshNormSizeMax);

    const size_t slice_texels = (size_t)tex_size  * tex_size;
    const size_t orm_texels   = (size_t)orm_size   * orm_size;
    const size_t norm_texels  = (size_t)norm_size  * norm_size;
    result.tex_size  = tex_size;
    result.orm_size  = orm_size;
    result.norm_size = norm_size;
    result.texture_array_data.resize(num_mats * slice_texels * 4, 255); // white default
    // Default ORM: fully rough, fully dielectric.
    result.orm_array_data.assign(num_mats * orm_texels * 4, 0);
    for (size_t i = 0; i < num_mats * orm_texels; i++) {
        result.orm_array_data[i*4+0] = 255;  // occlusion
        result.orm_array_data[i*4+1] = 255;  // roughness
        result.orm_array_data[i*4+2] = 0;    // metallic
        result.orm_array_data[i*4+3] = 255;
    }
    // Default normal map: flat (0.5, 0.5, 1.0) = pointing straight up in tangent space.
    result.norm_array_data.assign(num_mats * norm_texels * 4, 0);
    for (size_t i = 0; i < num_mats * norm_texels; i++) {
        result.norm_array_data[i*4+0] = 128;  // X = 0
        result.norm_array_data[i*4+1] = 128;  // Y = 0
        result.norm_array_data[i*4+2] = 255;  // Z = +1
        result.norm_array_data[i*4+3] = 255;
    }

    const char* dump_dir = getenv("RT_DUMP_TEXTURES");
    std::cout << "[ModelLoader] Texture arrays: base " << tex_size << "^2, ORM "
              << orm_size << "^2, Norm " << norm_size << "^2 (largest source " << max_src << ")\n";

    result.materials.resize(scene->mNumMaterials);

    // Build semantic names for each material index from material names, mesh names, node names, and textures
    std::vector<std::string> mat_semantic_names(scene->mNumMaterials);
    for (unsigned int mi = 0; mi < scene->mNumMaterials; mi++) {
        const aiMaterial* ai_mat = scene->mMaterials[mi];
        aiString name;
        if (AI_SUCCESS == ai_mat->Get(AI_MATKEY_NAME, name)) {
            mat_semantic_names[mi] += name.C_Str();
            mat_semantic_names[mi] += " ";
        }
        aiString tex_path;
        if (GetBaseColorTexture(ai_mat, tex_path)) {
            mat_semantic_names[mi] += tex_path.C_Str();
            mat_semantic_names[mi] += " ";
        }
        aiString norm_path;
        if (GetNormalTexture(ai_mat, norm_path)) {
            mat_semantic_names[mi] += norm_path.C_Str();
            mat_semantic_names[mi] += " ";
        }
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        unsigned int mat_idx = mesh->mMaterialIndex;
        if (mat_idx < mat_semantic_names.size()) {
            mat_semantic_names[mat_idx] += mesh->mName.C_Str();
            mat_semantic_names[mat_idx] += " ";
        }
    }

    std::function<void(const aiNode*)> collectNodeNames = [&](const aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            unsigned int mesh_idx = node->mMeshes[i];
            if (mesh_idx < scene->mNumMeshes) {
                unsigned int mat_idx = scene->mMeshes[mesh_idx]->mMaterialIndex;
                if (mat_idx < mat_semantic_names.size()) {
                    mat_semantic_names[mat_idx] += node->mName.C_Str();
                    mat_semantic_names[mat_idx] += " ";
                }
            }
        }
        for (unsigned int c = 0; c < node->mNumChildren; c++) {
            collectNodeNames(node->mChildren[c]);
        }
    };
    if (scene->mRootNode) collectNodeNames(scene->mRootNode);

    for (auto& s : mat_semantic_names) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    }

    // Decoding and resampling 4K source maps is the bulk of the load time for a
    // file like Sponza4k.glb, and every material is independent, so fan it out.
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned nthreads = std::min<unsigned>(hw, scene->mNumMaterials);
    std::atomic<unsigned> next_mat{0};
    std::mutex log_mutex;

    // Decodes one texture reference, embedded (*N) or on disk, to RGBA8.
    auto decode_texture = [&](const aiString& tex_path, int& tw, int& th) -> uint8_t* {
        int tc = 0;
        uint8_t* pixels = nullptr;
        // Embedded textures (GLB) are referenced as *0, *1, ... Check those
        // first: for a GLB the on-disk path never resolves, and probing the
        // filesystem for every material is wasted I/O.
        if (tex_path.data[0] == '*') {
            int idx = atoi(&tex_path.data[1]);
            if (idx >= 0 && idx < (int)scene->mNumTextures) {
                aiTexture* tex = scene->mTextures[idx];
                if (tex->mHeight == 0) { // compressed blob
                    pixels = stbi_load_from_memory((const stbi_uc*)tex->pcData,
                                                   tex->mWidth, &tw, &th, &tc, 4);
                } else {                 // raw ARGB8888
                    tw = tex->mWidth; th = tex->mHeight;
                    pixels = (uint8_t*)stbi__malloc((size_t)tw * th * 4);
                    if (pixels) {
                        for (size_t p = 0; p < (size_t)tw * th; p++) {
                            pixels[p*4+0] = tex->pcData[p].r;
                            pixels[p*4+1] = tex->pcData[p].g;
                            pixels[p*4+2] = tex->pcData[p].b;
                            pixels[p*4+3] = tex->pcData[p].a;
                        }
                    }
                }
            }
        }
        if (!pixels) pixels = stbi_load((base_dir + tex_path.C_Str()).c_str(), &tw, &th, &tc, 4);
        return pixels;
    };

    auto worker = [&]() {
        for (;;) {
            unsigned mi = next_mat.fetch_add(1);
            if (mi >= scene->mNumMaterials) return;

            const aiMaterial* ai_mat = scene->mMaterials[mi];
            const std::string& semantic_name = mat_semantic_names[mi];
            GPUMaterial gm = ConvertMaterial(ai_mat, semantic_name);

            uint8_t* dst     = result.texture_array_data.data() + (size_t)mi * slice_texels * 4;
            uint8_t* orm_dst = result.orm_array_data.data()     + (size_t)mi * orm_texels * 4;

            // ---- base colour
            aiString tex_path;
            bool load_bc_tex = GetBaseColorTexture(ai_mat, tex_path);
            if (load_bc_tex) {
                std::string tp = tex_path.C_Str();
                std::transform(tp.begin(), tp.end(), tp.begin(), ::tolower);
                // Reject invalid texture assignments from GLTF exporter
                if (gm.type == 1 || gm.type == 2 || semantic_name.find("glass") != std::string::npos || semantic_name.find("window") != std::string::npos) {
                    load_bc_tex = false;
                } else if (mi == 0 || (semantic_name.find("badge") != std::string::npos && mi != 9 && mi != 10)) {
                    load_bc_tex = false;
                } else if (semantic_name.find("light") != std::string::npos && gm.type != 2) {
                    load_bc_tex = false;
                } else if (semantic_name.find("tire") != std::string::npos || semantic_name.find("barrel") != std::string::npos || semantic_name.find("wheel") != std::string::npos || semantic_name.find("brake") != std::string::npos || semantic_name.find("caliper") != std::string::npos) {
                    load_bc_tex = false;
                } else if (semantic_name.find("chassis") != std::string::npos && semantic_name.find("color_2") == std::string::npos) {
                    load_bc_tex = false;
                } else if ((semantic_name.find("plas") != std::string::npos || semantic_name.find("trim") != std::string::npos || semantic_name.find("plate") != std::string::npos) && (tp == "*2" || tp.find("nickel") != std::string::npos)) {
                    load_bc_tex = false;
                } else if ((semantic_name.find("carpaint") != std::string::npos || semantic_name.find("color_2") != std::string::npos) && (tp == "*2" || tp.find("nickel") != std::string::npos)) {
                    load_bc_tex = false;
                }
            }
            if (load_bc_tex) {
                int tw = 0, th = 0;
                uint8_t* pixels = decode_texture(tex_path, tw, th);
                if (pixels) {
                    ResizeBox(pixels, tw, th, dst, tex_size, tex_size);
                    stbi_image_free(pixels);
                    gm.flags |= MATFLAG_HAS_BASECOLOR_TEX;
                } else {
                    std::lock_guard<std::mutex> lk(log_mutex);
                    std::cerr << "[ModelLoader] Warning: failed to load base colour for material "
                              << mi << " (" << tex_path.C_Str() << ")" << std::endl;
                }
            }
            if (!(gm.flags & MATFLAG_HAS_BASECOLOR_TEX)) {
                // Flat albedo slice, so the shader can sample uniformly.
                uint8_t r = (uint8_t)(std::clamp(gm.albedo[0], 0.0f, 1.0f) * 255.0f);
                uint8_t g = (uint8_t)(std::clamp(gm.albedo[1], 0.0f, 1.0f) * 255.0f);
                uint8_t b = (uint8_t)(std::clamp(gm.albedo[2], 0.0f, 1.0f) * 255.0f);
                uint8_t a = (gm.type == 2) ? 0 : 255;
                for (size_t i = 0; i < slice_texels; i++) {
                    dst[i*4+0] = r; dst[i*4+1] = g; dst[i*4+2] = b; dst[i*4+3] = a;
                }
            }

            // ---- occlusion / roughness / metallic
            aiString orm_path;
            const char* orm_kind = "none";
            if (load_bc_tex && GetORMTexture(ai_mat, orm_path, orm_kind)) {
                int tw = 0, th = 0;
                uint8_t* pixels = decode_texture(orm_path, tw, th);
                if (pixels) {
                    ResizeBox(pixels, tw, th, orm_dst, orm_size, orm_size);
                    stbi_image_free(pixels);
                    gm.flags |= MATFLAG_HAS_ORM_TEX;
                }
            }
            if (!(gm.flags & MATFLAG_HAS_ORM_TEX)) {
                // If a texture wasn't found for ORM, fill with scalar properties.
                // For metals: fully metallic, polished roughness.
                if (gm.type == 1) {
                    gm.metallic = 1.0f; gm.roughness = 0.02f;
                }
                uint8_t rough = (uint8_t)(std::clamp(gm.roughness, 0.0f, 1.0f) * 255.0f);
                uint8_t metal = (uint8_t)(std::clamp(gm.metallic,  0.0f, 1.0f) * 255.0f);
                for (size_t i = 0; i < orm_texels; i++) {
                    orm_dst[i*4+0] = 255; orm_dst[i*4+1] = rough;
                    orm_dst[i*4+2] = metal; orm_dst[i*4+3] = 255;
                }
            }

            // ---- tangent-space normal map
            uint8_t* norm_dst = result.norm_array_data.data() + (size_t)mi * norm_texels * 4;
            if (semantic_name.find("tire") == std::string::npos &&
                semantic_name.find("barrel") == std::string::npos &&
                semantic_name.find("wheel") == std::string::npos &&
                semantic_name.find("brake") == std::string::npos &&
                semantic_name.find("caliper") == std::string::npos) {
                aiString norm_path;
                if (GetNormalTexture(ai_mat, norm_path)) {
                    int tw = 0, th = 0;
                    uint8_t* pixels = decode_texture(norm_path, tw, th);
                    if (pixels) {
                        ResizeBox(pixels, tw, th, norm_dst, norm_size, norm_size);
                        stbi_image_free(pixels);
                        gm.flags |= MATFLAG_HAS_NORMAL_TEX;
                    }
                }
            }
            // If no normal map was found the default flat (128,128,255) is already in place.

            double br=0, bg=0, bb=0, ba=0, bopaque=0, wr=0, wg=0, wb=0, wsum=0;
            double min_alpha = 1.0;
            double num_transparent = 0;
            for (size_t i = 0; i < slice_texels; i++) {
                br += dst[i*4+0]; bg += dst[i*4+1]; bb += dst[i*4+2]; ba += dst[i*4+3];
                uint8_t a = dst[i*4+3];
                if (a < 250) {
                    num_transparent += 1.0;
                    if (a / 255.0 < min_alpha) min_alpha = a / 255.0;
                }
                { double aw = a / 255.0;
                  wr += dst[i*4+0] * aw; wg += dst[i*4+1] * aw; wb += dst[i*4+2] * aw; wsum += aw; }
            }
            br /= slice_texels*255.0; bg /= slice_texels*255.0; bb /= slice_texels*255.0;
            ba /= slice_texels*255.0; bopaque = num_transparent / (double)slice_texels;
            if (wsum > 0) { wr /= wsum*255.0; wg /= wsum*255.0; wb /= wsum*255.0; }

            // Decide once, from the data, whether this material is see-through.
            bool is_decal = (mi == 9 || mi == 10);
            if (is_decal && bopaque > 0.001 && gm.type != 2) gm.flags |= MATFLAG_ALPHA_BLEND;
            result.materials[mi] = gm;

            double ar=0, ag=0, ab=0;
            for (size_t i = 0; i < orm_texels; i++) {
                ar += orm_dst[i*4+0]; ag += orm_dst[i*4+1]; ab += orm_dst[i*4+2];
            }
            ar /= orm_texels*255.0; ag /= orm_texels*255.0; ab /= orm_texels*255.0;

            std::lock_guard<std::mutex> lk(log_mutex);
            std::cout << "[ModelLoader] Material " << mi << " (" << semantic_name.substr(0, 35) << ")"
                      << ": basecolor=" << ((gm.flags & MATFLAG_HAS_BASECOLOR_TEX) ? "tex" : "flat")
                      << " orm=" << ((gm.flags & MATFLAG_HAS_ORM_TEX) ? orm_kind : "flat")
                      << " metallic=" << gm.metallic
                      << " roughness=" << gm.roughness << " TYPE=" << gm.type
                      << "  | base avg=" << br << "," << bg << "," << bb
                      << "  visible_rgb=" << wr << "," << wg << "," << wb
                      << "  alpha_avg=" << ba << " frac_transparent=" << bopaque
                      << "  factor=" << gm.albedo[0] << "," << gm.albedo[1] << "," << gm.albedo[2]
                      << "  | ORM avg=" << ar << "," << ag << "," << ab << "\n";
        }
    };

    {
        std::vector<std::thread> pool;
        for (unsigned t = 1; t < nthreads; t++) pool.emplace_back(worker);
        worker();
        for (auto& t : pool) t.join();
    }

    // Ensure at least one material
    if (result.materials.empty()) {
        GPUMaterial def{}; def.albedo[0]=def.albedo[1]=def.albedo[2]=0.8f;
        def.roughness=0.5f; def.type=6; result.materials.push_back(def);
        
        uint8_t* dst = result.texture_array_data.data();
        for (int i = 0; i < tex_size * tex_size; i++) {
            dst[i*4+0] = dst[i*4+1] = dst[i*4+2] = 204;
            dst[i*4+3] = 255;
        }
    }

    phase("textures");

  // Traverse all meshes
    std::function<void(aiNode*)> traverse = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            int mat_idx = (int)mesh->mMaterialIndex;
            if (mat_idx >= (int)result.materials.size()) mat_idx = 0;

            ModelSubmesh submesh{};
            std::string sname = (mesh->mName.length > 0) ? mesh->mName.C_Str() : node->mName.C_Str();
            if (sname.empty() || sname == "RootNode") sname = "part_" + std::to_string(result.submeshes.size() + 1);
            submesh.name = sname;
            submesh.tri_start = (int)result.triangles.size();
            submesh.mat_index = mat_idx;

            aiVector3D m_min(1e9f, 1e9f, 1e9f), m_max(-1e9f, -1e9f, -1e9f);
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                m_min.x = std::min(m_min.x, mesh->mVertices[v].x);
                m_min.y = std::min(m_min.y, mesh->mVertices[v].y);
                m_min.z = std::min(m_min.z, mesh->mVertices[v].z);
                m_max.x = std::max(m_max.x, mesh->mVertices[v].x);
                m_max.y = std::max(m_max.y, mesh->mVertices[v].y);
                m_max.z = std::max(m_max.z, mesh->mVertices[v].z);
            }
            aiVector3D m_ext = m_max - m_min;
            aiVector3D m_cnt = (m_min + m_max) * 0.5f;
            std::string s_lower = sname;
            std::transform(s_lower.begin(), s_lower.end(), s_lower.begin(), ::tolower);
            bool is_wheel_part = s_lower.find("tire") != std::string::npos ||
                                 s_lower.find("wheel") != std::string::npos ||
                                 s_lower.find("barrel") != std::string::npos ||
                                 s_lower.find("brake") != std::string::npos;

            for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;

                GPUTriangle tri{};
                for (int v = 0; v < 3; v++) {
                    unsigned int idx = face.mIndices[v];
                    float* dst_v = (v == 0) ? tri.v0 : (v == 1) ? tri.v1 : tri.v2;
                    float* dst_n = (v == 0) ? tri.n0 : (v == 1) ? tri.n1 : tri.n2;

                    dst_v[0] = mesh->mVertices[idx].x;
                    dst_v[1] = mesh->mVertices[idx].y;
                    dst_v[2] = mesh->mVertices[idx].z;

                    if (mesh->HasNormals()) {
                        dst_n[0] = mesh->mNormals[idx].x;
                        dst_n[1] = mesh->mNormals[idx].y;
                        dst_n[2] = mesh->mNormals[idx].z;
                    } else {
                        dst_n[0] = 0; dst_n[1] = 1; dst_n[2] = 0;
                    }

                    float* dst_uv = (v == 0) ? tri.uv0 : (v == 1) ? tri.uv1 : tri.uv2;
                    if (mesh->HasTextureCoords(0)) {
                        dst_uv[0] = mesh->mTextureCoords[0][idx].x;
                        dst_uv[1] = mesh->mTextureCoords[0][idx].y;
                    } else if (s_lower.find("lightglass") != std::string::npos || s_lower.find("light_glass") != std::string::npos) {
                        float lx = (mesh->mVertices[idx].x - m_cnt.x) / std::max(m_ext.x, 1e-4f);
                        float ly = (mesh->mVertices[idx].y - m_cnt.y) / std::max(m_ext.y, 1e-4f);
                        if (mesh->mVertices[idx].z > 0.0f) {
                            // Front headlight circular Fresnel fluting lens
                            dst_uv[0] = std::clamp(0.20f + lx * 0.16f, 0.04f, 0.36f);
                            dst_uv[1] = std::clamp(0.23f + ly * 0.16f, 0.04f, 0.38f);
                        } else {
                            // Rear taillight vertical 3-bar fluting lens
                            dst_uv[0] = std::clamp(0.25f + lx * 0.35f, 0.06f, 0.68f);
                            dst_uv[1] = std::clamp(0.75f + ly * 0.18f, 0.56f, 0.94f);
                        }
                    } else if (s_lower.find("badge") != std::string::npos) {
                        float lx = (mesh->mVertices[idx].x - m_cnt.x) / std::max(m_ext.x, 1e-4f);
                        float ly = (mesh->mVertices[idx].y - m_cnt.y) / std::max(m_ext.y, 1e-4f);
                        if (mesh->mVertices[idx].z > 0.0f) {
                            // Front running horse badge
                            dst_uv[0] = std::clamp(0.20f + lx * 0.15f, 0.04f, 0.35f);
                            dst_uv[1] = std::clamp(0.50f + ly * 0.35f, 0.12f, 0.88f);
                        } else {
                            // Rear MUSTANG chrome letters
                            dst_uv[0] = std::clamp(0.50f + lx * 0.45f, 0.05f, 0.95f);
                            dst_uv[1] = std::clamp(0.85f + ly * 0.10f, 0.76f, 0.96f);
                        }
                    } else {
                        dst_uv[0] = 0.0f;
                        dst_uv[1] = 0.0f;
                    }
                }
                tri.mat_index = mat_idx;
                result.triangles.push_back(tri);
            }
            submesh.tri_count = (int)result.triangles.size() - submesh.tri_start;
            if (submesh.tri_count > 0) result.submeshes.push_back(submesh);
        }
        for (unsigned int c = 0; c < node->mNumChildren; c++)
            traverse(node->mChildren[c]);
    };
    traverse(scene->mRootNode);

    if (result.triangles.empty()) {
        std::cerr << "[ModelLoader] No triangles found in: " << path << std::endl;
        return result;
    }

    // Optional: write every loaded slice to disk, for inspecting what actually
    // came out of the importer rather than inferring it from the render.
    if (dump_dir) {
        for (size_t mi = 0; mi < result.materials.size(); mi++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/base_%02zu.png", dump_dir, mi);
            stbi_write_png(path, tex_size, tex_size, 4,
                           result.texture_array_data.data() + mi * slice_texels * 4, tex_size * 4);
        }
        std::cout << "[ModelLoader] dumped " << result.materials.size()
                  << " base-colour slices to " << dump_dir << "\n";
    }

    // Compute mesh AABB
    float mn[3] = {1e30f, 1e30f, 1e30f};
    float mx[3] = {-1e30f,-1e30f,-1e30f};
    for (auto& t : result.triangles) {
        for (int k = 0; k < 3; k++) {
            mn[k] = std::min({mn[k], t.v0[k], t.v1[k], t.v2[k]});
            mx[k] = std::max({mx[k], t.v0[k], t.v1[k], t.v2[k]});
        }
    }

    float cx = (mn[0] + mx[0]) * 0.5f;
    float cy = (mn[1] + mx[1]) * 0.5f;
    float cz = (mn[2] + mx[2]) * 0.5f;
    float max_dim = std::max({mx[0]-mn[0], mx[1]-mn[1], mx[2]-mn[2]});
    float scale = (target_size > 0.0f && max_dim > 1e-6f) ? (target_size / max_dim) : 1.0f;

    std::cout << "[ModelLoader] " << result.triangles.size() << " tris, "
              << result.materials.size() << " materials. Scale: " << scale << std::endl;

    // Centre horizontally and scale, seating the model on y = 0
    const float centre[3] = {cx, cy, cz};
    for (auto& t : result.triangles) {
        for (int k = 0; k < 3; k++) {
            t.v0[k] = (t.v0[k] - centre[k]) * scale;
            t.v1[k] = (t.v1[k] - centre[k]) * scale;
            t.v2[k] = (t.v2[k] - centre[k]) * scale;
        }
    }
    const float base_y = (mn[1] - cy) * scale;
    for (auto& t : result.triangles) {
        t.v0[1] -= base_y; t.v1[1] -= base_y; t.v2[1] -= base_y;
    }

    result.aabb_min = Vec3((mn[0]-cx)*scale, 0.0f,                (mn[2]-cz)*scale);
    result.aabb_max = Vec3((mx[0]-cx)*scale, (mx[1]-mn[1])*scale, (mx[2]-cz)*scale);

    for (auto& sm : result.submeshes) {
        if (sm.tri_count <= 0) continue;
        float smn[3] = { 1e30f,  1e30f,  1e30f};
        float smx[3] = {-1e30f, -1e30f, -1e30f};
        for (int ti = sm.tri_start; ti < sm.tri_start + sm.tri_count; ti++) {
            const auto& t = result.triangles[ti];
            for (int k = 0; k < 3; k++) {
                smn[k] = std::min({smn[k], t.v0[k], t.v1[k], t.v2[k]});
                smx[k] = std::max({smx[k], t.v0[k], t.v1[k], t.v2[k]});
            }
        }
        sm.aabb_min = Vec3(smn[0], smn[1], smn[2]);
        sm.aabb_max = Vec3(smx[0], smx[1], smx[2]);
    }

    // The aiScene (and, for a GLB, its embedded texture blobs) is the largest
    // live allocation at this point and nothing below needs it. Releasing it
    // before the BVH build keeps the two peaks from overlapping.
    phase("geometry");
  importer.FreeScene();

    // Build flat BVH
    BuildBVH(result.triangles, result.bvh_nodes, 0, (int)result.triangles.size());
    // Split into the two arrays the GPU actually wants. Done after the BVH has
  // permuted the triangles, so both stay in traversal order.
  result.tri_pos.resize(result.triangles.size());
  result.tri_attr.resize(result.triangles.size());
  for (size_t i = 0; i < result.triangles.size(); i++) {
    const GPUTriangle &t = result.triangles[i];
    GPUTriPos &p = result.tri_pos[i];
    GPUTriAttr &a = result.tri_attr[i];
    for (int k = 0; k < 3; k++) {
      p.v0[k] = t.v0[k];
      p.e1[k] = t.v1[k] - t.v0[k];
      p.e2[k] = t.v2[k] - t.v0[k];
      a.n0[k] = t.n0[k];
      a.n1[k] = t.n1[k];
      a.n2[k] = t.n2[k];
    }
    a.uv0[0] = t.uv0[0]; a.uv0[1] = t.uv0[1];
    a.uv1[0] = t.uv1[0]; a.uv1[1] = t.uv1[1];
    a.uv2[0] = t.uv2[0]; a.uv2[1] = t.uv2[1];
    a.mat_index = t.mat_index;
    p.pad0 = p.pad1 = p.pad2 = 0.0f;
    a.pad0 = a.pad1 = a.pad2 = a.pad3 = 0.0f;
  }
  // The fat build-time array is dead now; on Sponza4k it is 735 MB.
  std::vector<GPUTriangle>().swap(result.triangles);

  phase("bvh");
  std::cout << "[ModelLoader] BVH: " << result.bvh_nodes.size() << " nodes" << std::endl;

    result.valid = true;
    return result;
}

} // namespace lucida
