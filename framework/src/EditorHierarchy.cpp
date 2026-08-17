// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

namespace {

// Helper to determine node type icon and color
struct NodeVisualInfo {
    VectorIcon  icon = VectorIcon::None;
    ImU32       color = IM_COL32(94, 166, 255, 255); // Blue
    std::string badge;
};

NodeVisualInfo GetNodeVisualInfo(Registry& entities, Entity entity) {
    NodeVisualInfo info;
    if (entities.Get<GroupComponent>(entity)) {
        info.icon = VectorIcon::Group;
        info.color = IM_COL32(94, 166, 255, 255); // Light Blue Group
        return info;
    }

    if (const EditableMeshComponent* emc = entities.Get<EditableMeshComponent>(entity)) {
        info.icon = VectorIcon::Mesh;
        info.color = IM_COL32(80, 227, 194, 255); // Emerald Green Mesh
        const usize v_count = emc->mesh.vertices.size();
        if (v_count >= 1000) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1fk", static_cast<double>(v_count) / 1000.0);
            info.badge = buf;
        } else {
            info.badge = std::to_string(v_count) + " vtx";
        }
        return info;
    }

    if (const MeshInstance* mi = entities.Get<MeshInstance>(entity)) {
        info.icon = VectorIcon::Mesh;
        info.color = IM_COL32(80, 227, 194, 255); // Emerald Green Mesh
        (void)mi;
        info.badge = "Mesh";
        return info;
    }

    if (const PrimitiveShape* ps = entities.Get<PrimitiveShape>(entity)) {
        info.icon = VectorIcon::Mesh;
        info.color = IM_COL32(80, 227, 194, 255);
        switch (ps->type) {
            case PrimitiveType::Box:      info.badge = "12 tris"; break;
            case PrimitiveType::Sphere:   info.badge = "Sphere"; break;
            case PrimitiveType::Plane:    info.badge = "2 tris"; break;
            case PrimitiveType::Cylinder: info.badge = "Cylinder"; break;
            case PrimitiveType::Cone:     info.badge = "Cone"; break;
            case PrimitiveType::Torus:    info.badge = "Torus"; break;
            default:                      info.badge = "Shape"; break;
        }
        return info;
    }

    if (entities.Get<LightSource>(entity)) {
        info.icon = VectorIcon::Light;
        info.color = IM_COL32(255, 209, 102, 255); // Amber Light
        return info;
    }

    if (entities.Get<CameraComponent>(entity)) {
        info.icon = VectorIcon::Camera;
        info.color = IM_COL32(199, 146, 234, 255); // Purple Camera
        return info;
    }

    if (entities.Get<AudioSourceComponent>(entity)) {
        info.icon = VectorIcon::Audio;
        info.color = IM_COL32(128, 203, 196, 255); // Cyan Audio
        return info;
    }

    if (entities.Get<RigidBody>(entity)) {
        info.icon = VectorIcon::Physics;
        info.color = IM_COL32(255, 138, 101, 255); // Orange Physics
        return info;
    }

    if (entities.Get<TerrainComponent>(entity)) {
        info.icon = VectorIcon::Terrain;
        info.color = IM_COL32(160, 210, 100, 255); // Green Terrain
        return info;
    }

    return info;
}

} // namespace

void EditorUI::DrawSceneGraph(World& world, UiState& ui, Entity current_parent) {
    std::vector<bool> is_last_stack;
    DrawSceneGraphInternal(world, ui, current_parent, 0, is_last_stack);
}

