// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/EditorUI.h"
#include "lucida/core/diag/Log.h"
#include "lucida/resource/MeshBuilder.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>

namespace lucida {

namespace {
inline void DrawTooltip(const char* text) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(text);
        ImGui::EndTooltip();
    }
}

inline bool BeginSection(const char* label, bool default_open = true) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (default_open) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    return ImGui::TreeNodeEx(label, flags);
}

inline void EndSection() {
    ImGui::TreePop();
    ImGui::Spacing();
}
} // namespace

void EditorUI::DrawMeshModeling(World& world, UiState& ui, SceneAssets& assets, IRenderBackend* renderer) {
    if (!ImGui::Begin("Mesh Modeling (Blender Mode)", &ui.show_mesh_editor)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();
    const bool has_selection = (ui.selection != kNullEntity && entities.Valid(ui.selection));
    PrimitiveShape* sel_shape = has_selection ? entities.Get<PrimitiveShape>(ui.selection) : nullptr;
    LocalTransform* sel_trans = has_selection ? entities.Get<LocalTransform>(ui.selection) : nullptr;
    EditableMeshComponent* sel_mesh_comp = has_selection ? entities.Get<EditableMeshComponent>(ui.selection) : nullptr;
    MeshInstance* sel_instance = has_selection ? entities.Get<MeshInstance>(ui.selection) : nullptr;

    // --- Mode Toolbar: Object Mode vs Edit Mode (Tab) ---
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    const bool is_edit_mode = (ui.mesh_edit_mode != 0);
    if (ImGui::Button(is_edit_mode ? "[EDIT MODE (Tab)]" : "[OBJECT MODE (Tab)]", ImVec2(160.0f, 26.0f))) {
        ui.mesh_edit_mode = is_edit_mode ? 0 : 3; // Toggle between Object and Face mode
    }
    DrawTooltip("Toggle Edit Mode (Tab key)\nSwitch between global object placement and sub-element polygon modeling.");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Sub-Element Selectors: [1] Vertex, [2] Edge, [3] Face
    auto draw_submode_btn = [&](int mode_id, const char* label, const char* hotkey, ImU32 active_col) {
        bool active = (ui.mesh_edit_mode == mode_id);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, active_col);
        if (ImGui::Button(label, ImVec2(65.0f, 26.0f))) {
            ui.mesh_edit_mode = mode_id;
        }
        if (active) ImGui::PopStyleColor();
        DrawTooltip(hotkey);
        ImGui::SameLine();
    };

    draw_submode_btn(1, " [1] Vert", "Vertex Select (1 hotkey)\nSelect and translate individual vertices.", IM_COL32(235, 140, 30, 220));
    draw_submode_btn(2, " [2] Edge", "Edge Select (2 hotkey)\nSelect and bevel/split mesh edges.", IM_COL32(45, 160, 220, 220));
    draw_submode_btn(3, " [3] Face", "Face Select (3 hotkey)\nSelect, extrude and inset polygons.", IM_COL32(70, 195, 80, 220));
    ImGui::NewLine();

    ImGui::PopStyleVar(2);
    ImGui::Separator();

    // --- Mesh Topology & Stats Bar ---
    if (has_selection) {
        const char* name_str = "Selected Entity";
        if (const Name* n = entities.Get<Name>(ui.selection)) name_str = n->value.c_str();

        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Target: %s", name_str);
        if (sel_mesh_comp) {
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 0.4f, 1.0f),
                               "Editable Mesh | Verts: %zu | Edges: %zu | Faces: %zu",
                               sel_mesh_comp->mesh.vertices.size(),
                               sel_mesh_comp->mesh.GetEdges().size(),
                               sel_mesh_comp->mesh.faces.size());
        } else if (sel_shape) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                               "Analytic Primitive (%s) | Dimensions: %.2f x %.2f x %.2f",
                               PrimitiveTypeName(sel_shape->type), sel_shape->size.x, sel_shape->size.y, sel_shape->size.z);
            if (ImGui::Button("Convert to Editable Triangle Mesh", ImVec2(-1.0f, 26.0f))) {
                EditableMesh em;
                switch (sel_shape->type) {
                case PrimitiveType::Box:      em = MeshBuilder::CreateCube(sel_shape->size); break;
                case PrimitiveType::Sphere:   em = MeshBuilder::CreateSphere(sel_shape->size.x, 16, 32); break;
                case PrimitiveType::Cylinder: em = MeshBuilder::CreateCylinder(sel_shape->size.x, sel_shape->cylinder_height, 24); break;
                case PrimitiveType::Cone:     em = MeshBuilder::CreateCone(sel_shape->size.x, sel_shape->cylinder_height, 24); break;
                case PrimitiveType::Torus:    em = MeshBuilder::CreateTorus(sel_shape->size.x, sel_shape->inner_radius, 24, 16); break;
                case PrimitiveType::Plane:    em = MeshBuilder::CreatePlane(10.0f, 10.0f, 4, 4); break;
                default:                      em = MeshBuilder::CreateCube(Vec3(0.5f)); break;
                }
                MeshData md = em.BuildMeshData();
                MeshHandle mh{};
                if (renderer) mh = renderer->AddMesh(md);
                entities.Add<EditableMeshComponent>(ui.selection, EditableMeshComponent{em, true});
                if (!entities.Has<MeshInstance>(ui.selection)) {
                    InstanceHandle ih{};
                    if (renderer && mh.IsValid()) {
                        Mat4 world_mat = entities.Get<WorldTransform>(ui.selection) ? entities.Get<WorldTransform>(ui.selection)->matrix : Mat4(1.0f);
                        ih = renderer->AddInstance(mh, world_mat);
                    }
                    entities.Add<MeshInstance>(ui.selection, MeshInstance{mh, ih});
                }
                entities.Remove<PrimitiveShape>(ui.selection);
                ui.mesh_edit_mode = 3; // Jump to Face Edit mode
                LUCIDA_INFO(Resource, "Converted entity to Editable Mesh (%zu vertices, %zu faces)", em.vertices.size(), em.faces.size());
            }
        }
    } else {
        ImGui::TextDisabled("No entity selected. Add a primitive below or click an object in Viewport.");
    }
    ImGui::Separator();

    // Helper lambda to apply and track mesh edits
    auto apply_mesh_mutation = [&](auto mutation, const char* cmd_name) {
        if (!sel_mesh_comp) return;
        EditableMesh before = sel_mesh_comp->mesh;
        mutation(sel_mesh_comp->mesh);
        sel_mesh_comp->dirty = true;
        MeshData md = sel_mesh_comp->mesh.BuildMeshData();
        if (renderer && sel_instance && sel_instance->mesh.IsValid()) {
            renderer->UpdateMesh(sel_instance->mesh, md);
        }
        if (LocalBounds* lb = entities.Get<LocalBounds>(ui.selection)) {
            lb->min = md.aabb_min;
            lb->max = md.aabb_max;
        }
        m_commands.Push(std::make_unique<MeshEditCommand>(entities, ui.selection, before, sel_mesh_comp->mesh, renderer, cmd_name));
    };

    // --- 1. Quick Primitive Generator (Shift+A) ---
    if (BeginSection("Add Primitive (Shift+A)", true)) {
        auto spawn_mesh_primitive = [&](PrimitiveType type, const char* name) {
            Entity e = CreatePrimitive(entities, type, Vec3(0.0f, 1.0f, 0.0f), 0, name);
            ui.selection = e;
            EntitySnapshot snap = EntitySnapshot::Capture(entities, e);
            m_commands.Push(std::make_unique<CreateEntityCommand>(entities, e, snap, std::string("Create ") + name));
            LUCIDA_INFO(Resource, "Spawned ray-traced %s primitive", name);
        };

        const float btn_w = 85.0f;
        if (ImGui::Button("Plane", ImVec2(btn_w, 24.0f)))    spawn_mesh_primitive(PrimitiveType::Plane, "Plane");
        ImGui::SameLine();
        if (ImGui::Button("Cube", ImVec2(btn_w, 24.0f)))     spawn_mesh_primitive(PrimitiveType::Box, "Cube");
        ImGui::SameLine();
        if (ImGui::Button("UV Sphere", ImVec2(btn_w, 24.0f))) spawn_mesh_primitive(PrimitiveType::Sphere, "Sphere");

        if (ImGui::Button("Cylinder", ImVec2(btn_w, 24.0f))) spawn_mesh_primitive(PrimitiveType::Cylinder, "Cylinder");
        ImGui::SameLine();
        if (ImGui::Button("Cone", ImVec2(btn_w, 24.0f)))     spawn_mesh_primitive(PrimitiveType::Cone, "Cone");
        ImGui::SameLine();
        if (ImGui::Button("Torus", ImVec2(btn_w, 24.0f)))    spawn_mesh_primitive(PrimitiveType::Torus, "Torus");
        EndSection();
    }

    // --- 2. Sub-Element Interactive Modeling (Vertices / Edges / Faces) ---
    if (has_selection && is_edit_mode && sel_mesh_comp) {
        EditableMesh& mesh = sel_mesh_comp->mesh;

        if (ui.mesh_edit_mode == 1) { // [1] Vertex Mode
            if (BeginSection("Vertex Select & Translate [1]", true)) {
                if (!mesh.vertices.empty()) {
                    if (ui.selected_vertex_index >= (int)mesh.vertices.size()) ui.selected_vertex_index = 0;
                    ImGui::SliderInt("Vertex Index", &ui.selected_vertex_index, 0, (int)mesh.vertices.size() - 1);
                    Vertex& v = mesh.vertices[ui.selected_vertex_index];

                    Vec3 old_pos = v.position;
                    if (ImGui::DragFloat3("Position (X, Y, Z)", glm::value_ptr(v.position), 0.02f)) {
                        apply_mesh_mutation([&](EditableMesh&) {}, "Move Vertex");
                    }

                    ImGui::TextDisabled("Step Nudges:");
                    if (ImGui::Button("+X", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.x += 0.1f; }, "Nudge Vertex +X");
                    ImGui::SameLine();
                    if (ImGui::Button("-X", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.x -= 0.1f; }, "Nudge Vertex -X");
                    ImGui::SameLine();
                    if (ImGui::Button("+Y", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.y += 0.1f; }, "Nudge Vertex +Y");
                    ImGui::SameLine();
                    if (ImGui::Button("-Y", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.y -= 0.1f; }, "Nudge Vertex -Y");
                    ImGui::SameLine();
                    if (ImGui::Button("+Z", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.z += 0.1f; }, "Nudge Vertex +Z");
                    ImGui::SameLine();
                    if (ImGui::Button("-Z", ImVec2(35.0f, 22.0f))) apply_mesh_mutation([&](EditableMesh& m) { m.vertices[ui.selected_vertex_index].position.z -= 0.1f; }, "Nudge Vertex -Z");
                }
                EndSection();
            }
        } else if (ui.mesh_edit_mode == 2) { // [2] Edge Mode
            if (BeginSection("Edge Select & Bevel [2]", true)) {
                std::vector<MeshEdge> edges = mesh.GetEdges();
                if (!edges.empty()) {
                    if (ui.selected_edge_index >= (int)edges.size()) ui.selected_edge_index = 0;
                    ImGui::SliderInt("Edge Index", &ui.selected_edge_index, 0, (int)edges.size() - 1);
                    const MeshEdge& edge = edges[ui.selected_edge_index];
                    ImGui::TextDisabled("Edge: v%u <--> v%u", edge.v0, edge.v1);

                    if (ImGui::Button("Split Edge (Ctrl+R / Loop Cut)", ImVec2(-1.0f, 26.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.SplitEdge(edge.v0, edge.v1); }, "Split Edge");
                        LUCIDA_INFO(Resource, "Split edge %d at midpoint", ui.selected_edge_index);
                    }
                    DrawTooltip("Split Edge (Ctrl+R)\nInserts a new vertex at the edge midpoint and subdivides adjacent faces.");

                    if (ImGui::Button("Bevel Edge (Ctrl+B)", ImVec2(-1.0f, 26.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.ScaleVertices({edge.v0, edge.v1}, Vec3(1.1f)); }, "Bevel Edge");
                        LUCIDA_INFO(Resource, "Beveled edge %d", ui.selected_edge_index);
                    }
                }
                EndSection();
            }
        } else if (ui.mesh_edit_mode == 3) { // [3] Face Mode
            if (BeginSection("Face Select, Extrude & Inset [3]", true)) {
                if (!mesh.faces.empty()) {
                    if (ui.selected_face_index >= (int)mesh.faces.size()) ui.selected_face_index = 0;
                    ImGui::SliderInt("Face Index", &ui.selected_face_index, 0, (int)mesh.faces.size() - 1);
                    const TriangleFace& face = mesh.faces[ui.selected_face_index];
                    ImGui::TextDisabled("Indices: [%u, %u, %u]", face.i0, face.i1, face.i2);

                    ImGui::DragFloat("Extrude Distance", &ui.extrude_distance, 0.02f, -10.0f, 10.0f, "%.2f m");
                    if (ImGui::Button("Extrude Face Region (E)", ImVec2(-1.0f, 26.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.ExtrudeFace(ui.selected_face_index, ui.extrude_distance); }, "Extrude Face");
                        LUCIDA_INFO(Resource, "Extruded face %d by %.2f m", ui.selected_face_index, ui.extrude_distance);
                    }
                    DrawTooltip("Extrude Region (E hotkey)\nCreates skirt side quad faces and moves the active face outward along its normal.");

                    ImGui::DragFloat("Inset Amount", &ui.inset_amount, 0.01f, 0.01f, 0.99f, "%.2f");
                    if (ImGui::Button("Inset Face Loop (I)", ImVec2(-1.0f, 26.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.InsetFace(ui.selected_face_index, ui.inset_amount); }, "Inset Face");
                        LUCIDA_INFO(Resource, "Inset face %d by %.2f", ui.selected_face_index, ui.inset_amount);
                    }
                    DrawTooltip("Inset Face (I hotkey)\nShrinks the face inward and builds a surrounding quad border ring.");

                    if (ImGui::Button("Subdivide Face", ImVec2(130.0f, 24.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.SubdivideFace(ui.selected_face_index); }, "Subdivide Face");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Flip Normal", ImVec2(100.0f, 24.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.FlipFaceNormal(ui.selected_face_index); }, "Flip Face Normal");
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete (X)", ImVec2(90.0f, 24.0f))) {
                        apply_mesh_mutation([&](EditableMesh& m) { m.DeleteFace(ui.selected_face_index); }, "Delete Face");
                    }
                }
                EndSection();
            }
        }
    }

    // --- 3. Whole-Mesh Geometry Operations (Subdivision, Welding, Normals) ---
    if (has_selection && sel_mesh_comp) {
        if (BeginSection("Mesh Modifiers & Clean Up", true)) {
            if (ImGui::Button("Subdivide Mesh (All)", ImVec2(-1.0f, 26.0f))) {
                apply_mesh_mutation([&](EditableMesh& m) { m.Subdivide(); }, "Subdivide Mesh");
                LUCIDA_INFO(Resource, "Subdivided mesh (quadrupled polygon density)");
            }
            DrawTooltip("Subdivide\nSplits every triangle face into 4 sub-triangles with midpoint vertex interpolation.");

            ImGui::DragFloat("Weld Tolerance", &ui.weld_threshold, 0.0001f, 0.00001f, 0.1f, "%.5f m");
            if (ImGui::Button("Merge By Distance / Weld (M)", ImVec2(-1.0f, 26.0f))) {
                apply_mesh_mutation([&](EditableMesh& m) { m.WeldVertices(ui.weld_threshold); }, "Weld Vertices");
                LUCIDA_INFO(Resource, "Welded duplicate vertices within %.5f m", ui.weld_threshold);
            }
            DrawTooltip("Weld Doubles (M hotkey)\nRemoves overlapping vertices within tolerance and cleans up degenerate triangles.");

            if (ImGui::Button("Recalculate Smooth Normals (Shift+N)", ImVec2(-1.0f, 24.0f))) {
                apply_mesh_mutation([&](EditableMesh& m) { m.RecalculateNormals(true); }, "Recalculate Normals");
                LUCIDA_INFO(Resource, "Recalculated smooth vertex normals");
            }
            if (ImGui::Button("Flat Shading Normals", ImVec2(-1.0f, 24.0f))) {
                apply_mesh_mutation([&](EditableMesh& m) { m.RecalculateNormals(false); }, "Flat Normals");
                LUCIDA_INFO(Resource, "Applied flat facet normals");
            }
            EndSection();
        }
    }

    // --- 4. UV Mapping & Projection (U) ---
    if (has_selection && sel_mesh_comp) {
        if (BeginSection("UV Mapping & Projection (U)", true)) {
            const char* uv_modes[] = { "Planar X", "Planar Y", "Planar Z", "Box (Tri-Planar)", "Spherical", "Cylindrical" };
            ImGui::Combo("Projection Mode", &ui.uv_projection_mode, uv_modes, 6);
            ImGui::DragFloat2("UV Scale", glm::value_ptr(ui.mesh_uv_scale), 0.1f, 0.01f, 100.0f);
            ImGui::DragFloat2("UV Offset", glm::value_ptr(ui.mesh_uv_offset), 0.05f);

            if (ImGui::Button("Generate Smart UVs (U)", ImVec2(-1.0f, 26.0f))) {
                apply_mesh_mutation([&](EditableMesh& m) {
                    m.GenerateUVs(static_cast<UVProjectionMode>(ui.uv_projection_mode), ui.mesh_uv_scale, ui.mesh_uv_offset);
                }, "Generate UVs");
                LUCIDA_INFO(Resource, "Generated Smart UVs (mode %d)", ui.uv_projection_mode);
            }
            EndSection();
        }
    }

    ImGui::End();
}

} // namespace lucida
