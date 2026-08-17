// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

void EditorUI::TrackEdit(Registry& registry, Entity entity, const LocalTransform& current,
                        const char* name) {
    if (ImGui::IsItemActivated()) {
        m_drag_start = current;
        m_dragging = true;
    }
    if (m_dragging && ImGui::IsItemDeactivatedAfterEdit()) {
        m_commands.Push(std::make_unique<TransformCommand>(registry, entity, m_drag_start,
                                                           current, name));
        m_dragging = false;
    }
}

void EditorUI::TrackShapeEdit(Registry& registry, Entity entity, const PrimitiveShape& current,
                             const char* name) {
    if (ImGui::IsItemActivated()) {
        m_shape_drag_start = current;
        m_shape_dragging = true;
    }
    if (m_shape_dragging && ImGui::IsItemDeactivatedAfterEdit()) {
        m_commands.Push(std::make_unique<ShapeEditCommand>(registry, entity, m_shape_drag_start,
                                                           current, name));
        m_shape_dragging = false;
    }
}

void EditorUI::TrackMaterialEdit(SceneAssets& assets, i32 mat_index, const GPUMaterial& current,
                                const char* name) {
    if (ImGui::IsItemActivated()) {
        m_mat_drag_start = current;
        m_mat_dragging = true;
    }
    if (m_mat_dragging && ImGui::IsItemDeactivatedAfterEdit()) {
        m_commands.Push(std::make_unique<MaterialEditCommand>(assets, mat_index, m_mat_drag_start,
                                                              current, name));
        m_mat_dragging = false;
    }
}