void EditorUI::DrawSceneGraphInternal(World& world, UiState& ui, Entity current_parent, int depth, std::vector<bool>& is_last_stack) {
    Registry& entities = world.Entities();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Collect child entities matching parent / search
    std::vector<Entity> children;
    for (auto [entity, name] : entities.View<Name>().each()) {
        Entity its_parent = kNullEntity;
        if (Parent* p = entities.Get<Parent>(entity)) its_parent = p->entity;

        if (ui.hierarchy_search[0] != '\0') {
            std::string n = name.value;
            std::string s = ui.hierarchy_search;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (n.find(s) == std::string::npos) continue;
            children.push_back(entity);
        } else {
            if (its_parent == current_parent) {
                children.push_back(entity);
            }
        }
    }

    const usize child_count = children.size();
    for (usize idx = 0; idx < child_count; ++idx) {
        Entity entity = children[idx];
        const bool is_last = (idx + 1 == child_count);
        const Name* name_comp = entities.Get<Name>(entity);
        const std::string name_str = name_comp ? name_comp->value : ("Entity_" + std::to_string(entt::to_integral(entity)));

        // Check if entity has sub-children
        bool has_children = false;
        if (ui.hierarchy_search[0] == '\0') {
            for (auto [c, p] : entities.View<Parent>().each()) {
                if (p.entity == entity) { has_children = true; break; }
            }
        }

        const bool is_group = (entities.Get<GroupComponent>(entity) != nullptr);
        const bool is_selected = ui.IsSelected(entity);
        const bool is_collapsed = ui.IsCollapsed(entity);
        const NodeVisualInfo vinfo = GetNodeVisualInfo(entities, entity);

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

        const float row_height = 26.0f;
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        const float avail_width = ImGui::GetContentRegionAvail().x;
        ImVec2 row_min = cursor_pos;
        ImVec2 row_max = ImVec2(row_min.x + avail_width, row_min.y + row_height);

        // Invisible button spanning whole row for interaction & hover
        ImGui::InvisibleButton("##RowBtn", ImVec2(avail_width, row_height));
        const bool is_hovered = ImGui::IsItemHovered();
        const bool is_clicked = ImGui::IsItemClicked();

        // Right side control area reservation: 80px [Checkbox] [Eye] [•••]
        const float right_controls_width = 82.0f;
        const float right_start_x = row_max.x - right_controls_width;

        // Selection & Click handling (exclude click on right-side controls)
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;
        if (is_clicked && mouse_pos.x < right_start_x) {
            const bool multi = ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
            ui.SetSelected(entity, !is_selected || !multi, multi);
        }

        // Draw Row Background Highlight
        if (is_selected) {
            draw_list->AddRectFilled(row_min, row_max, IM_COL32(50, 95, 68, 220), 4.0f);
            draw_list->AddRect(row_min, row_max, IM_COL32(75, 160, 110, 240), 4.0f, 0, 1.0f);
        } else if (is_hovered) {
            draw_list->AddRectFilled(row_min, row_max, IM_COL32(255, 255, 255, 18), 4.0f);
        }

        // Tree Hierarchy Guide Lines
        const float indent_width = 18.0f;
        const float center_y = row_min.y + row_height * 0.5f;

        // Vertical lines for outer ancestor levels
        for (int d = 0; d < depth; ++d) {
            if (d < static_cast<int>(is_last_stack.size()) && !is_last_stack[d]) {
                float line_x = row_min.x + 8.0f + d * indent_width;
                draw_list->AddLine(ImVec2(line_x, row_min.y), ImVec2(line_x, row_max.y), IM_COL32(90, 90, 90, 140), 1.0f);
            }
        }

        // Branch connector for current node
        if (depth > 0) {
            float line_x = row_min.x + 8.0f + (depth - 1) * indent_width;
            draw_list->AddLine(ImVec2(line_x, row_min.y), ImVec2(line_x, center_y), IM_COL32(90, 90, 90, 160), 1.0f);
            if (!is_last) {
                draw_list->AddLine(ImVec2(line_x, center_y), ImVec2(line_x, row_max.y), IM_COL32(90, 90, 90, 160), 1.0f);
            }
            draw_list->AddLine(ImVec2(line_x, center_y), ImVec2(line_x + 10.0f, center_y), IM_COL32(90, 90, 90, 160), 1.0f);
            draw_list->AddCircleFilled(ImVec2(line_x + 10.0f, center_y), 1.5f, IM_COL32(130, 130, 130, 200));
        }

        // Node Expander Chevron / Leaf Dot using Vector Icons
        float cur_x = row_min.x + 6.0f + depth * indent_width;
        if (has_children) {
            ImVec2 chev_min(cur_x - 2.0f, row_min.y);
            ImVec2 chev_max(cur_x + 14.0f, row_max.y);
            const bool chev_hovered = ImGui::IsMouseHoveringRect(chev_min, chev_max);

            if (chev_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ui.ToggleCollapsed(entity);
            }

            DrawVectorIcon(draw_list, is_collapsed ? VectorIcon::RightArrow : VectorIcon::DownArrow,
                           ImVec2(cur_x + 5.0f, center_y), 10.0f,
                           chev_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 220));
            cur_x += 14.0f;
        } else {
            draw_list->AddCircleFilled(ImVec2(cur_x + 4.0f, center_y), 1.5f, IM_COL32(110, 110, 110, 180));
            cur_x += 14.0f;
        }

        // Node Type Vector Icon
        if (vinfo.icon != VectorIcon::None) {
            DrawVectorIcon(draw_list, vinfo.icon, ImVec2(cur_x + 6.0f, center_y), 13.0f, vinfo.color);
            cur_x += 16.0f;
        }

        // Entity Name
        std::string display_name = is_group ? ("[Group] " + name_str) : name_str;
        ImVec2 name_size = ImGui::CalcTextSize(display_name.c_str());
        ImU32 text_col = is_selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(215, 225, 235, 240);
        draw_list->AddText(ImVec2(cur_x, center_y - name_size.y * 0.5f), text_col, display_name.c_str());
        cur_x += name_size.x + 10.0f;

        // Geometry / Info Badge (e.g. 24 vtx, 12 tris, 22.3k)
        if (!vinfo.badge.empty() && cur_x < right_start_x - 10.0f) {
            ImVec2 badge_size = ImGui::CalcTextSize(vinfo.badge.c_str());
            draw_list->AddText(ImVec2(cur_x, center_y - badge_size.y * 0.5f), IM_COL32(130, 140, 150, 200), vinfo.badge.c_str());
        }

        // --- Right-Side Inline Interactive Controls (Pure Vector Drawing) ---
        // 1. Multi-Select Checkbox
        const float chk_x = right_start_x + 4.0f;
        const float btn_w = 20.0f;
        ImVec2 chk_min(chk_x, row_min.y + 4.0f);
        ImVec2 chk_max(chk_x + btn_w, row_max.y - 4.0f);
        const bool chk_hovered = ImGui::IsMouseHoveringRect(chk_min, chk_max);

        if (chk_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ui.SetSelected(entity, !is_selected, true);
        }

        if (is_selected) {
            draw_list->AddRectFilled(chk_min, chk_max, IM_COL32(75, 175, 120, 255), 3.0f);
            DrawVectorIcon(draw_list, VectorIcon::Check, ImVec2(chk_min.x + btn_w * 0.5f, center_y), 11.0f, IM_COL32(255, 255, 255, 255));
        } else {
            draw_list->AddRect(chk_min, chk_max, chk_hovered ? IM_COL32(200, 200, 200, 255) : IM_COL32(120, 120, 120, 180), 3.0f);
        }

        // 2. Visibility Eye (Open / Closed)
        const float eye_x = chk_x + btn_w + 6.0f;
        ImVec2 eye_min(eye_x, row_min.y + 3.0f);
        ImVec2 eye_max(eye_x + btn_w, row_max.y - 3.0f);
        const bool eye_hovered = ImGui::IsMouseHoveringRect(eye_min, eye_max);

        bool is_visible = true;
        if (const Visibility* v = entities.Get<Visibility>(entity)) is_visible = v->visible;

        if (eye_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (Visibility* v = entities.Get<Visibility>(entity)) {
                v->visible = !v->visible;
            } else {
                entities.Add<Visibility>(entity, Visibility{!is_visible});
            }
        }

        DrawVectorIcon(draw_list, is_visible ? VectorIcon::Eye : VectorIcon::EyeClosed,
                       ImVec2(eye_min.x + btn_w * 0.5f, center_y), 13.0f,
                       is_visible ? (eye_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 190, 200, 220))
                                  : IM_COL32(255, 90, 90, 240));

        // 3. Context Menu Button (•••)
        const float opt_x = eye_x + btn_w + 6.0f;
        ImVec2 opt_min(opt_x, row_min.y + 3.0f);
        ImVec2 opt_max(opt_x + btn_w, row_max.y - 3.0f);
        const bool opt_hovered = ImGui::IsMouseHoveringRect(opt_min, opt_max);

        if (opt_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImGui::OpenPopup("EntityContextMenu");
        }

        DrawVectorIcon(draw_list, VectorIcon::More, ImVec2(opt_min.x + btn_w * 0.5f, center_y), 12.0f,
                       opt_hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 160, 170, 200));

        // Context Menu Popup
        if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
            if (!ui.IsSelected(entity)) {
                ui.SetSelected(entity, true, false);
            }
            if (ImGui::MenuItem(TR("action_group", "Group (Ctrl+G)"), "Ctrl+G", false, ui.selections.size() > 1 || ui.selection != kNullEntity)) {
                m_commands.Execute(std::make_unique<GroupEntitiesCommand>(entities, ui.selections.empty() ? std::vector<Entity>{ui.selection} : ui.selections));
            }
            if (is_group && ImGui::MenuItem(TR("action_ungroup", "Ungroup (Ctrl+Alt+G)"), "Ctrl+Alt+G")) {
                m_commands.Execute(std::make_unique<UngroupEntitiesCommand>(entities, entity));
            }
            if (ui.selections.size() >= 2 && ImGui::MenuItem(TR("action_join", "Join Meshes (Ctrl+J)"), "Ctrl+J")) {
                m_commands.Execute(std::make_unique<JoinMeshesCommand>(entities, ui.selections));
            }
            ImGui::Separator();
            Entity its_parent = kNullEntity;
            if (Parent* p = entities.Get<Parent>(entity)) its_parent = p->entity;
            if (ImGui::MenuItem("Unparent", nullptr, false, its_parent != kNullEntity)) {
                entities.Remove<Parent>(entity);
            }
            if (ImGui::MenuItem(TR("action_duplicate", "Duplicate"), "Cmd+D")) {
                EntitySnapshot snap = EntitySnapshot::Capture(entities, entity);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(entities);
                ui.SetSelected(dup, true, false);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, dup, snap, "Duplicate Entity"));
            }
            ImGui::Separator();
            if (ImGui::MenuItem(TR("action_delete", "Delete"), "Del")) {
                for (Entity to_del : ui.selections) {
                    if (entities.Valid(to_del)) {
                        m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Entity"));
                    }
                }
                ui.DeselectAll();
            }
            ImGui::EndPopup();
        }

        // Drag & Drop
        if (ImGui::BeginDragDropSource()) {
            Entity dragged = entity;
            ImGui::SetDragDropPayload("ENTITY_PAYLOAD", &dragged, sizeof(Entity));
            ImGui::TextUnformatted(name_str.c_str());
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
                Entity dropped = *static_cast<const Entity*>(payload->Data);
                if (dropped != entity) {
                    Entity before_parent = kNullEntity;
                    if (const Parent* p = entities.Get<Parent>(dropped)) before_parent = p->entity;
                    m_commands.Execute(std::make_unique<ReparentCommand>(entities, dropped, before_parent, entity));
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Recurse into children if node is not collapsed
        if (has_children && !is_collapsed && ui.hierarchy_search[0] == '\0') {
            is_last_stack.push_back(is_last);
            DrawSceneGraphInternal(world, ui, entity, depth + 1, is_last_stack);
            is_last_stack.pop_back();
        }

        ImGui::PopID();
    }
}

