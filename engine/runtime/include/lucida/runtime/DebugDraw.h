// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Visual 3D Debugging Draw System for gameplay gizmos (GEA ch.3.5 / ch.14.7).

#include "lucida/core/math/Math.h"

#include <vector>
#include <mutex>

namespace lucida {

struct DebugLine {
    Vec3 start{0.0f};
    Vec3 end{0.0f};
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct DebugBox {
    Vec3 center{0.0f};
    Vec3 half_extents{0.5f};
    Vec4 color{0.0f, 1.0f, 0.0f, 1.0f};
};

struct DebugSphere {
    Vec3 center{0.0f};
    f32  radius = 0.5f;
    Vec4 color{0.0f, 1.0f, 0.0f, 1.0f};
};

struct DebugCone {
    Vec3 apex{0.0f};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    f32  angle_rad = 0.5f;
    f32  distance = 5.0f;
    Vec4 color{1.0f, 1.0f, 0.0f, 1.0f};
};

class DebugDraw {
public:
    static void DrawLine(const Vec3& start, const Vec3& end, const Vec4& color = Vec4(1.0f));
    static void DrawRay(const Vec3& origin, const Vec3& dir, f32 length = 2.0f, const Vec4& color = Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    static void DrawBox(const Vec3& center, const Vec3& half_extents, const Vec4& color = Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    static void DrawSphere(const Vec3& center, f32 radius, const Vec4& color = Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    static void DrawCone(const Vec3& apex, const Vec3& direction, f32 angle_rad, f32 distance, const Vec4& color = Vec4(1.0f, 1.0f, 0.0f, 1.0f));

    static void Clear();

    static std::vector<DebugLine>   GetLines();
    static std::vector<DebugBox>    GetBoxes();
    static std::vector<DebugSphere> GetSpheres();
    static std::vector<DebugCone>   GetCones();

    static bool& Enabled();
};

} // namespace lucida