void EditorUI::DrawInspector(World& world, UiState& ui, SceneAssets& assets, CameraController& camera,
                            IRenderBackend* renderer) {
    if (!ImGui::Begin("Inspector", &ui.show_inspector)) {
        ImGui::End();
        return;
    }

    Registry& entities = world.Entities();
    if (!entities.Valid(ui.selection)) {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::End();
        return;
    }

    if (Name* name = entities.Get<Name>(ui.selection)) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%s", name->value.c_str());
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) name->value = buffer;
    }

    if (Visibility* visibility = entities.Get<Visibility>(ui.selection)) {
        ImGui::Checkbox("Visible", &visibility->visible);
    }

    if (LocalTransform* local = entities.Get<LocalTransform>(ui.selection)) {
        if (BeginSection("Transform", true)) {
            // Unit mode toggle
            ImGui::TextDisabled("Units:");
            ImGui::SameLine();
            if (ImGui::RadioButton("m",  &g_units_mode, 0)) {}
            ImGui::SameLine();
            if (ImGui::RadioButton("cm", &g_units_mode, 1)) {}
            ImGui::Spacing();

            Vec3RowUnits("Position", local->position);
            TrackEdit(entities, ui.selection, *local, "Move");

            Vec3 euler = glm::degrees(glm::eulerAngles(local->rotation));
            if (Vec3Row("Rotation", euler, 0.5f)) {
                local->rotation = Quat(glm::radians(euler));
            }
            TrackEdit(entities, ui.selection, *local, "Rotate");

            {
                static bool s_uniform_scale = false;
                ImGui::TextUnformatted("Scale");
                ImGui::SameLine(90.0f);
                ImGui::SetNextItemWidth(-50.0f);
                if (s_uniform_scale) {
                    float u_scale = local->scale.x;
                    if (ImGui::DragFloat("##scale_u", &u_scale, 0.01f, 0.001f, 1000.0f, "%.3f")) {
                        local->scale = Vec3(u_scale);
                    }
                } else {
                    ImGui::DragFloat3("##scale_xyz", &local->scale.x, 0.01f, 0.001f, 1000.0f, "%.3f");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(s_uniform_scale ? "XYZ" : "Lock")) {
                    s_uniform_scale = !s_uniform_scale;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(s_uniform_scale ? "Switch to non-uniform 3-axis (X, Y, Z) scale" : "Lock to uniform scale across all 3 axes");
                }
            }
            TrackEdit(entities, ui.selection, *local, "Scale");
            EndSection();
        }
    }

    if (PrimitiveShape* shape = entities.Get<PrimitiveShape>(ui.selection)) {
        if (BeginSection("Shape", true)) {
            const char* type_names[] = { "Sphere", "Box", "Plane", "Cylinder", "Cone", "Torus", "Disk" };
            int type_idx = (int)shape->type;
            if (ImGui::Combo("Type", &type_idx, type_names, 7)) {
                shape->type = (PrimitiveType)type_idx;
            }

            if (shape->type == PrimitiveType::Sphere) {
                DragFloatUnits("Radius", shape->size.x);
            } else if (shape->type == PrimitiveType::Box) {
                Vec3RowUnits("Half Extents", shape->size);
            } else if (shape->type == PrimitiveType::Plane) {
                Vec3Row("Normal", shape->normal);
                DragFloatUnits("Offset", shape->offset, -1000.0f, 1000.0f);
            } else if (shape->type == PrimitiveType::Cylinder || shape->type == PrimitiveType::Cone) {
                DragFloatUnits("Radius", shape->size.x);
                DragFloatUnits("Height", shape->cylinder_height);
            } else if (shape->type == PrimitiveType::Torus) {
                DragFloatUnits("Radius", shape->size.x);
                DragFloatUnits("Inner Radius", shape->inner_radius);
            } else if (shape->type == PrimitiveType::Disk) {
                DragFloatUnits("Radius", shape->size.x);
                Vec3Row("Normal", shape->normal);
            }
            TrackShapeEdit(entities, ui.selection, *shape, "Shape");
            EndSection();
        }
    }

    if (MaterialRef* mat_ref = entities.Get<MaterialRef>(ui.selection)) {
        if (BeginSection("Material", true)) {
            if (!assets.materials.empty()) {
                if (mat_ref->index < 0 || mat_ref->index >= (i32)assets.materials.size()) {
                    mat_ref->index = 0;
                }

                const std::string& current_name = assets.material_names[mat_ref->index];
                if (ImGui::BeginCombo("Slot", current_name.c_str())) {
                    for (i32 i = 0; i < (i32)assets.materials.size(); ++i) {
                        const bool is_selected = (mat_ref->index == i);
                        if (ImGui::Selectable(assets.material_names[i].c_str(), is_selected)) {
                            mat_ref->index = i;
                        }
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Make Unique")) {
                    GPUMaterial cloned = assets.materials[mat_ref->index];
                    std::string new_name = assets.material_names[mat_ref->index] + "_unique";
                    assets.materials.push_back(cloned);
                    assets.material_names.push_back(new_name);
                    mat_ref->index = (i32)assets.materials.size() - 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("+ New Material")) {
                    i32 new_idx = assets.AddMaterial(
                        Material(DIFFUSE, {0.75f, 0.75f, 0.78f}, {0, 0, 0}, 0.5f, 0.0f),
                        PROC_NONE, "mat_" + std::to_string(assets.materials.size()));
                    mat_ref->index = new_idx;
                }

                GPUMaterial& m = assets.materials[mat_ref->index];

                ImGui::TextDisabled("Presets:");
                ImGui::SameLine();
                auto apply_preset = [&](const GPUMaterial& new_mat, const char* preset_name) {
                    GPUMaterial before = m;
                    m = new_mat;
                    m_commands.Push(std::make_unique<MaterialEditCommand>(assets, mat_ref->index, before, m, preset_name));
                };

                if (ImGui::SmallButton("Gold")) {
                    GPUMaterial preset = m; preset.type = 1; preset.albedo[0]=1.0f; preset.albedo[1]=0.76f; preset.albedo[2]=0.33f;
                    preset.roughness=0.15f; preset.metallic=1.0f;
                    apply_preset(preset, "Material: Gold");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Chrome")) {
                    GPUMaterial preset = m; preset.type = 1; preset.albedo[0]=0.95f; preset.albedo[1]=0.95f; preset.albedo[2]=0.95f;
                    preset.roughness=0.05f; preset.metallic=1.0f;
                    apply_preset(preset, "Material: Chrome");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Copper")) {
                    GPUMaterial preset = m; preset.type = 1; preset.albedo[0]=0.95f; preset.albedo[1]=0.64f; preset.albedo[2]=0.54f;
                    preset.roughness=0.20f; preset.metallic=1.0f;
                    apply_preset(preset, "Material: Copper");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Glass")) {
                    GPUMaterial preset = m; preset.type = 2; preset.albedo[0]=1.0f; preset.albedo[1]=1.0f; preset.albedo[2]=1.0f;
                    preset.roughness=0.0f; preset.metallic=0.0f; preset.refractive_index=1.52f;
                    apply_preset(preset, "Material: Glass");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Neon")) {
                    GPUMaterial preset = m; preset.type = 3; preset.albedo[0]=0.1f; preset.albedo[1]=0.8f; preset.albedo[2]=1.0f;
                    preset.emission[0]=1.5f; preset.emission[1]=12.0f; preset.emission[2]=15.0f;
                    apply_preset(preset, "Material: Neon");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Rubber")) {
                    GPUMaterial preset = m; preset.type = 0; preset.albedo[0]=0.12f; preset.albedo[1]=0.12f; preset.albedo[2]=0.13f;
                    preset.roughness=0.90f; preset.metallic=0.0f;
                    apply_preset(preset, "Material: Rubber");
                }

                ImGui::Separator();

                char name_buf[64];
                std::strncpy(name_buf, assets.material_names[mat_ref->index].c_str(), sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = '\0';
                if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                    assets.material_names[mat_ref->index] = name_buf;
                }

                const char* mat_types[] = { "Diffuse", "Metal", "Glass", "Emissive", "Checkerboard", "Water", "PBR" };
                if (ImGui::Combo("Type", &m.type, mat_types, 7)) {
                    TrackMaterialEdit(assets, mat_ref->index, m, "Material Type");
                }

                if (ImGui::ColorEdit3("Albedo", m.albedo)) {
                    TrackMaterialEdit(assets, mat_ref->index, m, "Material Albedo");
                }
                if (m.type == 4) { // Checkerboard
                    if (ImGui::ColorEdit3("Albedo 2", m.albedo2))
                        TrackMaterialEdit(assets, mat_ref->index, m, "Material Albedo 2");
                }
                if (m.type == 3) { // Emissive
                    if (ImGui::ColorEdit3("Emission", m.emission))
                        TrackMaterialEdit(assets, mat_ref->index, m, "Material Emission");
                }

                if (ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f))
                    TrackMaterialEdit(assets, mat_ref->index, m, "Material Roughness");
                if (ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f))
                    TrackMaterialEdit(assets, mat_ref->index, m, "Material Metallic");
                if (m.type == 2 || m.type == 5) { // Glass or Water
                    if (ImGui::SliderFloat("IOR", &m.refractive_index, 1.0f, 3.0f))
                        TrackMaterialEdit(assets, mat_ref->index, m, "Material IOR");
                }

                const char* proc_types[] = {
                    "None", "Marble", "Wood", "Rust", "Tiles", "Brushed", "Hex",
                    "Rough Ramp", "Patina", "Concrete", "Perlin Noise", "Voronoi Cells"
                };
                if (ImGui::Combo("Pattern", &m.proc_id, proc_types, 12))
                    TrackMaterialEdit(assets, mat_ref->index, m, "Material Pattern");
            }
            EndSection();
        }
    }

    if (const WorldTransform* world_transform = entities.Get<WorldTransform>(ui.selection)) {
        if (BeginSection("World", true)) {
            const Vec3 p(world_transform->matrix[3]);
            if (g_units_mode == 1) {
                LabelledText("Position", "%.1f  %.1f  %.1f cm",
                             p.x * 100.0f, p.y * 100.0f, p.z * 100.0f);
            } else {
                LabelledText("Position", "%.3f  %.3f  %.3f m", p.x, p.y, p.z);
            }
            ImGui::TextDisabled("Derived from the parent chain each frame.");
            EndSection();
        }
    }

    const MeshInstance* mesh = entities.Get<MeshInstance>(ui.selection);
    if (!mesh) {
        Entity cur = ui.selection;
        while (const Parent* p = entities.Get<Parent>(cur)) {
            if (p->entity == kNullEntity) break;
            cur = p->entity;
            if (const MeshInstance* parent_mesh = entities.Get<MeshInstance>(cur)) {
                mesh = parent_mesh;
                break;
            }
        }
    }

    if (mesh) {
        if (BeginSection("Mesh & PBR Materials", true)) {
            LabelledText("Mesh Handle", "%u", mesh->mesh.index);
            LabelledText("Instance", "%u", mesh->instance.index);

            if (renderer && mesh->mesh.IsValid()) {
                std::vector<GPUMaterial> mats = renderer->GetMeshMaterials(mesh->mesh);
                if (!mats.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Materials (%zu slots):", mats.size());

                    static int s_selected_mat = 0;
                    if (s_selected_mat >= (int)mats.size()) s_selected_mat = 0;

                    char preview_buf[64];
                    std::snprintf(preview_buf, sizeof(preview_buf), "Material Slot %d", s_selected_mat);
                    if (ImGui::BeginCombo("Slot", preview_buf)) {
                        for (int mi = 0; mi < (int)mats.size(); ++mi) {
                            const bool is_sel = (s_selected_mat == mi);
                            char label_buf[64];
                            std::snprintf(label_buf, sizeof(label_buf), "Slot %d (Type %d)", mi, mats[mi].type);
                            if (ImGui::Selectable(label_buf, is_sel)) s_selected_mat = mi;
                            if (is_sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    GPUMaterial& m = mats[s_selected_mat];
                    bool mat_changed = false;

                    ImGui::TextDisabled("Quick Presets:");
                    if (ImGui::SmallButton("Yellow 302")) {
                        m.type = 6; m.albedo[0]=0.96f; m.albedo[1]=0.72f; m.albedo[2]=0.08f;
                        m.roughness=0.15f; m.metallic=0.0f; mat_changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Red Flame")) {
                        m.type = 6; m.albedo[0]=0.85f; m.albedo[1]=0.08f; m.albedo[2]=0.08f;
                        m.roughness=0.12f; m.metallic=0.1f; mat_changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Blue Metallic")) {
                        m.type = 6; m.albedo[0]=0.08f; m.albedo[1]=0.28f; m.albedo[2]=0.85f;
                        m.roughness=0.18f; m.metallic=0.85f; mat_changed = true;
                    }
                    if (ImGui::SmallButton("Satin Black")) {
                        m.type = 6; m.albedo[0]=0.03f; m.albedo[1]=0.03f; m.albedo[2]=0.03f;
                        m.roughness=0.35f; m.metallic=0.1f; mat_changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Chrome Wheel")) {
                        m.type = 1; m.albedo[0]=0.95f; m.albedo[1]=0.95f; m.albedo[2]=0.95f;
                        m.roughness=0.02f; m.metallic=1.0f; mat_changed = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Tinted Glass")) {
                        m.type = 2; m.albedo[0]=0.8f; m.albedo[1]=0.85f; m.albedo[2]=0.9f;
                        m.roughness=0.0f; m.metallic=0.0f; m.refractive_index=1.52f; mat_changed = true;
                    }

                    ImGui::Separator();

                    const char* type_names[] = { "Diffuse", "Metal (Mirror)", "Glass (Refractive)", "Emissive", "Checkerboard", "Water", "PBR Standard" };
                    int type_idx = m.type;
                    if (type_idx < 0 || type_idx > 6) type_idx = 6;
                    if (ImGui::Combo("Shading Type", &type_idx, type_names, 7)) {
                        m.type = type_idx;
                        mat_changed = true;
                    }

                    if (m.type == 2) {
                        ImGui::TextDisabled("Glass Settings:");
                        if (ImGui::ColorEdit3("Glass Tint / Color", m.albedo)) mat_changed = true;
                        if (ImGui::SliderFloat("IOR (Refraction)", &m.refractive_index, 1.0f, 2.8f, "%.3f")) mat_changed = true;
                        if (ImGui::SliderFloat("Surface Roughness", &m.roughness, 0.0f, 1.0f, "%.3f")) mat_changed = true;

                        ImGui::TextDisabled("Glass Presets:");
                        if (ImGui::SmallButton("Clear (1.52)")) {
                            m.albedo[0]=0.98f; m.albedo[1]=0.99f; m.albedo[2]=1.0f;
                            m.refractive_index=1.52f; m.roughness=0.0f; mat_changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Auto Smoke")) {
                            m.albedo[0]=0.55f; m.albedo[1]=0.60f; m.albedo[2]=0.65f;
                            m.refractive_index=1.52f; m.roughness=0.0f; mat_changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Ruby Taillight")) {
                            m.albedo[0]=0.88f; m.albedo[1]=0.04f; m.albedo[2]=0.04f;
                            m.refractive_index=1.55f; m.roughness=0.0f; mat_changed = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Amber")) {
                            m.albedo[0]=0.95f; m.albedo[1]=0.48f; m.albedo[2]=0.02f;
                            m.refractive_index=1.52f; m.roughness=0.0f; mat_changed = true;
                        }
                    } else {
                        if (ImGui::ColorEdit3("Albedo / Paint Color", m.albedo)) mat_changed = true;
                        if (ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f, "%.3f")) mat_changed = true;
                        if (ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f, "%.3f")) mat_changed = true;
                    }

                    if (m.type == 3 || (m.emission[0] + m.emission[1] + m.emission[2] > 0.001f)) {
                        if (ImGui::ColorEdit3("Emission Color", m.emission)) mat_changed = true;
                    }

                    if (mat_changed) {
                        renderer->SetMeshMaterial(mesh->mesh, s_selected_mat, m);
                    }
                }
            }
            EndSection();
        }
    }

    if (RigidBody* rb = entities.Get<RigidBody>(ui.selection)) {
        if (BeginSection("Physics / RigidBody", true)) {
            const char* body_types[] = { "Static", "Dynamic", "Kinematic" };
            int type_idx = static_cast<int>(rb->type);
            if (ImGui::Combo("Body Type", &type_idx, body_types, 3)) {
                rb->type = static_cast<BodyType>(type_idx);
                rb->body = BodyHandle{};
            }
            DrawTooltip("Body Type:\n• Static: Immovable collider (ground, walls, obstacles)\n• Dynamic: Falls under gravity and reacts to collisions\n• Kinematic: Moved by script/transform without physical forces.");

            const char* shape_types[] = { "Box", "Sphere", "Capsule", "Plane", "Mesh" };
            int shape_idx = static_cast<int>(rb->shape);
            if (ImGui::Combo("Collider Shape", &shape_idx, shape_types, 5)) {
                rb->shape = static_cast<ShapeType>(shape_idx);
                rb->body = BodyHandle{};
            }
            DrawTooltip("Collider Shape: Geometric collision primitive used in Jolt physics.");

            if (rb->type == BodyType::Dynamic) {
                ImGui::DragFloat("Mass (kg)", &rb->mass, 0.1f, 0.01f, 10000.0f, "%.2f");
                DrawTooltip("Mass in kilograms. Determines inertia and momentum in collisions.");

                ImGui::DragFloat("Gravity Scale", &rb->gravity_scale, 0.05f, 0.0f, 10.0f, "%.2f");
                DrawTooltip("Gravity multiplier. 1.0 = standard gravity (-9.81 m/s²).");

                ImGui::SliderFloat("Linear Damping", &rb->linear_damping, 0.0f, 1.0f);
                DrawTooltip("Air drag slowing linear velocity over time.");

                ImGui::SliderFloat("Angular Damping", &rb->angular_damping, 0.0f, 1.0f);
                DrawTooltip("Rotational drag slowing angular spin over time.");
            }

            ImGui::SliderFloat("Friction", &rb->friction, 0.0f, 1.0f);
            DrawTooltip("Coulomb surface friction coefficient (0.0 = ice, 1.0 = rubber).");

            ImGui::SliderFloat("Restitution", &rb->restitution, 0.0f, 1.0f);
            DrawTooltip("Restitution (Bounciness): 0.0 = completely inelastic, 1.0 = perfectly elastic bounce.");

            ImGui::Checkbox("Simulate", &rb->is_active);
            DrawTooltip("Enable or disable physics simulation for this entity.");

            ImGui::Checkbox("Is Trigger (Sensor)", &rb->is_trigger);
            DrawTooltip("Trigger Volume: Detects overlaps and fires OnTriggerEnter/Exit events without physical collision pushback.");

            ImGui::Separator();
            if (ImGui::Button("Remove Physics", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<RigidBody>(ui.selection);
            }
            EndSection();
        }
    } else {
        if (BeginSection("Physics", false)) {
            if (ImGui::Button("+ Add RigidBody Component", ImVec2(-1.0f, 24.0f))) {
                RigidBody rb{};
                if (PrimitiveShape* ps = entities.Get<PrimitiveShape>(ui.selection)) {
                    if (ps->type == PrimitiveType::Sphere) rb.shape = ShapeType::Sphere;
                    else if (ps->type == PrimitiveType::Plane) { rb.shape = ShapeType::Plane; rb.type = BodyType::Static; }
                    else rb.shape = ShapeType::Box;
                }
                entities.Add<RigidBody>(ui.selection, rb);
            }
            DrawTooltip("Attach a Jolt physics RigidBody component to this entity.");
            EndSection();
        }
    }

    if (LightSource* light = entities.Get<LightSource>(ui.selection)) {
        if (BeginSection("Light Source", true)) {
            const char* light_types[] = { "Point Light", "Directional Light (Sun)", "Spot Light", "Area Light" };
            int type_idx = static_cast<int>(light->type);
            if (ImGui::Combo("Light Type", &type_idx, light_types, 4)) {
                light->type = static_cast<LightType>(type_idx);
            }
            DrawTooltip("Light Type:\n• Point: Radiates omnidirectionally in all directions\n• Directional: Parallel sunlight with soft shadows\n• Spot: Focused conical beam\n• Area: Rectangular diffuse light.");

            ImGui::ColorEdit3("Color", glm::value_ptr(light->color));
            DrawTooltip("Light emission RGB color.");

            ImGui::TextDisabled("Color Temp:");
            ImGui::SameLine();
            if (ImGui::SmallButton("Candle 2000K"))   light->color = Vec3(1.00f, 0.58f, 0.16f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Warm 3200K"))     light->color = Vec3(1.00f, 0.82f, 0.62f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Daylight 6500K")) light->color = Vec3(1.00f, 1.00f, 1.00f);
            ImGui::SameLine();
            if (ImGui::SmallButton("Sky 9000K"))      light->color = Vec3(0.80f, 0.88f, 1.00f);

            ImGui::DragFloat("Intensity", &light->intensity, 0.5f, 0.0f, 10000.0f, "%.1f");
            DrawTooltip("Luminous flux / emission intensity.");

            ImGui::DragFloat("Radius / Range", &light->radius, 0.05f, 0.01f, 100.0f, "%.2f");
            DrawTooltip("Maximum distance of illumination reach.");

            if (light->type == LightType::Directional || light->type == LightType::Spot) {
                ImGui::Separator();
                Vec3Row("Direction", light->direction);
                if (glm::length(light->direction) > 0.001f) {
                    light->direction = glm::normalize(light->direction);
                }
                DrawTooltip("Orientation direction vector of the light beam.");
            }

            if (light->type == LightType::Spot) {
                ImGui::SliderFloat("Inner Cone", &light->inner_angle, 1.0f, 89.0f, "%.1f deg");
                DrawTooltip("Full intensity inner beam cone angle in degrees.");
                ImGui::SliderFloat("Outer Cone", &light->outer_angle, light->inner_angle, 90.0f, "%.1f deg");
                DrawTooltip("Falloff outer boundary cone angle in degrees.");
            }

            ImGui::Checkbox("Cast Shadows", &light->cast_shadows);
            DrawTooltip("Enable ray-traced soft shadow casting from this light.");
            EndSection();
        }
    }

    if (TerrainComponent* terrain = entities.Get<TerrainComponent>(ui.selection)) {
        if (BeginSection("Terrain Generator", true)) {
            bool changed = false;
            changed |= ImGui::SliderInt("Resolution", &terrain->resolution, 8, 256);
            changed |= ImGui::DragFloat("World Size", &terrain->size, 0.5f, 5.0f, 500.0f, "%.1f m");
            changed |= ImGui::DragFloat("Max Height", &terrain->max_height, 0.1f, 0.5f, 100.0f, "%.1f m");
            changed |= ImGui::SliderFloat("Frequency", &terrain->frequency, 0.001f, 0.2f, "%.4f");
            changed |= ImGui::SliderInt("Octaves", &terrain->octaves, 1, 8);
            changed |= ImGui::SliderFloat("Persistence", &terrain->persistence, 0.1f, 0.9f, "%.2f");
            changed |= ImGui::SliderFloat("Lacunarity", &terrain->lacunarity, 1.0f, 4.0f, "%.2f");
            
            ImGui::InputScalar("Seed", ImGuiDataType_U32, &terrain->seed);
            ImGui::SameLine();
            if (ImGui::Button("Random")) {
                terrain->seed = static_cast<u32>(rand());
                changed = true;
            }

            if (changed) terrain->dirty = true;

            ImGui::Separator();
            if (ImGui::Button("Regenerate Terrain", ImVec2(-1.0f, 26.0f))) {
                terrain->dirty = true;
            }
            EndSection();
        }
    }

    if (CameraComponent* cam = entities.Get<CameraComponent>(ui.selection)) {
        if (BeginSection("Camera Component", true)) {
            const char* proj_types[] = { "Perspective", "Orthographic" };
            int cur_proj = static_cast<int>(cam->projection);
            if (ImGui::Combo("Projection", &cur_proj, proj_types, 2)) {
                cam->projection = static_cast<ProjectionType>(cur_proj);
            }

            if (cam->projection == ProjectionType::Perspective) {
                ImGui::SliderFloat("Field of View", &cam->fov, 10.0f, 130.0f, "%.1f deg");
            } else {
                ImGui::DragFloat("Ortho Size", &cam->ortho_size, 0.1f, 0.1f, 100.0f, "%.1f m");
            }

            ImGui::DragFloat("Near Clip", &cam->near_clip, 0.01f, 0.001f, 10.0f, "%.3f m");
            ImGui::DragFloat("Far Clip", &cam->far_clip, 1.0f, 10.0f, 10000.0f, "%.0f m");
            ImGui::Checkbox("Main Scene Camera", &cam->is_primary);
            ImGui::DragFloat("Exposure", &cam->exposure, 0.05f, 0.1f, 10.0f, "%.2f");

            ImGui::Separator();
            if (LocalTransform* lt = entities.Get<LocalTransform>(ui.selection)) {
                if (ImGui::Button("Align Camera to View", ImVec2(-1.0f, 24.0f))) {
                    lt->position = camera.Camera().position;
                    const Vec3 fwd = camera.Camera().Forward();
                    lt->rotation = glm::quatLookAt(fwd, Vec3(0, 1, 0));
                }
                if (ImGui::Button("Align View to Camera", ImVec2(-1.0f, 24.0f))) {
                    const_cast<CameraController&>(camera).Camera().position = lt->position;
                    const Vec3 fwd = lt->rotation * Vec3(0, 0, -1);
                    const_cast<CameraController&>(camera).LookAt(lt->position, lt->position + fwd);
                }
            }
            EndSection();
        }
    }

    if (Vehicle* vehicle = entities.Get<Vehicle>(ui.selection)) {
        if (BeginSection("Vehicle", true)) {
            ImGui::SliderFloat("Throttle", &vehicle->input.throttle, 0.0f, 1.0f);
            ImGui::SliderFloat("Brake", &vehicle->input.brake, 0.0f, 1.0f);
            ImGui::SliderFloat("Steer", &vehicle->input.steer, -1.0f, 1.0f);
            ImGui::Checkbox("Handbrake", &vehicle->input.handbrake);
            EndSection();
        }
    }

    if (TagComponent* tag = entities.Get<TagComponent>(ui.selection)) {
        if (BeginSection("Tag & Layer", true)) {
            char tag_buf[64];
            std::snprintf(tag_buf, sizeof(tag_buf), "%s", tag->tag.c_str());
            if (ImGui::InputText("Tag", tag_buf, sizeof(tag_buf))) tag->tag = tag_buf;
            int layer = static_cast<int>(tag->layer);
            if (ImGui::InputInt("Layer", &layer)) tag->layer = static_cast<u32>(std::max(0, layer));
            if (ImGui::Button("Remove Tag", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<TagComponent>(ui.selection);
            }
            EndSection();
        }
    }

    if (ParticleEmitterComponent* pe = entities.Get<ParticleEmitterComponent>(ui.selection)) {
        if (BeginSection("Particle Emitter (VFX)", true)) {
            ImGui::Checkbox("Active", &pe->is_active);
            ImGui::DragFloat("Rate (p/s)", &pe->emission_rate, 1.0f, 1.0f, 2000.0f);
            ImGui::SliderFloat("Spread Angle", &pe->spread_angle, 0.0f, 90.0f * kDegToRad, "%.1f rad");
            ImGui::DragFloatRange2("Lifetime (s)", &pe->lifetime_min, &pe->lifetime_max, 0.05f, 0.1f, 10.0f);
            ImGui::DragFloatRange2("Speed (m/s)", &pe->speed_min, &pe->speed_max, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat3("Gravity", glm::value_ptr(pe->gravity), 0.1f);
            ImGui::DragFloat2("Size Start/End", &pe->size_start, 0.01f, 0.0f, 5.0f);
            ImGui::ColorEdit4("Start Color", glm::value_ptr(pe->color_start));
            ImGui::ColorEdit4("End Color", glm::value_ptr(pe->color_end));
            ImGui::TextDisabled("Particles: %u / %u", pe->active_count, pe->max_particles);
            if (ImGui::Button("Remove Emitter", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<ParticleEmitterComponent>(ui.selection);
            }
            EndSection();
        }
    }

    if (AudioSourceComponent* audio = entities.Get<AudioSourceComponent>(ui.selection)) {
        if (BeginSection("Audio Source", true)) {
            char path_buf[256];
            std::snprintf(path_buf, sizeof(path_buf), "%s", audio->sound_path.c_str());
            if (ImGui::InputText("Sound Path", path_buf, sizeof(path_buf))) audio->sound_path = path_buf;
            ImGui::SliderFloat("Volume", &audio->volume, 0.0f, 2.0f);
            ImGui::SliderFloat("Pitch", &audio->pitch, 0.1f, 3.0f);
            ImGui::Checkbox("3D Spatial Audio", &audio->is_3d);
            ImGui::Checkbox("Loop", &audio->loop);
            ImGui::Checkbox("Play On Start", &audio->play_on_start);
            if (ImGui::Button("Remove Audio Source", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<AudioSourceComponent>(ui.selection);
            }
            EndSection();
        }
    }

    if (AudioListenerComponent* listener = entities.Get<AudioListenerComponent>(ui.selection)) {
        if (BeginSection("Audio Listener", true)) {
            ImGui::Checkbox("Active Listener", &listener->is_active);
            ImGui::SliderFloat("Master Volume", &listener->master_volume, 0.0f, 2.0f);
            if (ImGui::Button("Remove Audio Listener", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<AudioListenerComponent>(ui.selection);
            }
            EndSection();
        }
    }

    if (AnimatorComponent* anim = entities.Get<AnimatorComponent>(ui.selection)) {
        if (BeginSection("Skeletal Animator", true)) {
            ImGui::SliderFloat("Speed", &anim->playback_speed, 0.0f, 5.0f);
            ImGui::Checkbox("Looping", &anim->is_looping);
            ImGui::Checkbox("Playing", &anim->is_playing);
            if (anim->current_clip) {
                ImGui::ProgressBar(anim->current_clip->duration > 0.0f ? anim->current_time / anim->current_clip->duration : 0.0f);
                ImGui::TextDisabled("Time: %.2f / %.2f s", anim->current_time, anim->current_clip->duration);
            }
            if (ImGui::Button("Remove Animator", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<AnimatorComponent>(ui.selection);
            }
            EndSection();
        }
    }

    if (ScriptComponent* sc = entities.Get<ScriptComponent>(ui.selection)) {
        if (BeginSection("Native Scripts", true)) {
            ImGui::TextDisabled("Attached Scripts: %zu", sc->scripts.size());
            if (ImGui::Button("Remove Scripts", ImVec2(-1.0f, 22.0f))) {
                entities.Remove<ScriptComponent>(ui.selection);
            }
            EndSection();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("+ Add Component", ImVec2(-1.0f, 28.0f))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("Add Component:");
        ImGui::Separator();
        if (!entities.Get<TagComponent>(ui.selection) && ImGui::MenuItem("Tag & Layer")) {
            entities.Add<TagComponent>(ui.selection);
        }
        if (!entities.Get<RigidBody>(ui.selection) && ImGui::MenuItem("RigidBody (Physics)")) {
            entities.Add<RigidBody>(ui.selection);
        }
        if (!entities.Get<ParticleEmitterComponent>(ui.selection) && ImGui::MenuItem("Particle Emitter (VFX)")) {
            entities.Add<ParticleEmitterComponent>(ui.selection);
        }
        if (!entities.Get<AudioSourceComponent>(ui.selection) && ImGui::MenuItem("Audio Source")) {
            entities.Add<AudioSourceComponent>(ui.selection);
        }
        if (!entities.Get<AudioListenerComponent>(ui.selection) && ImGui::MenuItem("Audio Listener")) {
            entities.Add<AudioListenerComponent>(ui.selection);
        }
        if (!entities.Get<AnimatorComponent>(ui.selection) && ImGui::MenuItem("Skeletal Animator")) {
            entities.Add<AnimatorComponent>(ui.selection);
        }
        if (!entities.Get<ScriptComponent>(ui.selection) && ImGui::MenuItem("Script Component")) {
            entities.Add<ScriptComponent>(ui.selection);
        }
        if (!entities.Get<LightSource>(ui.selection) && ImGui::MenuItem("Light Source")) {
            entities.Add<LightSource>(ui.selection);
        }
        if (!entities.Get<CameraComponent>(ui.selection) && ImGui::MenuItem("Camera Component")) {
            entities.Add<CameraComponent>(ui.selection);
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace lucida
