// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Math facade over glm. Nothing here reimplements glm - it names the types the
// engine uses and adds the few aggregates glm does not ship (AABB, Ray).

#include "lucida/core/platform/Platform.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <limits>

namespace lucida {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using IVec2 = glm::ivec2;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

inline constexpr f32 kPi      = 3.14159265358979323846f;
inline constexpr f32 kTwoPi   = kPi * 2.0f;
inline constexpr f32 kHalfPi  = kPi * 0.5f;
inline constexpr f32 kDegToRad = kPi / 180.0f;
inline constexpr f32 kRadToDeg = 180.0f / kPi;
inline constexpr f32 kEpsilon = 1e-6f;
inline constexpr f32 kInfinity = std::numeric_limits<f32>::infinity();

template <typename T>
constexpr T Min(T a, T b) { return a < b ? a : b; }
template <typename T>
constexpr T Max(T a, T b) { return a > b ? a : b; }
template <typename T>
constexpr T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

constexpr f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

struct Ray {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};
    f32  tmin = 0.0f;
    f32  tmax = kInfinity;

    Vec3 At(f32 t) const { return origin + direction * t; }
};

struct AABB {
    Vec3 min{ kInfinity,  kInfinity,  kInfinity};
    Vec3 max{-kInfinity, -kInfinity, -kInfinity};

    void Expand(const Vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
    void Expand(const AABB& b) { min = glm::min(min, b.min); max = glm::max(max, b.max); }

    Vec3 Center()  const { return (min + max) * 0.5f; }
    Vec3 Extent()  const { return max - min; }
    bool IsValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    f32 SurfaceArea() const {
        if (!IsValid()) return 0.0f;
        const Vec3 d = Extent();
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    // Corner-by-corner: an affine transform of the box is not the box of the
    // transformed min/max once rotation is involved.
    AABB Transformed(const Mat4& m) const {
        AABB out;
        for (int i = 0; i < 8; ++i) {
            const Vec3 corner((i & 1) ? max.x : min.x,
                              (i & 2) ? max.y : min.y,
                              (i & 4) ? max.z : min.z);
            out.Expand(Vec3(m * Vec4(corner, 1.0f)));
        }
        return out;
    }
};

// Rigid transform with uniform scale. Stored decomposed: interpolating a
// matrix is wrong, interpolating position/rotation/scale is not.
struct Transform {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    f32  scale = 1.0f;

    Mat4 ToMatrix() const {
        Mat4 m = glm::mat4_cast(rotation);
        m[0] *= scale; m[1] *= scale; m[2] *= scale;
        m[3] = Vec4(position, 1.0f);
        return m;
    }

    static Transform Lerp(const Transform& a, const Transform& b, f32 t) {
        Transform out;
        out.position = glm::mix(a.position, b.position, t);
        out.rotation = glm::slerp(a.rotation, b.rotation, t);
        out.scale    = a.scale + (b.scale - a.scale) * t;
        return out;
    }
};

} // namespace lucida