void EditorUI::DrawHierarchy(World& world, UiState& ui, SceneAssets& assets) {
    (void)assets;
    if (!ImGui::Begin("Hierarchy", &ui.show_hierarchy)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();

    // Top Quick Action Grid Toolbar (Vector Icons with Animated Tween Buttons)
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float btn_w = (avail_w - 3.0f * 4.0f) / 4.0f;
    const ImVec2 tool_btn_size(btn_w, 24.0f);

    const bool has_sel = (ui.selection != kNullEntity || !ui.selections.empty());
    const bool is_group_sel = (ui.selection != kNullEntity && entities.Get<GroupComponent>(ui.selection) != nullptr);

    // Row 1: Add | Delete | Join | Separate
    if (VectorIconButton("tb_add", VectorIcon::Plus, TR("action_add", "Add..."), tool_btn_size, 0)) {
        ImGui::OpenPopup("AddEntityPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_add", "Create new entity or primitive..."));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_del", VectorIcon::Cross, TR("action_delete_short", "Delete"), tool_btn_size, has_sel ? IM_COL32(160, 45, 45, 230) : 0) && has_sel) {
        for (Entity to_del : ui.selections) {
            if (entities.Valid(to_del)) {
                m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Selection"));
            }
        }
        ui.DeselectAll();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_del", "Delete selected entities (Del)"));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_join", VectorIcon::Join, TR("action_join_short", "Join"), tool_btn_size, ui.selections.size() >= 2 ? IM_COL32(38, 145, 75, 230) : 0) && ui.selections.size() >= 2) {
        m_commands.Execute(std::make_unique<JoinMeshesCommand>(entities, ui.selections));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_join", "Join selected meshes into one entity (Ctrl+J)"));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_sep", VectorIcon::Separate, TR("action_separate_short", "Separate"), tool_btn_size, 0) && has_sel) {
        for (Entity e : ui.selections) {
            entities.Remove<Parent>(e);
        }
        UpdateWorldTransforms(entities);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_unparent", "Unparent from parent entity"));

    // Row 2: Boolean | Copy | Group | Ungroup
    if (VectorIconButton("tb_bool", VectorIcon::Boolean, TR("action_boolean", "Boolean..."), tool_btn_size, 0)) {
        ImGui::OpenPopup("BooleanModifierPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_boolean", "CSG Boolean operations (Union, Difference, Intersection)"));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_copy", VectorIcon::Copy, TR("action_copy", "Copy"), tool_btn_size, 0) && has_sel) {
        std::vector<Entity> new_selections;
        for (Entity e : ui.selections) {
            if (entities.Valid(e)) {
                EntitySnapshot snap = EntitySnapshot::Capture(entities, e);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(entities);
                new_selections.push_back(dup);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, dup, snap, "Duplicate Entity"));
            }
        }
        ui.selections = new_selections;
        ui.selection = ui.selections.empty() ? kNullEntity : ui.selections.front();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_duplicate", "Duplicate selection (Cmd+D / Ctrl+D)"));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_grp", VectorIcon::Group, TR("action_group_short", "Group"), tool_btn_size, has_sel ? IM_COL32(35, 110, 180, 230) : 0) && has_sel) {
        m_commands.Execute(std::make_unique<GroupEntitiesCommand>(entities, ui.selections.empty() ? std::vector<Entity>{ui.selection} : ui.selections));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_group", "Group selected entities (Ctrl+G)"));

    ImGui::SameLine(0.0f, 4.0f);
    if (VectorIconButton("tb_ungrp", VectorIcon::Ungroup, TR("action_ungroup_short", "Ungroup"), tool_btn_size, is_group_sel ? IM_COL32(180, 110, 35, 230) : 0) && is_group_sel) {
        m_commands.Execute(std::make_unique<UngroupEntitiesCommand>(entities, ui.selection));
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tooltip_ungroup", "Ungroup group (Ctrl+Alt+G)"));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Header Bar: "Frame / Scene" with Question Help & Search
    ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "%s", TR("tree_frame", "Frame / Scene"));
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", entities.Count());
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("tooltip_shortcuts",
            "Shortcuts:\n"
            " • A: Select All\n"
            " • Alt+A: Deselect All\n"
            " • Shift+Click: Multi-select\n"
            " • Ctrl+G: Group\n"
            " • Ctrl+Alt+G: Ungroup\n"
            " • Ctrl+J: Join Meshes\n"
            " • Cmd+D / Ctrl+D: Duplicate\n"
            " • Del: Delete"
        ));
    }

    // Hierarchy Search & Filter Bar
    ImGui::SetNextItemWidth(-28.0f);
    ImGui::InputTextWithHint("##HierarchySearch", TR("search_entities", "Search entities..."), ui.hierarchy_search, sizeof(ui.hierarchy_search));
    ImGui::SameLine();
    if (ImGui::SmallButton("X")) {
        ui.hierarchy_search[0] = '\0';
    }

    ImGui::Spacing();

    // Add Entity Full Archetypes Popup
    if (ImGui::BeginPopup("AddEntityPopup")) {
        if (ImGui::MenuItem("Empty Entity")) {
            Entity created = entities.Create();
            entities.Add<Name>(created, "Entity_" + std::to_string(entt::to_integral(created)));
            entities.Add<LocalTransform>(created);
            entities.Add<WorldTransform>(created);
            entities.Add<Visibility>(created, true);
            ui.selection = created;
            EntitySnapshot snap = EntitySnapshot::Capture(entities, created);
            m_commands.Push(std::make_unique<CreateEntityCommand>(entities, created, snap, "Create Empty Entity"));
        }
        ImGui::Separator();

        if (ImGui::BeginMenu("3D Primitives")) {
            auto create_primitive = [&](const char* name, PrimitiveType type, const Vec3& size = Vec3(1.0f)) {
                Entity created = entities.Create();
                entities.Add<Name>(created, name);
                entities.Add<LocalTransform>(created);
                entities.Add<WorldTransform>(created);
                entities.Add<Visibility>(created, true);
                entities.Add<PrimitiveShape>(created, type, size);
                entities.Add<MaterialRef>(created, 0);
                entities.Add<LocalBounds>(created, -size, size);
                ui.selection = created;
                EntitySnapshot snap = EntitySnapshot::Capture(entities, created);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, created, snap, std::string("Create ") + name));
            };

            if (ImGui::MenuItem("Cube / Box"))        create_primitive("Box", PrimitiveType::Box, Vec3(0.5f));
            if (ImGui::MenuItem("Sphere"))            create_primitive("Sphere", PrimitiveType::Sphere, Vec3(0.5f));
            if (ImGui::MenuItem("Plane / Quad"))      create_primitive("Plane", PrimitiveType::Plane, Vec3(5.0f, 0.0f, 5.0f));
            if (ImGui::MenuItem("Cylinder"))          create_primitive("Cylinder", PrimitiveType::Cylinder, Vec3(0.5f, 1.0f, 0.5f));
            if (ImGui::MenuItem("Cone"))              create_primitive("Cone", PrimitiveType::Cone, Vec3(0.5f, 1.0f, 0.5f));
            if (ImGui::MenuItem("Torus"))             create_primitive("Torus", PrimitiveType::Torus, Vec3(0.6f, 0.2f, 0.6f));
            if (ImGui::MenuItem("Disk"))              create_primitive("Disk", PrimitiveType::Disk, Vec3(0.5f));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Lighting & Atmosphere")) {
            if (ImGui::MenuItem("Point Light")) {
                Entity created = entities.Create();
                entities.Add<Name>(created, "PointLight");
                entities.Add<LocalTransform>(created, Vec3(0.0f, 3.0f, 0.0f));
                entities.Add<WorldTransform>(created);
                entities.Add<Visibility>(created, true);
                LightSource ls{};
                ls.type = LightType::Point;
                ls.color = Vec3(1.0f, 0.95f, 0.85f);
                ls.intensity = 15.0f;
                ls.radius = 10.0f;
                entities.Add<LightSource>(created, ls);
                ui.selection = created;
                EntitySnapshot snap = EntitySnapshot::Capture(entities, created);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, created, snap, "Create Point Light"));
            }
            if (ImGui::MenuItem("Directional Light (Sun)")) ui.selection = Prefab::CreateDirectionalLightNode(world, Vec3(1,-2,1), Vec3(1,0.95f,0.85f), 10.0f);
            if (ImGui::MenuItem("Spot Light")) {
                Entity created = entities.Create();
                entities.Add<Name>(created, "SpotLight");
                entities.Add<LocalTransform>(created, Vec3(0.0f, 5.0f, 0.0f));
                entities.Add<WorldTransform>(created);
                entities.Add<Visibility>(created, true);
                LightSource ls{};
                ls.type = LightType::Spot;
                ls.direction = Vec3(0.0f, -1.0f, 0.0f);
                ls.color = Vec3(1.0f, 1.0f, 1.0f);
                ls.intensity = 25.0f;
                ls.inner_angle = 20.0f;
                ls.outer_angle = 35.0f;
                entities.Add<LightSource>(created, ls);
                ui.selection = created;
                EntitySnapshot snap = EntitySnapshot::Capture(entities, created);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, created, snap, "Create Spot Light"));
            }
            if (ImGui::MenuItem("Fog Volume (Atmosphere)")) ui.selection = Prefab::CreateFogVolumeNode(world, Vec3(0,0,0), Vec3(50,20,50));
            if (ImGui::MenuItem("Post-Process Volume"))     ui.selection = Prefab::CreatePostProcessVolumeNode(world);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Cameras & Rigs")) {
            if (ImGui::MenuItem("Perspective Camera")) {
                Entity created = entities.Create();
                entities.Add<Name>(created, "Camera");
                entities.Add<LocalTransform>(created, Vec3(0.0f, 2.0f, 5.0f));
                entities.Add<WorldTransform>(created);
                entities.Add<Visibility>(created, true);
                CameraComponent cam{};
                cam.fov = 60.0f;
                cam.near_clip = 0.1f;
                cam.far_clip = 1000.0f;
                entities.Add<CameraComponent>(created, cam);
                ui.selection = created;
                EntitySnapshot snap = EntitySnapshot::Capture(entities, created);
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, created, snap, "Create Camera"));
            }
            if (ImGui::MenuItem("Cinematic Camera (85mm)")) ui.selection = Prefab::CreateCinematicCameraNode(world, Vec3(0,1.5f,4.0f));
            if (ImGui::MenuItem("Spring Arm Rig (Boom)"))   ui.selection = Prefab::CreateSpringArmNode(world, Vec3(0,1.5f,0), 4.5f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Environment & World")) {
            if (ImGui::MenuItem("Terrain Generator"))      ui.selection = Prefab::CreateTerrainNode(world);
            if (ImGui::MenuItem("Water Body (Ocean)"))     ui.selection = Prefab::CreateWaterBodyNode(world, Vec3(0,-0.5f,0), Vec2(100,100));
            if (ImGui::MenuItem("River Spline Node"))      ui.selection = Prefab::CreateRiverNode(world, {Vec3(-20,0,-20), Vec3(0,0,0), Vec3(20,0,20)});
            if (ImGui::MenuItem("Foliage Instancer"))      ui.selection = Prefab::CreateFoliageNode(world, "assets/models/grass.obj", 500);
            if (ImGui::MenuItem("Wind Source Node"))       ui.selection = Prefab::CreateWindSourceNode(world, Vec3(1,0,0), 5.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Physics & Sensors")) {
            if (ImGui::MenuItem("Dynamic Box Actor"))      ui.selection = Prefab::CreatePhysicsActorNode(world, PrimitiveType::Box, BodyType::Dynamic, Vec3(0,3,0));
            if (ImGui::MenuItem("Dynamic Sphere Actor"))   ui.selection = Prefab::CreatePhysicsActorNode(world, PrimitiveType::Sphere, BodyType::Dynamic, Vec3(0,4,0));
            if (ImGui::MenuItem("Trigger Volume (Sensor)")) ui.selection = Prefab::CreateTriggerVolumeNode(world, Vec3(0,1,0), Vec3(1,1,1));
            if (ImGui::MenuItem("Raycast Sensor Node"))    ui.selection = Prefab::CreateRaycastSensorNode(world, Vec3(0,1,0), Vec3(0,-1,0), 10.0f);
            if (ImGui::MenuItem("Physics Joint (Hinge)"))  ui.selection = Prefab::CreatePhysicsJointNode(world, JointType::Hinge, Vec3(0,2,0));
            if (ImGui::MenuItem("Buoyancy Actor Node"))    ui.selection = Prefab::CreateBuoyancyNode(world, Vec3(0,1,0), 0.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Vehicles")) {
            if (ImGui::MenuItem("Muscle Car (Wheeled)"))   ui.selection = Prefab::CreateWheeledVehicleNode(world, Vec3(0,1,0), "WheeledCar");
            if (ImGui::MenuItem("Tank (Tracked)"))         ui.selection = Prefab::CreateTrackedVehicleNode(world, Vec3(0,1,0), "Tank");
            if (ImGui::MenuItem("Aircraft"))               ui.selection = Prefab::CreateAircraftNode(world, Vec3(0,10,0));
            if (ImGui::MenuItem("Watercraft / Boat"))      ui.selection = Prefab::CreateWatercraftNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Vehicle Wheel"))          ui.selection = Prefab::CreateVehicleWheelNode(world, Vec3(0.8f,0.35f,1.5f));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Characters & AI")) {
            if (ImGui::MenuItem("Player Pawn"))            ui.selection = Prefab::CreatePawnNode(world, Vec3(0, 1.8f, 5.0f));
            if (ImGui::MenuItem("Character Body (Humanoid)")) ui.selection = Prefab::CreateCharacterBodyNode(world, Vec3(0, 1.0f, 0.0f));
            if (ImGui::MenuItem("AI Enemy / Controller"))  ui.selection = Prefab::CreateAIControllerNode(world, Vec3(0, 1.0f, 0.0f));
            if (ImGui::MenuItem("Player Input Node"))      ui.selection = Prefab::CreatePlayerInputNode(world);
            if (ImGui::MenuItem("Ragdoll Node"))           ui.selection = Prefab::CreateRagdollNode(world, Vec3(0, 1.0f, 0.0f));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("VFX & Audio")) {
            if (ImGui::MenuItem("Particle Emitter"))       ui.selection = Prefab::CreateParticleEmitterNode(world, Vec3(0,1,0));
            if (ImGui::MenuItem("VFX Graph Node"))         ui.selection = Prefab::CreateVFXGraphNode(world, "fire_sparks.vfx");
            if (ImGui::MenuItem("Trail Effect"))           ui.selection = Prefab::CreateTrailNode(world, Vec3(0,1,0));
            if (ImGui::MenuItem("Beam / Laser Emitter"))   ui.selection = Prefab::CreateBeamEmitterNode(world, Vec3(0,1,0), Vec3(0,1,10));
            ImGui::Separator();
            if (ImGui::MenuItem("Spatial Audio Source"))   ui.selection = Prefab::CreateAudioSourceNode(world, "assets/sound/sfx.wav", Vec3(0,1,0));
            if (ImGui::MenuItem("Audio Listener Node"))    ui.selection = Prefab::CreateAudioListenerNode(world, Vec3(0,1.8f,0));
            if (ImGui::MenuItem("Audio Reverb Zone"))      ui.selection = Prefab::CreateAudioReverbZoneNode(world, Vec3(0,0,0), 20.0f);
            if (ImGui::MenuItem("Music Track Node"))       ui.selection = Prefab::CreateMusicTrackNode(world, "assets/audio/combat_theme.ogg");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("UI & HUD")) {
            if (ImGui::MenuItem("Canvas Layer"))           ui.selection = Prefab::CreateCanvasLayerNode(world, 0);
            if (ImGui::MenuItem("UI Panel"))               ui.selection = Prefab::CreateUIPanelNode(world, Vec2(200,150));
            if (ImGui::MenuItem("UI Container"))           ui.selection = Prefab::CreateUIContainerNode(world);
            if (ImGui::MenuItem("UI Button"))              ui.selection = Prefab::CreateUIButtonNode(world, "Play");
            if (ImGui::MenuItem("UI Label"))               ui.selection = Prefab::CreateUILabelNode(world, "Score: 0");
            if (ImGui::MenuItem("UI Image"))               ui.selection = Prefab::CreateUIImageNode(world, "icon.png");
            if (ImGui::MenuItem("World Space UI (Healthbar)")) ui.selection = Prefab::CreateWorldSpaceUINode(world, Vec3(0,2,0), "Boss Health");
            if (ImGui::MenuItem("Mini Map Node"))          ui.selection = Prefab::CreateMiniMapNode(world);
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("BooleanModifierPopup")) {
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "CSG Boolean Modifiers");
        ImGui::Separator();
        if (ImGui::MenuItem(TR("boolean_union", "Union"))) {
            Prefab::CreateCSGNode(world, CSGOperation::Union);
        }
        if (ImGui::MenuItem(TR("boolean_difference", "Difference"))) {
            Prefab::CreateCSGNode(world, CSGOperation::Difference);
        }
        if (ImGui::MenuItem(TR("boolean_intersection", "Intersection"))) {
            Prefab::CreateCSGNode(world, CSGOperation::Intersection);
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Draw Recursive Tree Graph Nodes
    DrawSceneGraph(world, ui, kNullEntity);

    // Drop on empty space to unparent
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
            Entity dropped = *static_cast<const Entity*>(payload->Data);
            entities.Remove<Parent>(dropped);
            UpdateWorldTransforms(entities);
        }
        ImGui::EndDragDropTarget();
    }

    if (entities.Count() == 0) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", TR("empty_scene", "Empty scene."));
        ImGui::TextDisabled("%s", TR("empty_scene_hint", "Click '+ Add...' or import a 3D model."));
    }

    ImGui::End();
}

} // namespace lucida
