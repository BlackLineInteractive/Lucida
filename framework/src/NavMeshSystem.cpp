// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/NavMeshSystem.h"

#include "lucida/runtime/GameplayComponents.h"
#include "lucida/runtime/World.h"
#include "lucida/runtime/DebugDraw.h"
#include "lucida/core/ecs/Registry.h"
#include "lucida/core/diag/Log.h"

#include <queue>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace lucida {

u32 NavMeshSystem::GetNodeIndex(i32 x, i32 z) const {
    if (x < 0 || x >= m_grid_x || z < 0 || z >= m_grid_z) return UINT32_MAX;
    return static_cast<u32>(z * m_grid_x + x);
}

void NavMeshSystem::Clear() {
    m_nodes.clear();
    m_grid_x = 0;
    m_grid_z = 0;
}

void NavMeshSystem::Bake(World& world) {
    Clear();
    Registry& reg = world.Entities();

    // 1. Find NavMeshBoundsComponent to determine bounding volume
    Vec3 bounds_min(-25.0f, 0.0f, -25.0f);
    Vec3 bounds_max( 25.0f, 5.0f,  25.0f);
    f32  cell_size = 1.0f;

    auto bounds_view = reg.Raw().view<NavMeshBoundsComponent, LocalTransform>();
    for (auto entity : bounds_view) {
        const auto& bc = bounds_view.get<NavMeshBoundsComponent>(entity);
        const auto& lt = bounds_view.get<LocalTransform>(entity);
        bounds_min = lt.position - bc.size * 0.5f;
        bounds_max = lt.position + bc.size * 0.5f;
        cell_size  = std::max(0.25f, bc.cell_size);
        break; // Use primary bounds
    }

    m_origin    = bounds_min;
    m_cell_size = cell_size;
    m_grid_x    = std::max(2, static_cast<i32>(std::ceil((bounds_max.x - bounds_min.x) / cell_size)));
    m_grid_z    = std::max(2, static_cast<i32>(std::ceil((bounds_max.z - bounds_min.z) / cell_size)));
    m_extents   = (bounds_max - bounds_min) * 0.5f;

    m_nodes.resize(m_grid_x * m_grid_z);

    // 2. Initialize grid nodes
    for (i32 z = 0; z < m_grid_z; ++z) {
        for (i32 x = 0; x < m_grid_x; ++x) {
            u32 idx = GetNodeIndex(x, z);
            m_nodes[idx].position = Vec3(
                bounds_min.x + (x + 0.5f) * cell_size,
                bounds_min.y,
                bounds_min.z + (z + 0.5f) * cell_size
            );
            m_nodes[idx].is_walkable = true;
            m_nodes[idx].neighbors.clear();

            // Connect 4-way adjacent grid neighbors
            if (x > 0)             m_nodes[idx].neighbors.push_back(GetNodeIndex(x - 1, z));
            if (x < m_grid_x - 1)  m_nodes[idx].neighbors.push_back(GetNodeIndex(x + 1, z));
            if (z > 0)             m_nodes[idx].neighbors.push_back(GetNodeIndex(x, z - 1));
            if (z < m_grid_z - 1)  m_nodes[idx].neighbors.push_back(GetNodeIndex(x, z + 1));
        }
    }

    // 3. Carve obstacles (NavMeshObstacleComponent)
    auto obs_view = reg.Raw().view<NavMeshObstacleComponent, LocalTransform>();
    for (auto entity : obs_view) {
        const auto& obs = obs_view.get<NavMeshObstacleComponent>(entity);
        const auto& lt  = obs_view.get<LocalTransform>(entity);
        if (!obs.carve_navmesh) continue;

        Vec3 obs_min = lt.position - obs.size * 0.5f;
        Vec3 obs_max = lt.position + obs.size * 0.5f;

        i32 min_x = std::clamp(static_cast<i32>((obs_min.x - bounds_min.x) / cell_size), 0, m_grid_x - 1);
        i32 max_x = std::clamp(static_cast<i32>((obs_max.x - bounds_min.x) / cell_size), 0, m_grid_x - 1);
        i32 min_z = std::clamp(static_cast<i32>((obs_min.z - bounds_min.z) / cell_size), 0, m_grid_z - 1);
        i32 max_z = std::clamp(static_cast<i32>((obs_max.z - bounds_min.z) / cell_size), 0, m_grid_z - 1);

        for (i32 oz = min_z; oz <= max_z; ++oz) {
            for (i32 ox = min_x; ox <= max_x; ++ox) {
                u32 idx = GetNodeIndex(ox, oz);
                if (idx < m_nodes.size()) {
                    m_nodes[idx].is_walkable = false;
                }
            }
        }
    }

    // 4. Add off-mesh links (NavMeshLinkComponent)
    auto link_view = reg.Raw().view<NavMeshLinkComponent, LocalTransform>();
    for (auto entity : link_view) {
        const auto& link = link_view.get<NavMeshLinkComponent>(entity);
        const auto& lt   = link_view.get<LocalTransform>(entity);
        Vec3 sp = lt.position + link.start_point;
        Vec3 ep = lt.position + link.end_point;

        i32 sx = std::clamp(static_cast<i32>((sp.x - bounds_min.x) / cell_size), 0, m_grid_x - 1);
        i32 sz = std::clamp(static_cast<i32>((sp.z - bounds_min.z) / cell_size), 0, m_grid_z - 1);
        i32 ex = std::clamp(static_cast<i32>((ep.x - bounds_min.x) / cell_size), 0, m_grid_x - 1);
        i32 ez = std::clamp(static_cast<i32>((ep.z - bounds_min.z) / cell_size), 0, m_grid_z - 1);

        u32 s_idx = GetNodeIndex(sx, sz);
        u32 e_idx = GetNodeIndex(ex, ez);
        if (s_idx < m_nodes.size() && e_idx < m_nodes.size()) {
            m_nodes[s_idx].neighbors.push_back(e_idx);
            if (link.is_bidirectional) {
                m_nodes[e_idx].neighbors.push_back(s_idx);
            }
        }
    }

    LUCIDA_INFO(Runtime, "NavMesh baked: %d x %d grid (%zu nodes)", m_grid_x, m_grid_z, m_nodes.size());
}

