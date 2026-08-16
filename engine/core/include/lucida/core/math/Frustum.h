// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/math/Math.h"
#include <array>

namespace lucida {

struct Plane {
    Vec3 normal{0.0f, 1.0f, 0.0f};
    f32  distance = 0.0f; // ax + by + cz + d = 0

    Plane() = default;
    Plane(const Vec3& norm, f32 dist) : normal(norm), distance(dist) {}
    Plane(const Vec4& v) : normal(v.x, v.y, v.z), distance(v.w) {
        Normalize();
    }

    void Normalize() {
        const f32 len = glm::length(normal);
        if (len > 1e-6f) {
            normal /= len;
            distance /= len;
        }
    }

    f32 DistanceToPoint(const Vec3& pt) const {
        return glm::dot(normal, pt) + distance;
    }
};

class Frustum {
public:
    enum PlaneSide : usize {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        PlaneCount
    };

    std::array<Plane, PlaneCount> planes;

    Frustum() = default;

    // Extracts frustum clipping planes from a combined View-Projection matrix (Gribb-Hartmann method)
    static Frustum FromMatrix(const Mat4& vp) {
        Frustum f;

        // Left
        f.planes[Left] = Plane(Vec4(
            vp[0][3] + vp[0][0],
            vp[1][3] + vp[1][0],
            vp[2][3] + vp[2][0],
            vp[3][3] + vp[3][0]));

        // Right
        f.planes[Right] = Plane(Vec4(
            vp[0][3] - vp[0][0],
            vp[1][3] - vp[1][0],
            vp[2][3] - vp[2][0],
            vp[3][3] - vp[3][0]));

        // Bottom
        f.planes[Bottom] = Plane(Vec4(
            vp[0][3] + vp[0][1],
            vp[1][3] + vp[1][1],
            vp[2][3] + vp[2][1],
            vp[3][3] + vp[3][1]));

        // Top
        f.planes[Top] = Plane(Vec4(
            vp[0][3] - vp[0][1],
            vp[1][3] - vp[1][1],
            vp[2][3] - vp[2][1],
            vp[3][3] - vp[3][1]));

        // Near
        f.planes[Near] = Plane(Vec4(
            vp[0][3] + vp[0][2],
            vp[1][3] + vp[1][2],
            vp[2][3] + vp[2][2],
            vp[3][3] + vp[3][2]));

        // Far
        f.planes[Far] = Plane(Vec4(
            vp[0][3] - vp[0][2],
            vp[1][3] - vp[1][2],
            vp[2][3] - vp[2][2],
            vp[3][3] - vp[3][2]));

        return f;
    }

    bool ContainsPoint(const Vec3& pt) const {
        for (const auto& plane : planes) {
            if (plane.DistanceToPoint(pt) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    // Fast AABB-Frustum intersection using positive/negative vertex testing (GEA 11.2)
    bool Intersects(const AABB& aabb) const {
        if (!aabb.IsValid()) return false;

        for (const auto& plane : planes) {
            Vec3 p = aabb.min;
            if (plane.normal.x >= 0.0f) p.x = aabb.max.x;
            if (plane.normal.y >= 0.0f) p.y = aabb.max.y;
            if (plane.normal.z >= 0.0f) p.z = aabb.max.z;

            if (plane.DistanceToPoint(p) < 0.0f) {
                return false; // Entirely outside this plane
            }
        }
        return true;
    }
};

} // namespace lucida
