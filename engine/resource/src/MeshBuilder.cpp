// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/MeshBuilder.h"
#include "lucida/resource/ModelLoader.h" // BuildBVH

#include <cmath>
#include <algorithm>
#include <map>

namespace lucida {

void EditableMesh::Translate(const Vec3& offset) {
    for (auto& v : vertices) {
        v.position += offset;
    }
}

void EditableMesh::Scale(const Vec3& scale) {
    for (auto& v : vertices) {
        v.position *= scale;
    }
}

void EditableMesh::RotateX(float angle_radians) {
    const float c = std::cos(angle_radians);
    const float s = std::sin(angle_radians);
    for (auto& v : vertices) {
        float y = v.position.y * c - v.position.z * s;
        float z = v.position.y * s + v.position.z * c;
        v.position.y = y;
        v.position.z = z;

        float ny = v.normal.y * c - v.normal.z * s;
        float nz = v.normal.y * s + v.normal.z * c;
        v.normal.y = ny;
        v.normal.z = nz;
    }
}

void EditableMesh::RotateY(float angle_radians) {
    const float c = std::cos(angle_radians);
    const float s = std::sin(angle_radians);
    for (auto& v : vertices) {
        float x = v.position.x * c + v.position.z * s;
        float z = -v.position.x * s + v.position.z * c;
        v.position.x = x;
        v.position.z = z;

        float nx = v.normal.x * c + v.normal.z * s;
        float nz = -v.normal.x * s + v.normal.z * c;
        v.normal.x = nx;
        v.normal.z = nz;
    }
}

void EditableMesh::RotateZ(float angle_radians) {
    const float c = std::cos(angle_radians);
    const float s = std::sin(angle_radians);
    for (auto& v : vertices) {
        float x = v.position.x * c - v.position.y * s;
        float y = v.position.x * s + v.position.y * c;
        v.position.x = x;
        v.position.y = y;

        float nx = v.normal.x * c - v.normal.y * s;
        float ny = v.normal.x * s + v.normal.y * c;
        v.normal.x = nx;
        v.normal.y = ny;
    }
}

void EditableMesh::RecalculateNormals(bool smooth) {
    if (vertices.empty() || faces.empty()) return;

    if (smooth) {
        for (auto& v : vertices) {
            v.normal = Vec3(0.0f);
        }
        for (const auto& f : faces) {
            if (f.i0 >= vertices.size() || f.i1 >= vertices.size() || f.i2 >= vertices.size()) continue;
            const Vec3& p0 = vertices[f.i0].position;
            const Vec3& p1 = vertices[f.i1].position;
            const Vec3& p2 = vertices[f.i2].position;
            const Vec3 fn = glm::cross(p1 - p0, p2 - p0);
            vertices[f.i0].normal += fn;
            vertices[f.i1].normal += fn;
            vertices[f.i2].normal += fn;
        }
        for (auto& v : vertices) {
            if (glm::length(v.normal) > 1e-6f) {
                v.normal = glm::normalize(v.normal);
            } else {
                v.normal = Vec3(0.0f, 1.0f, 0.0f);
            }
        }
    } else {
        // Flat shading: duplicate vertices per face
        std::vector<Vertex> new_verts;
        std::vector<TriangleFace> new_faces;
        new_verts.reserve(faces.size() * 3);
        new_faces.reserve(faces.size());

        for (const auto& f : faces) {
            if (f.i0 >= vertices.size() || f.i1 >= vertices.size() || f.i2 >= vertices.size()) continue;
            Vertex v0 = vertices[f.i0];
            Vertex v1 = vertices[f.i1];
            Vertex v2 = vertices[f.i2];
            Vec3 fn = glm::cross(v1.position - v0.position, v2.position - v0.position);
            if (glm::length(fn) > 1e-6f) fn = glm::normalize(fn);
            else fn = Vec3(0.0f, 1.0f, 0.0f);

            v0.normal = fn;
            v1.normal = fn;
            v2.normal = fn;

            uint32_t idx = static_cast<uint32_t>(new_verts.size());
            new_verts.push_back(v0);
            new_verts.push_back(v1);
            new_verts.push_back(v2);

            new_faces.push_back({idx, idx + 1, idx + 2, f.material_index});
        }
        vertices = std::move(new_verts);
        faces = std::move(new_faces);
    }
}

void EditableMesh::Subdivide() {
    if (faces.empty()) return;

    std::vector<TriangleFace> new_faces;
    new_faces.reserve(faces.size() * 4);

    auto get_midpoint = [this](uint32_t i0, uint32_t i1, std::map<std::pair<uint32_t, uint32_t>, uint32_t>& mid_cache) -> uint32_t {
        if (i0 > i1) std::swap(i0, i1);
        auto key = std::make_pair(i0, i1);
        auto it = mid_cache.find(key);
        if (it != mid_cache.end()) return it->second;

        Vertex mid{};
        mid.position = (vertices[i0].position + vertices[i1].position) * 0.5f;
        mid.normal   = glm::normalize(vertices[i0].normal + vertices[i1].normal);
        mid.uv       = (vertices[i0].uv + vertices[i1].uv) * 0.5f;
        mid.tangent  = (vertices[i0].tangent + vertices[i1].tangent) * 0.5f;

        uint32_t new_idx = static_cast<uint32_t>(vertices.size());
        vertices.push_back(mid);
        mid_cache[key] = new_idx;
        return new_idx;
    };

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpoints;
    for (const auto& f : faces) {
        uint32_t m01 = get_midpoint(f.i0, f.i1, midpoints);
        uint32_t m12 = get_midpoint(f.i1, f.i2, midpoints);
        uint32_t m20 = get_midpoint(f.i2, f.i0, midpoints);

        new_faces.push_back({f.i0, m01, m20, f.material_index});
        new_faces.push_back({f.i1, m12, m01, f.material_index});
        new_faces.push_back({f.i2, m20, m12, f.material_index});
        new_faces.push_back({m01, m12, m20, f.material_index});
    }

    faces = std::move(new_faces);
}

void EditableMesh::Deform(const std::function<Vec3(const Vec3& pos)>& deformer) {
    for (auto& v : vertices) {
        v.position = deformer(v.position);
    }
    RecalculateNormals(true);
}

std::vector<MeshEdge> EditableMesh::GetEdges() const {
    std::vector<MeshEdge> edges;
    auto add_edge = [&edges](uint32_t a, uint32_t b) {
        MeshEdge e{std::min(a, b), std::max(a, b)};
        if (std::find(edges.begin(), edges.end(), e) == edges.end()) {
            edges.push_back(e);
        }
    };
    for (const auto& f : faces) {
        add_edge(f.i0, f.i1);
        add_edge(f.i1, f.i2);
        add_edge(f.i2, f.i0);
    }
    return edges;
}

void EditableMesh::TranslateVertices(const std::vector<uint32_t>& indices, const Vec3& offset) {
    for (uint32_t idx : indices) {
        if (idx < vertices.size()) {
            vertices[idx].position += offset;
        }
    }
    RecalculateNormals(true);
}

void EditableMesh::ScaleVertices(const std::vector<uint32_t>& indices, const Vec3& scale, const Vec3& pivot) {
    for (uint32_t idx : indices) {
        if (idx < vertices.size()) {
            vertices[idx].position = pivot + (vertices[idx].position - pivot) * scale;
        }
    }
    RecalculateNormals(true);
}

void EditableMesh::WeldVertices(float threshold) {
    const float thresh_sq = threshold * threshold;
    std::vector<uint32_t> remap(vertices.size());
    std::vector<Vertex> new_verts;

    for (size_t i = 0; i < vertices.size(); ++i) {
        int target = -1;
        for (size_t j = 0; j < new_verts.size(); ++j) {
            Vec3 diff = vertices[i].position - new_verts[j].position;
            if (glm::dot(diff, diff) <= thresh_sq) {
                target = static_cast<int>(j);
                break;
            }
        }
        if (target >= 0) {
            remap[i] = static_cast<uint32_t>(target);
        } else {
            remap[i] = static_cast<uint32_t>(new_verts.size());
            new_verts.push_back(vertices[i]);
        }
    }

    std::vector<TriangleFace> new_faces;
    for (auto f : faces) {
        f.i0 = remap[f.i0];
        f.i1 = remap[f.i1];
        f.i2 = remap[f.i2];
        // Only keep non-degenerate triangles
        if (f.i0 != f.i1 && f.i1 != f.i2 && f.i2 != f.i0) {
            new_faces.push_back(f);
        }
    }

    vertices = std::move(new_verts);
    faces = std::move(new_faces);
    RecalculateNormals(true);
}

void EditableMesh::SplitEdge(uint32_t v0, uint32_t v1) {
    if (v0 >= vertices.size() || v1 >= vertices.size()) return;

    Vertex mid{};
    mid.position = (vertices[v0].position + vertices[v1].position) * 0.5f;
    mid.normal   = glm::normalize(vertices[v0].normal + vertices[v1].normal);
    mid.uv       = (vertices[v0].uv + vertices[v1].uv) * 0.5f;
    mid.tangent  = (vertices[v0].tangent + vertices[v1].tangent) * 0.5f;

    const uint32_t mid_idx = static_cast<uint32_t>(vertices.size());
    vertices.push_back(mid);

    std::vector<TriangleFace> new_faces;
    for (const auto& f : faces) {
        bool has_v0 = (f.i0 == v0 || f.i1 == v0 || f.i2 == v0);
        bool has_v1 = (f.i0 == v1 || f.i1 == v1 || f.i2 == v1);

        if (has_v0 && has_v1) {
            // Find third vertex
            uint32_t v_other = f.i0;
            if (v_other == v0 || v_other == v1) v_other = f.i1;
            if (v_other == v0 || v_other == v1) v_other = f.i2;

            new_faces.push_back({v0, mid_idx, v_other, f.material_index});
            new_faces.push_back({mid_idx, v1, v_other, f.material_index});
        } else {
            new_faces.push_back(f);
        }
    }
    faces = std::move(new_faces);
    RecalculateNormals(true);
}

void EditableMesh::ExtrudeFace(uint32_t face_index, float distance) {
    if (face_index >= faces.size()) return;

    const TriangleFace orig = faces[face_index];
    const Vertex& v0 = vertices[orig.i0];
    const Vertex& v1 = vertices[orig.i1];
    const Vertex& v2 = vertices[orig.i2];

    Vec3 face_normal = glm::normalize(glm::cross(v1.position - v0.position, v2.position - v0.position));

    Vertex nv0 = v0; nv0.position += face_normal * distance;
    Vertex nv1 = v1; nv1.position += face_normal * distance;
    Vertex nv2 = v2; nv2.position += face_normal * distance;

    const uint32_t j0 = static_cast<uint32_t>(vertices.size());
    const uint32_t j1 = j0 + 1;
    const uint32_t j2 = j0 + 2;

    vertices.push_back(nv0);
    vertices.push_back(nv1);
    vertices.push_back(nv2);

    // Update top face
    faces[face_index] = {j0, j1, j2, orig.material_index};

    // Add side quad skirts (2 triangles each)
    // Side 0 (orig.i0 -> orig.i1)
    faces.push_back({orig.i0, orig.i1, j1, orig.material_index});
    faces.push_back({orig.i0, j1, j0, orig.material_index});

    // Side 1 (orig.i1 -> orig.i2)
    faces.push_back({orig.i1, orig.i2, j2, orig.material_index});
    faces.push_back({orig.i1, j2, j1, orig.material_index});

    // Side 2 (orig.i2 -> orig.i0)
    faces.push_back({orig.i2, orig.i0, j0, orig.material_index});
    faces.push_back({orig.i2, j0, j2, orig.material_index});

    RecalculateNormals(true);
}

void EditableMesh::InsetFace(uint32_t face_index, float inset_amount) {
    if (face_index >= faces.size()) return;

    const TriangleFace orig = faces[face_index];
    const Vertex& v0 = vertices[orig.i0];
    const Vertex& v1 = vertices[orig.i1];
    const Vertex& v2 = vertices[orig.i2];

    const Vec3 center = (v0.position + v1.position + v2.position) / 3.0f;
    const float t = Clamp(inset_amount, 0.0f, 0.95f);

    Vertex nv0 = v0; nv0.position = glm::mix(v0.position, center, t);
    Vertex nv1 = v1; nv1.position = glm::mix(v1.position, center, t);
    Vertex nv2 = v2; nv2.position = glm::mix(v2.position, center, t);

    const uint32_t j0 = static_cast<uint32_t>(vertices.size());
    const uint32_t j1 = j0 + 1;
    const uint32_t j2 = j0 + 2;

    vertices.push_back(nv0);
    vertices.push_back(nv1);
    vertices.push_back(nv2);

    faces[face_index] = {j0, j1, j2, orig.material_index};

    faces.push_back({orig.i0, orig.i1, j1, orig.material_index});
    faces.push_back({orig.i0, j1, j0, orig.material_index});

    faces.push_back({orig.i1, orig.i2, j2, orig.material_index});
    faces.push_back({orig.i1, j2, j1, orig.material_index});

    faces.push_back({orig.i2, orig.i0, j0, orig.material_index});
    faces.push_back({orig.i2, j0, j2, orig.material_index});

    RecalculateNormals(true);
}

void EditableMesh::SubdivideFace(uint32_t face_index) {
    if (face_index >= faces.size()) return;

    const TriangleFace f = faces[face_index];

    auto make_mid = [this](uint32_t a, uint32_t b) -> uint32_t {
        Vertex mid{};
        mid.position = (vertices[a].position + vertices[b].position) * 0.5f;
        mid.normal   = glm::normalize(vertices[a].normal + vertices[b].normal);
        mid.uv       = (vertices[a].uv + vertices[b].uv) * 0.5f;
        mid.tangent  = (vertices[a].tangent + vertices[b].tangent) * 0.5f;
        uint32_t idx = static_cast<uint32_t>(vertices.size());
        vertices.push_back(mid);
        return idx;
    };

    uint32_t m01 = make_mid(f.i0, f.i1);
    uint32_t m12 = make_mid(f.i1, f.i2);
    uint32_t m20 = make_mid(f.i2, f.i0);

    faces[face_index] = {f.i0, m01, m20, f.material_index};
    faces.push_back({f.i1, m12, m01, f.material_index});
    faces.push_back({f.i2, m20, m12, f.material_index});
    faces.push_back({m01, m12, m20, f.material_index});

    RecalculateNormals(true);
}

void EditableMesh::FlipFaceNormal(uint32_t face_index) {
    if (face_index >= faces.size()) return;
    std::swap(faces[face_index].i1, faces[face_index].i2);
    RecalculateNormals(true);
}

void EditableMesh::DeleteFace(uint32_t face_index) {
    if (face_index < faces.size()) {
        faces.erase(faces.begin() + face_index);
    }
}

void EditableMesh::GenerateUVs(UVProjectionMode mode, const Vec2& scale, const Vec2& offset) {
    Vec3 aabb_min(1e20f);
    Vec3 aabb_max(-1e20f);
    for (const auto& v : vertices) {
        aabb_min = glm::min(aabb_min, v.position);
        aabb_max = glm::max(aabb_max, v.position);
    }
    const Vec3 extent = glm::max(aabb_max - aabb_min, Vec3(1e-4f));

    for (auto& v : vertices) {
        Vec3 norm_p = (v.position - aabb_min) / extent;
        Vec2 uv(0.0f);

        switch (mode) {
        case UVProjectionMode::PlanarX:
            uv = Vec2(norm_p.z, norm_p.y);
            break;
        case UVProjectionMode::PlanarY:
            uv = Vec2(norm_p.x, norm_p.z);
            break;
        case UVProjectionMode::PlanarZ:
            uv = Vec2(norm_p.x, norm_p.y);
            break;
        case UVProjectionMode::Box: {
            Vec3 abs_norm = glm::abs(v.normal);
            if (abs_norm.x >= abs_norm.y && abs_norm.x >= abs_norm.z) {
                uv = Vec2(norm_p.z, norm_p.y);
            } else if (abs_norm.y >= abs_norm.x && abs_norm.y >= abs_norm.z) {
                uv = Vec2(norm_p.x, norm_p.z);
            } else {
                uv = Vec2(norm_p.x, norm_p.y);
            }
            break;
        }
        case UVProjectionMode::Spherical: {
            Vec3 dir = glm::normalize(v.position - (aabb_min + aabb_max) * 0.5f);
            uv.x = 0.5f + std::atan2(dir.z, dir.x) / (2.0f * kPi);
            uv.y = 0.5f - std::asin(Clamp(dir.y, -1.0f, 1.0f)) / kPi;
            break;
        }
        case UVProjectionMode::Cylindrical: {
            Vec3 dir = glm::normalize(Vec3(v.position.x - (aabb_min.x + aabb_max.x) * 0.5f, 0.0f, v.position.z - (aabb_min.z + aabb_max.z) * 0.5f));
            uv.x = 0.5f + std::atan2(dir.z, dir.x) / (2.0f * kPi);
            uv.y = norm_p.y;
            break;
        }
        }

        v.uv = uv * scale + offset;
    }
}

MeshData EditableMesh::BuildMeshData(int material_index) const {
    MeshData result{};
    if (vertices.empty() || faces.empty()) return result;

    result.triangles.reserve(faces.size());

    Vec3 aabb_min(1e20f);
    Vec3 aabb_max(-1e20f);

    for (const auto& f : faces) {
        if (f.i0 >= vertices.size() || f.i1 >= vertices.size() || f.i2 >= vertices.size()) continue;

        const Vertex& v0 = vertices[f.i0];
        const Vertex& v1 = vertices[f.i1];
        const Vertex& v2 = vertices[f.i2];

        GPUTriangle tri{};
        tri.v0[0] = v0.position.x; tri.v0[1] = v0.position.y; tri.v0[2] = v0.position.z;
        tri.v1[0] = v1.position.x; tri.v1[1] = v1.position.y; tri.v1[2] = v1.position.z;
        tri.v2[0] = v2.position.x; tri.v2[1] = v2.position.y; tri.v2[2] = v2.position.z;

        tri.n0[0] = v0.normal.x; tri.n0[1] = v0.normal.y; tri.n0[2] = v0.normal.z;
        tri.n1[0] = v1.normal.x; tri.n1[1] = v1.normal.y; tri.n1[2] = v1.normal.z;
        tri.n2[0] = v2.normal.x; tri.n2[1] = v2.normal.y; tri.n2[2] = v2.normal.z;

        tri.uv0[0] = v0.uv.x; tri.uv0[1] = v0.uv.y;
        tri.uv1[0] = v1.uv.x; tri.uv1[1] = v1.uv.y;
        tri.uv2[0] = v2.uv.x; tri.uv2[1] = v2.uv.y;

        tri.mat_index = (f.material_index != 0) ? f.material_index : material_index;
        result.triangles.push_back(tri);

        aabb_min = glm::min(aabb_min, v0.position);
        aabb_min = glm::min(aabb_min, v1.position);
        aabb_min = glm::min(aabb_min, v2.position);

        aabb_max = glm::max(aabb_max, v0.position);
        aabb_max = glm::max(aabb_max, v1.position);
        aabb_max = glm::max(aabb_max, v2.position);
    }

    result.aabb_min = aabb_min;
    result.aabb_max = aabb_max;

    // Build BVH
    BuildBVH(result.triangles, result.bvh_nodes, 0, static_cast<int>(result.triangles.size()), 0);

    // Split into GPUTriPos and GPUTriAttr
    result.tri_pos.resize(result.triangles.size());
    result.tri_attr.resize(result.triangles.size());
    for (size_t i = 0; i < result.triangles.size(); ++i) {
        const GPUTriangle& t = result.triangles[i];
        GPUTriPos& p = result.tri_pos[i];
        GPUTriAttr& a = result.tri_attr[i];

        p.v0[0] = t.v0[0]; p.v0[1] = t.v0[1]; p.v0[2] = t.v0[2];
        p.e1[0] = t.v1[0] - t.v0[0]; p.e1[1] = t.v1[1] - t.v0[1]; p.e1[2] = t.v1[2] - t.v0[2];
        p.e2[0] = t.v2[0] - t.v0[0]; p.e2[1] = t.v2[1] - t.v0[1]; p.e2[2] = t.v2[2] - t.v0[2];

        a.n0[0] = t.n0[0]; a.n0[1] = t.n0[1]; a.n0[2] = t.n0[2];
        a.n1[0] = t.n1[0]; a.n1[1] = t.n1[1]; a.n1[2] = t.n1[2];
        a.n2[0] = t.n2[0]; a.n2[1] = t.n2[1]; a.n2[2] = t.n2[2];
        a.uv0[0] = t.uv0[0]; a.uv0[1] = t.uv0[1];
        a.uv1[0] = t.uv1[0]; a.uv1[1] = t.uv1[1];
        a.uv2[0] = t.uv2[0]; a.uv2[1] = t.uv2[1];
        a.mat_index = t.mat_index;
    }

    result.valid = true;
    return result;
}

// ---------------------------------------------------------------- MeshBuilder -

EditableMesh MeshBuilder::CreatePlane(float width, float height, int segments_x, int segments_y) {
    EditableMesh mesh;
    segments_x = std::max(1, segments_x);
    segments_y = std::max(1, segments_y);

    const float hx = width * 0.5f;
    const float hz = height * 0.5f;
    const float dx = width / static_cast<float>(segments_x);
    const float dz = height / static_cast<float>(segments_y);

    for (int y = 0; y <= segments_y; ++y) {
        for (int x = 0; x <= segments_x; ++x) {
            Vertex v;
            v.position = Vec3(-hx + x * dx, 0.0f, -hz + y * dz);
            v.normal   = Vec3(0.0f, 1.0f, 0.0f);
            v.uv       = Vec2(static_cast<float>(x) / segments_x, static_cast<float>(y) / segments_y);
            mesh.vertices.push_back(v);
        }
    }

    const int stride = segments_x + 1;
    for (int y = 0; y < segments_y; ++y) {
        for (int x = 0; x < segments_x; ++x) {
            uint32_t i0 = y * stride + x;
            uint32_t i1 = (y + 1) * stride + x;
            uint32_t i2 = y * stride + (x + 1);
            uint32_t i3 = (y + 1) * stride + (x + 1);

            mesh.faces.push_back({i0, i1, i2, 0});
            mesh.faces.push_back({i2, i1, i3, 0});
        }
    }
    return mesh;
}

EditableMesh MeshBuilder::CreateCube(const Vec3& half_extents) {
    EditableMesh mesh;
    const Vec3 h = half_extents;

    struct FaceDef {
        Vec3 normal;
        Vec3 corners[4];
        Vec2 uvs[4];
    };

    const FaceDef faces[6] = {
        // Front (+Z)
        { Vec3(0,0,1), { Vec3(-h.x,-h.y, h.z), Vec3( h.x,-h.y, h.z), Vec3( h.x, h.y, h.z), Vec3(-h.x, h.y, h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
        // Back (-Z)
        { Vec3(0,0,-1), { Vec3( h.x,-h.y,-h.z), Vec3(-h.x,-h.y,-h.z), Vec3(-h.x, h.y,-h.z), Vec3( h.x, h.y,-h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
        // Top (+Y)
        { Vec3(0,1,0), { Vec3(-h.x, h.y, h.z), Vec3( h.x, h.y, h.z), Vec3( h.x, h.y,-h.z), Vec3(-h.x, h.y,-h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
        // Bottom (-Y)
        { Vec3(0,-1,0), { Vec3(-h.x,-h.y,-h.z), Vec3( h.x,-h.y,-h.z), Vec3( h.x,-h.y, h.z), Vec3(-h.x,-h.y, h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
        // Right (+X)
        { Vec3(1,0,0), { Vec3( h.x,-h.y, h.z), Vec3( h.x,-h.y,-h.z), Vec3( h.x, h.y,-h.z), Vec3( h.x, h.y, h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
        // Left (-X)
        { Vec3(-1,0,0), { Vec3(-h.x,-h.y,-h.z), Vec3(-h.x,-h.y, h.z), Vec3(-h.x, h.y, h.z), Vec3(-h.x, h.y,-h.z) }, { {0,0}, {1,0}, {1,1}, {0,1} } },
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (int v = 0; v < 4; ++v) {
            Vertex vert;
            vert.position = faces[f].corners[v];
            vert.normal   = faces[f].normal;
            vert.uv       = faces[f].uvs[v];
            mesh.vertices.push_back(vert);
        }
        mesh.faces.push_back({base, base + 1, base + 2, 0});
        mesh.faces.push_back({base, base + 2, base + 3, 0});
    }
    return mesh;
}

EditableMesh MeshBuilder::CreateSphere(float radius, int rings, int sectors) {
    EditableMesh mesh;
    rings = std::max(3, rings);
    sectors = std::max(3, sectors);

    for (int r = 0; r <= rings; ++r) {
        const float phi = static_cast<float>(M_PI) * static_cast<float>(r) / rings;
        const float sin_phi = std::sin(phi);
        const float cos_phi = std::cos(phi);

        for (int s = 0; s <= sectors; ++s) {
            const float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(s) / sectors;
            const float sin_theta = std::sin(theta);
            const float cos_theta = std::cos(theta);

            Vec3 norm(cos_theta * sin_phi, cos_phi, sin_theta * sin_phi);
            Vertex v;
            v.position = norm * radius;
            v.normal   = norm;
            v.uv       = Vec2(static_cast<float>(s) / sectors, static_cast<float>(r) / rings);
            mesh.vertices.push_back(v);
        }
    }

    const int stride = sectors + 1;
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < sectors; ++s) {
            uint32_t i0 = r * stride + s;
            uint32_t i1 = (r + 1) * stride + s;
            uint32_t i2 = (r + 1) * stride + (s + 1);
            uint32_t i3 = r * stride + (s + 1);

            mesh.faces.push_back({i0, i1, i2, 0});
            mesh.faces.push_back({i0, i2, i3, 0});
        }
    }
    return mesh;
}

EditableMesh MeshBuilder::CreateCylinder(float radius, float height, int segments) {
    EditableMesh mesh;
    segments = std::max(3, segments);
    const float hh = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        float ca = std::cos(angle);
        float sa = std::sin(angle);
        Vec3 n(ca, 0.0f, sa);

        Vertex v_top;
        v_top.position = Vec3(ca * radius, hh, sa * radius);
        v_top.normal   = n;
        v_top.uv       = Vec2(static_cast<float>(i) / segments, 1.0f);
        mesh.vertices.push_back(v_top);

        Vertex v_bot;
        v_bot.position = Vec3(ca * radius, -hh, sa * radius);
        v_bot.normal   = n;
        v_bot.uv       = Vec2(static_cast<float>(i) / segments, 0.0f);
        mesh.vertices.push_back(v_bot);
    }

    // Side faces
    for (int i = 0; i < segments; ++i) {
        uint32_t top0 = i * 2;
        uint32_t bot0 = i * 2 + 1;
        uint32_t top1 = (i + 1) * 2;
        uint32_t bot1 = (i + 1) * 2 + 1;

        mesh.faces.push_back({top0, bot0, bot1, 0});
        mesh.faces.push_back({top0, bot1, top1, 0});
    }

    // Top cap
    uint32_t top_center_idx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({Vec3(0, hh, 0), Vec3(0, 1, 0), Vec2(0.5f, 0.5f)});
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        float a1 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i + 1) / segments;
        uint32_t v0 = static_cast<uint32_t>(mesh.vertices.size());
        uint32_t v1 = v0 + 1;
        mesh.vertices.push_back({Vec3(std::cos(a0) * radius, hh, std::sin(a0) * radius), Vec3(0, 1, 0), Vec2(0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0))});
        mesh.vertices.push_back({Vec3(std::cos(a1) * radius, hh, std::sin(a1) * radius), Vec3(0, 1, 0), Vec2(0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1))});
        mesh.faces.push_back({top_center_idx, v0, v1, 0});
    }

    // Bottom cap
    uint32_t bot_center_idx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({Vec3(0, -hh, 0), Vec3(0, -1, 0), Vec2(0.5f, 0.5f)});
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        float a1 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i + 1) / segments;
        uint32_t v0 = static_cast<uint32_t>(mesh.vertices.size());
        uint32_t v1 = v0 + 1;
        mesh.vertices.push_back({Vec3(std::cos(a0) * radius, -hh, std::sin(a0) * radius), Vec3(0, -1, 0), Vec2(0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0))});
        mesh.vertices.push_back({Vec3(std::cos(a1) * radius, -hh, std::sin(a1) * radius), Vec3(0, -1, 0), Vec2(0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1))});
        mesh.faces.push_back({bot_center_idx, v1, v0, 0});
    }

    return mesh;
}

EditableMesh MeshBuilder::CreateCone(float radius, float height, int segments) {
    EditableMesh mesh;
    segments = std::max(3, segments);
    const float hh = height * 0.5f;

    // Apex
    uint32_t apex_idx = 0;
    mesh.vertices.push_back({Vec3(0, hh, 0), Vec3(0, 1, 0), Vec2(0.5f, 1.0f)});

    // Base ring
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        float ca = std::cos(angle);
        float sa = std::sin(angle);
        Vec3 side_normal = glm::normalize(Vec3(ca, radius / height, sa));
        mesh.vertices.push_back({Vec3(ca * radius, -hh, sa * radius), side_normal, Vec2(static_cast<float>(i) / segments, 0.0f)});
    }

    for (int i = 0; i < segments; ++i) {
        mesh.faces.push_back({apex_idx, static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i + 2), 0});
    }

    // Base cap
    uint32_t bot_center_idx = static_cast<uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({Vec3(0, -hh, 0), Vec3(0, -1, 0), Vec2(0.5f, 0.5f)});
    for (int i = 0; i < segments; ++i) {
        float a0 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / segments;
        float a1 = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i + 1) / segments;
        uint32_t v0 = static_cast<uint32_t>(mesh.vertices.size());
        uint32_t v1 = v0 + 1;
        mesh.vertices.push_back({Vec3(std::cos(a0) * radius, -hh, std::sin(a0) * radius), Vec3(0, -1, 0), Vec2(0.5f + 0.5f * std::cos(a0), 0.5f + 0.5f * std::sin(a0))});
        mesh.vertices.push_back({Vec3(std::cos(a1) * radius, -hh, std::sin(a1) * radius), Vec3(0, -1, 0), Vec2(0.5f + 0.5f * std::cos(a1), 0.5f + 0.5f * std::sin(a1))});
        mesh.faces.push_back({bot_center_idx, v1, v0, 0});
    }

    return mesh;
}

EditableMesh MeshBuilder::CreateTorus(float main_radius, float tube_radius, int radial_segs, int tubular_segs) {
    EditableMesh mesh;
    radial_segs = std::max(3, radial_segs);
    tubular_segs = std::max(3, tubular_segs);

    for (int j = 0; j <= radial_segs; ++j) {
        float u = 2.0f * static_cast<float>(M_PI) * static_cast<float>(j) / radial_segs;
        float cu = std::cos(u);
        float su = std::sin(u);

        for (int i = 0; i <= tubular_segs; ++i) {
            float v = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / tubular_segs;
            float cv = std::cos(v);
            float sv = std::sin(v);

            Vec3 pos((main_radius + tube_radius * cv) * cu,
                     tube_radius * sv,
                     (main_radius + tube_radius * cv) * su);

            Vec3 norm(cv * cu, sv, cv * su);

            Vertex vert;
            vert.position = pos;
            vert.normal   = glm::normalize(norm);
            vert.uv       = Vec2(static_cast<float>(j) / radial_segs, static_cast<float>(i) / tubular_segs);
            mesh.vertices.push_back(vert);
        }
    }

    const int stride = tubular_segs + 1;
    for (int j = 0; j < radial_segs; ++j) {
        for (int i = 0; i < tubular_segs; ++i) {
            uint32_t a = j * stride + i;
            uint32_t b = (j + 1) * stride + i;
            uint32_t c = (j + 1) * stride + (i + 1);
            uint32_t d = j * stride + (i + 1);

            mesh.faces.push_back({a, b, d, 0});
            mesh.faces.push_back({b, c, d, 0});
        }
    }

    return mesh;
}

} // namespace lucida
