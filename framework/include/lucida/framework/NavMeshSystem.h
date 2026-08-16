// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Navigation Mesh & Pathfinding System (GEA ch. 18)
//
// 1. Bakes walkable geometry & obstacles into a navigable mesh graph.
// 2. Computes shortest collision-free paths using A* search.
// 3. Drives NavigationAgentComponent entities along waypoints.
// 4. Integrates with DebugDraw for real-time path & NavMesh visualization.

#include "lucida/core/math/Math.h"
#include <vector>

namespace lucida {

class World;

struct NavNode {
    Vec3 position{0.0f};
    std::vector<u32> neighbors;
    bool is_walkable = true;
};

class NavMeshSystem {
public:
    // Bake navigation graph from scene bounds, obstacles, and off-mesh links
    void Bake(World& world);

    // Pathfinding query using A* search on the NavMesh
    bool FindPath(const Vec3& start, const Vec3& goal, std::vector<Vec3>& out_path) const;

    // Fixed tick update: steers NavigationAgentComponent entities along path
    void Update(World& world, f32 dt);

    bool IsBaked() const { return !m_nodes.empty(); }
    usize NodeCount() const { return m_nodes.size(); }
    const std::vector<NavNode>& Nodes() const { return m_nodes; }

    void Clear();

private:
    std::vector<NavNode> m_nodes;
    Vec3 m_origin{0.0f};
    Vec3 m_extents{25.0f, 5.0f, 25.0f};
    f32  m_cell_size = 1.0f;
    i32  m_grid_x = 0;
    i32  m_grid_z = 0;

    u32 GetNodeIndex(i32 x, i32 z) const;
};

} // namespace lucida
