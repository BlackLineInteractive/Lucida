// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/runtime/DebugDraw.h"

namespace lucida {

namespace {
struct DebugDrawStorage {
    std::vector<DebugLine>   lines;
    std::vector<DebugBox>    boxes;
    std::vector<DebugSphere> spheres;
    std::vector<DebugCone>   cones;
    std::mutex               mutex;
    bool                     enabled = true;
};

DebugDrawStorage& Storage() {
    static DebugDrawStorage s_storage;
    return s_storage;
}
} // namespace

bool& DebugDraw::Enabled() {
    return Storage().enabled;
}

void DebugDraw::DrawLine(const Vec3& start, const Vec3& end, const Vec4& color) {
    auto& s = Storage();
    if (!s.enabled) return;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.lines.push_back({start, end, color});
}

void DebugDraw::DrawRay(const Vec3& origin, const Vec3& dir, f32 length, const Vec4& color) {
    DrawLine(origin, origin + glm::normalize(dir) * length, color);
}

void DebugDraw::DrawBox(const Vec3& center, const Vec3& half_extents, const Vec4& color) {
    auto& s = Storage();
    if (!s.enabled) return;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.boxes.push_back({center, half_extents, color});
}

void DebugDraw::DrawSphere(const Vec3& center, f32 radius, const Vec4& color) {
    auto& s = Storage();
    if (!s.enabled) return;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.spheres.push_back({center, radius, color});
}

void DebugDraw::DrawCone(const Vec3& apex, const Vec3& direction, f32 angle_rad, f32 distance, const Vec4& color) {
    auto& s = Storage();
    if (!s.enabled) return;
    std::lock_guard<std::mutex> lock(s.mutex);
    s.cones.push_back({apex, glm::normalize(direction), angle_rad, distance, color});
}

void DebugDraw::Clear() {
    auto& s = Storage();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.lines.clear();
    s.boxes.clear();
    s.spheres.clear();
    s.cones.clear();
}

std::vector<DebugLine> DebugDraw::GetLines() {
    auto& s = Storage();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.lines;
}

std::vector<DebugBox> DebugDraw::GetBoxes() {
    auto& s = Storage();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.boxes;
}

std::vector<DebugSphere> DebugDraw::GetSpheres() {
    auto& s = Storage();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.spheres;
}

std::vector<DebugCone> DebugDraw::GetCones() {
    auto& s = Storage();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.cones;
}

} // namespace lucida
