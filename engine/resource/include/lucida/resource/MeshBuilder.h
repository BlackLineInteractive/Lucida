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

struct EditableMesh {
    std::vector<Vertex> vertices;
    std::vector<TriangleFace> faces;

    // Transforms & manipulation
    void Translate(const Vec3& offset);
    void Scale(const Vec3& scale);
    void RotateX(float angle_radians);
    void RotateY(float angle_radians);
    void RotateZ(float angle_radians);
    void RecalculateNormals(bool smooth = true);
    void Subdivide();
    void Deform(const std::function<Vec3(const Vec3& pos)>& deformer);

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