bool NavMeshSystem::FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& out_path) const {
    out_path.clear();
    if (m_nodes.empty() || m_grid_x <= 0 || m_grid_z <= 0) return false;

    // Find nearest start and goal grid indices
    i32 sx = std::clamp(static_cast<i32>((start.x - m_origin.x) / m_cell_size), 0, m_grid_x - 1);
    i32 sz = std::clamp(static_cast<i32>((start.z - m_origin.z) / m_cell_size), 0, m_grid_z - 1);
    i32 gx = std::clamp(static_cast<i32>((goal.x - m_origin.x) / m_cell_size),  0, m_grid_x - 1);
    i32 gz = std::clamp(static_cast<i32>((goal.z - m_origin.z) / m_cell_size),  0, m_grid_z - 1);

    u32 start_idx = GetNodeIndex(sx, sz);
    u32 goal_idx  = GetNodeIndex(gx, gz);

    if (start_idx >= m_nodes.size() || goal_idx >= m_nodes.size()) return false;
    if (!m_nodes[start_idx].is_walkable || !m_nodes[goal_idx].is_walkable) {
        // Simple direct fallback if endpoints are unroutable
        out_path.push_back(goal);
        return true;
    }

    // A* Priority Queue: (f_score, node_index)
    typedef std::pair<f32, u32> NodeScore;
    std::priority_queue<NodeScore, std::vector<NodeScore>, std::greater<NodeScore>> open_set;

    std::unordered_map<u32, f32> g_score;
    std::unordered_map<u32, u32> came_from;

    g_score[start_idx] = 0.0f;
    f32 h_start = glm::distance(m_nodes[start_idx].position, m_nodes[goal_idx].position);
    open_set.push({h_start, start_idx});

    bool found = false;

    while (!open_set.empty()) {
        u32 current = open_set.top().second;
        open_set.pop();

        if (current == goal_idx) {
            found = true;
            break;
        }

        f32 current_g = g_score[current];

        for (u32 neighbor : m_nodes[current].neighbors) {
            if (neighbor >= m_nodes.size() || !m_nodes[neighbor].is_walkable) continue;

            f32 tentative_g = current_g + glm::distance(m_nodes[current].position, m_nodes[neighbor].position);
            auto it = g_score.find(neighbor);
            if (it == g_score.end() || tentative_g < it->second) {
                came_from[neighbor] = current;
                g_score[neighbor] = tentative_g;
                f32 f = tentative_g + glm::distance(m_nodes[neighbor].position, m_nodes[goal_idx].position);
                open_set.push({f, neighbor});
            }
        }
    }

    if (!found) {
        // Direct route if pathfinding did not converge
        out_path.push_back(goal);
        return true;
    }

    // Reconstruct path backwards
    std::vector<Vec3> rev_path;
    rev_path.push_back(goal);
    u32 curr = goal_idx;
    while (curr != start_idx) {
        rev_path.push_back(m_nodes[curr].position);
        auto it = came_from.find(curr);
        if (it == came_from.end()) break;
        curr = it->second;
    }
    rev_path.push_back(start);

    // Reverse to forward order
    out_path.assign(rev_path.rbegin(), rev_path.rend());
    return true;
}

void NavMeshSystem::Update(World& world, f32 dt) {
    if (dt <= 0.0f) return;
    Registry& reg = world.Entities();

    auto view = reg.Raw().view<NavigationAgentComponent, LocalTransform>();
    for (auto entity : view) {
        auto& agent = view.get<NavigationAgentComponent>(entity);
        auto& lt    = view.get<LocalTransform>(entity);

        // Path requested or target destination changed
        f32 dist_to_dest = glm::distance(lt.position, agent.destination);
        if (agent.path_pending || (agent.path_points.empty() && dist_to_dest > agent.stopping_distance)) {
            if (!IsBaked()) {
                Bake(world);
            }
            FindPath(lt.position, agent.destination, agent.path_points);
            agent.path_pending = false;
        }

        // Steer entity along waypoints
        while (!agent.path_points.empty()) {
            Vec3 target = agent.path_points.front();
            target.y = lt.position.y; // Keep agent on plane

            Vec3 diff = target - lt.position;
            diff.y = 0.0f;
            f32 dist = glm::length(diff);

            if (dist <= 0.05f) {
                // Already at this waypoint, pop and evaluate next
                agent.path_points.erase(agent.path_points.begin());
                continue;
            }

            f32 step = std::min(agent.speed * dt, dist);
            Vec3 dir = diff / dist;
            lt.position += dir * step;

            // Smoothly orient rotation towards move direction
            f32 target_yaw = std::atan2(dir.x, dir.z);
            lt.rotation = glm::angleAxis(target_yaw, Vec3(0, 1, 0));

            if (dist - step <= agent.stopping_distance) {
                agent.path_points.erase(agent.path_points.begin());
            }
            break;
        }

        // Visualize active agent path in editor
        for (usize i = 0; i + 1 < agent.path_points.size(); ++i) {
            DebugDraw::DrawLine(agent.path_points[i] + Vec3(0, 0.2f, 0),
                                agent.path_points[i+1] + Vec3(0, 0.2f, 0),
                                Vec4(0.1f, 0.9f, 0.2f, 1.0f));
        }
    }
}

} // namespace lucida
