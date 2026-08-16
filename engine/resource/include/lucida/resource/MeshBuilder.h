// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/math/Math.h"
#include "lucida/render/MeshData.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace lucida {

enum class MeshEditMode : u8 {
    Object = 0,
    Vertex,
    Edge,
    Face
};

enum class UVProjectionMode : u8 {
    PlanarX = 0,
    PlanarY,
    PlanarZ,
    Box,
    Spherical,
    Cylindrical
};

struct Vertex {
    Vec3 position{0.0f};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    Vec2 uv{0.0f};
    Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
};

struct TriangleFace {
    uint32_t i0 = 0;
    uint32_t i1 = 0;
    uint32_t i2 = 0;
    int material_index = 0;
};

struct MeshEdge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    bool operator==(const MeshEdge& other) const {
        return (v0 == other.v0 && v1 == other.v1) || (v0 == other.v1 && v1 == other.v0);
    }
};

struct EditableMesh {
    std::vector<Vertex> vertices;
    std::vector<TriangleFace> faces;

    // Whole Mesh Transforms & manipulation
    void Translate(const Vec3& offset);
    void Scale(const Vec3& scale);
    void RotateX(float angle_radians);
    void RotateY(float angle_radians);
    void RotateZ(float angle_radians);
    void RecalculateNormals(bool smooth = true);
    void Subdivide();
    void Deform(const std::function<Vec3(const Vec3& pos)>& deformer);

    // Sub-Element Manipulation (Vertices, Edges, Faces)
    std::vector<MeshEdge> GetEdges() const;
    void TranslateVertices(const std::vector<uint32_t>& indices, const Vec3& offset);
    void ScaleVertices(const std::vector<uint32_t>& indices, const Vec3& scale, const Vec3& pivot = Vec3(0.0f));
    void WeldVertices(float threshold = 1e-4f);

    void SplitEdge(uint32_t v0, uint32_t v1);
    void ExtrudeFace(uint32_t face_index, float distance);
    void InsetFace(uint32_t face_index, float inset_amount);
    void SubdivideFace(uint32_t face_index);
    void FlipFaceNormal(uint32_t face_index);
    void DeleteFace(uint32_t face_index);

    // UV Mapping & Unwrapping
    void GenerateUVs(UVProjectionMode mode, const Vec2& scale = Vec2(1.0f), const Vec2& offset = Vec2(0.0f));

    // Conversion to engine GPU-compatible MeshData with BVH
    MeshData BuildMeshData(int material_index = 0) const;
};

class MeshBuilder {
public:
    // Procedural Primitives
    static EditableMesh CreatePlane(float width = 1.0f, float height = 1.0f, int segments_x = 1, int segments_y = 1);
    static EditableMesh CreateCube(const Vec3& half_extents = Vec3(0.5f));
    static EditableMesh CreateSphere(float radius = 0.5f, int rings = 16, int sectors = 32);
    static EditableMesh CreateCylinder(float radius = 0.5f, float height = 1.0f, int segments = 24);
    static EditableMesh CreateCone(float radius = 0.5f, float height = 1.0f, int segments = 24);
    static EditableMesh CreateTorus(float main_radius = 0.5f, float tube_radius = 0.15f, int radial_segs = 24, int tubular_segs = 16);
};

} // namespace lucida
