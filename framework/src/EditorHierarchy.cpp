// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

void EditorUI::DrawSceneGraph(World& world, UiState& ui, Entity current_parent) {
    Registry& entities = world.Entities();
    for (auto [entity, name] : entities.View<Name>().each()) {
        Entity its_parent = kNullEntity;
        if (Parent* p = entities.Get<Parent>(entity)) its_parent = p->entity;

        if (ui.hierarchy_search[0] != '\0') {
            std::string n = name.value;
            std::string s = ui.hierarchy_search;
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (n.find(s) == std::string::npos) continue;
        } else {
            if (its_parent != current_parent) continue;
        }

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
        
        bool has_children = false;
        for (auto [child, parent_comp] : entities.View<Parent>().each()) {
            if (parent_comp.entity == entity) { has_children = true; break; }
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (ui.selection == entity) flags |= ImGuiTreeNodeFlags_Selected;
        if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;
        flags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool opened = ImGui::TreeNodeEx(name.value.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            ui.selection = entity;
        }

        if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
            ui.selection = entity;
            if (ImGui::MenuItem("Unparent", nullptr, false, its_parent != kNullEntity)) {
                entities.Remove<Parent>(entity);
            }
            if (ImGui::MenuItem("Duplicate", "Cmd+D")) {
                EntitySnapshot snap = EntitySnapshot::Capture(entities, entity);
                snap.name += "_copy";
                snap.transform.position += Vec3(0.5f, 0.0f, 0.5f);
                Entity dup = snap.Restore(entities);
                ui.selection = dup;
                m_commands.Push(std::make_unique<CreateEntityCommand>(entities, dup, snap, "Duplicate Entity"));
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del")) {
                Entity to_del = entity;
                if (ui.selection == to_del) ui.selection = kNullEntity;
                m_commands.Execute(std::make_unique<DestroyEntityCommand>(entities, to_del, "Delete Entity"));
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource()) {
            Entity dragged = entity;
            ImGui::SetDragDropPayload("ENTITY_PAYLOAD", &dragged, sizeof(Entity));
            ImGui::TextUnformatted(name.value.c_str());
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

        if (opened) {
            DrawSceneGraph(world, ui, entity);
            ImGui::TreePop();
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

    // Hierarchy Search & Filter Bar
    ImGui::SetNextItemWidth(-30.0f);
    ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", ui.hierarchy_search, sizeof(ui.hierarchy_search));
    ImGui::SameLine();
    if (ImGui::SmallButton("X")) {
        ui.hierarchy_search[0] = '\0';
    }

    if (ImGui::Button("+ Create Entity", ImVec2(-1.0f, 24.0f))) {
        ImGui::OpenPopup("AddEntityPopup");
    }

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
            if (ImGui::MenuItem("Terrain Generator")) {
                ui.selection = Prefab::CreateTerrainNode(world);
            }
            if (ImGui::MenuItem("Water Body (Ocean)")) ui.selection = Prefab::CreateWaterBodyNode(world, Vec3(0,-0.5f,0), Vec2(100,100));
            if (ImGui::MenuItem("River Spline Node")) ui.selection = Prefab::CreateRiverNode(world, {Vec3(-20,0,-20), Vec3(0,0,0), Vec3(20,0,20)});
            if (ImGui::MenuItem("Foliage Instancer")) ui.selection = Prefab::CreateFoliageNode(world, "assets/models/grass.obj", 500);
            if (ImGui::MenuItem("Wind Source Node")) ui.selection = Prefab::CreateWindSourceNode(world, Vec3(1,0,0), 5.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Physics & Sensors")) {
            if (ImGui::MenuItem("Dynamic Box Actor")) ui.selection = Prefab::CreatePhysicsActorNode(world, PrimitiveType::Box, BodyType::Dynamic, Vec3(0,3,0));
            if (ImGui::MenuItem("Dynamic Sphere Actor")) ui.selection = Prefab::CreatePhysicsActorNode(world, PrimitiveType::Sphere, BodyType::Dynamic, Vec3(0,4,0));
            if (ImGui::MenuItem("Trigger Volume (Sensor)")) ui.selection = Prefab::CreateTriggerVolumeNode(world, Vec3(0,1,0), Vec3(1,1,1));
            if (ImGui::MenuItem("Raycast Sensor Node")) ui.selection = Prefab::CreateRaycastSensorNode(world, Vec3(0,1,0), Vec3(0,-1,0), 10.0f);
            if (ImGui::MenuItem("Physics Joint (Hinge)")) ui.selection = Prefab::CreatePhysicsJointNode(world, JointType::Hinge, Vec3(0,2,0));
            if (ImGui::MenuItem("Buoyancy Actor Node")) ui.selection = Prefab::CreateBuoyancyNode(world, Vec3(0,1,0), 0.0f);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Vehicles")) {
            if (ImGui::MenuItem("Muscle Car (Wheeled)"))   ui.selection = Prefab::CreateWheeledVehicleNode(world, Vec3(0,1,0), "WheeledCar");
            if (ImGui::MenuItem("Tank (Tracked)"))          ui.selection = Prefab::CreateTrackedVehicleNode(world, Vec3(0,1,0), "Tank");
            if (ImGui::MenuItem("Aircraft"))                ui.selection = Prefab::CreateAircraftNode(world, Vec3(0,10,0));
            if (ImGui::MenuItem("Watercraft / Boat"))       ui.selection = Prefab::CreateWatercraftNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Vehicle Wheel"))           ui.selection = Prefab::CreateVehicleWheelNode(world, Vec3(0.8f,0.35f,1.5f));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Characters & AI")) {
            if (ImGui::MenuItem("Player Pawn"))             ui.selection = Prefab::CreatePawnNode(world, Vec3(0, 1.8f, 5.0f));
            if (ImGui::MenuItem("Character Body (Humanoid)")) ui.selection = Prefab::CreateCharacterBodyNode(world, Vec3(0, 1.0f, 0.0f));
            if (ImGui::MenuItem("AI Enemy / Controller"))   ui.selection = Prefab::CreateAIControllerNode(world, Vec3(0, 1.0f, 0.0f));
            if (ImGui::MenuItem("Player Input Node"))       ui.selection = Prefab::CreatePlayerInputNode(world);
            if (ImGui::MenuItem("Ragdoll Node"))            ui.selection = Prefab::CreateRagdollNode(world, Vec3(0, 1.0f, 0.0f));
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Animation & Kinematics")) {
            if (ImGui::MenuItem("Skeleton Node"))           ui.selection = Prefab::CreateSkeletonNode(world);
            if (ImGui::MenuItem("Bone Node"))               ui.selection = Prefab::CreateBoneNode(world, "Spine_01");
            if (ImGui::MenuItem("Socket / Attachment"))     ui.selection = Prefab::CreateSocketNode(world, "Hand_R");
            if (ImGui::MenuItem("Animation Player"))        ui.selection = Prefab::CreateAnimationPlayerNode(world, "Idle");
            if (ImGui::MenuItem("Animation Tree Blend"))    ui.selection = Prefab::CreateAnimationTreeBlendNode(world);
            if (ImGui::MenuItem("IK Solver Node"))          ui.selection = Prefab::CreateIKSolverNode(world, "Foot_L");
            if (ImGui::MenuItem("Morph Target Node"))       ui.selection = Prefab::CreateMorphTargetNode(world);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("AI & Navigation")) {
            if (ImGui::MenuItem("NavMesh Bounds"))          ui.selection = Prefab::CreateNavMeshBoundsNode(world, Vec3(0,0,0), Vec3(50,10,50));
            if (ImGui::MenuItem("NavMesh Obstacle"))        ui.selection = Prefab::CreateNavMeshObstacleNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("NavMesh Link"))            ui.selection = Prefab::CreateNavMeshLinkNode(world, Vec3(0,0,0), Vec3(0,2,3));
            if (ImGui::MenuItem("Navigation Agent"))        ui.selection = Prefab::CreateNavigationAgentNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Behavior Tree Node"))      ui.selection = Prefab::CreateBehaviorTreeNode(world, "PatrolAndChase");
            if (ImGui::MenuItem("FSM (State Machine)"))     ui.selection = Prefab::CreateFSMNode(world, "Patrol");
            if (ImGui::MenuItem("Perception Sensor"))       ui.selection = Prefab::CreatePerceptionSensorNode(world, 20.0f);
            if (ImGui::MenuItem("AI Blackboard"))           ui.selection = Prefab::CreateBlackboardNode(world);
            if (ImGui::MenuItem("Patrol Path (Spline)"))    ui.selection = Prefab::CreatePatrolPathNode(world, {Vec3(-5,0,-5), Vec3(5,0,-5), Vec3(5,0,5), Vec3(-5,0,5)});
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("VFX & Audio")) {
            if (ImGui::MenuItem("Particle Emitter"))        ui.selection = Prefab::CreateParticleEmitterNode(world, Vec3(0,1,0));
            if (ImGui::MenuItem("VFX Graph Node"))          ui.selection = Prefab::CreateVFXGraphNode(world, "fire_sparks.vfx");
            if (ImGui::MenuItem("Trail Effect"))            ui.selection = Prefab::CreateTrailNode(world, Vec3(0,1,0));
            if (ImGui::MenuItem("Beam / Laser Emitter"))    ui.selection = Prefab::CreateBeamEmitterNode(world, Vec3(0,1,0), Vec3(0,1,10));
            ImGui::Separator();
            if (ImGui::MenuItem("Spatial Audio Source"))    ui.selection = Prefab::CreateAudioSourceNode(world, "assets/sound/sfx.wav", Vec3(0,1,0));
            if (ImGui::MenuItem("Audio Listener Node"))     ui.selection = Prefab::CreateAudioListenerNode(world, Vec3(0,1.8f,0));
            if (ImGui::MenuItem("Audio Reverb Zone"))       ui.selection = Prefab::CreateAudioReverbZoneNode(world, Vec3(0,0,0), 20.0f);
            if (ImGui::MenuItem("Music Track Node"))        ui.selection = Prefab::CreateMusicTrackNode(world, "assets/audio/combat_theme.ogg");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Gameplay Systems")) {
            if (ImGui::MenuItem("Health Node"))             ui.selection = Prefab::CreateHealthNode(world, 100.0f);
            if (ImGui::MenuItem("Damage Receiver"))         ui.selection = Prefab::CreateDamageReceiverNode(world);
            if (ImGui::MenuItem("Hitbox Node"))             ui.selection = Prefab::CreateHitboxNode(world, 25.0f);
            if (ImGui::MenuItem("Hurtbox Node"))            ui.selection = Prefab::CreateHurtboxNode(world);
            if (ImGui::MenuItem("Inventory Node"))          ui.selection = Prefab::CreateInventoryNode(world, 20);
            if (ImGui::MenuItem("Equipment Node"))          ui.selection = Prefab::CreateEquipmentNode(world);
            if (ImGui::MenuItem("Interactable Chest"))      ui.selection = Prefab::CreateInteractableNode(world, Vec3(0,0,0), "Press [E] to Open");
            if (ImGui::MenuItem("Ability Node (Fireball)")) ui.selection = Prefab::CreateAbilityNode(world, "Fireball");
            if (ImGui::MenuItem("Quest Trigger"))           ui.selection = Prefab::CreateQuestTriggerNode(world, Vec3(0,0,0), "Quest_01");
            if (ImGui::MenuItem("Save Point Node"))         ui.selection = Prefab::CreateSavePointNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Item Spawner"))            ui.selection = Prefab::CreateItemSpawnerNode(world, Vec3(0,1,0), "HealthPotion");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Networking")) {
            if (ImGui::MenuItem("Network Identity"))        ui.selection = Prefab::CreateNetworkIdentityNode(world, 1);
            if (ImGui::MenuItem("Network Transform"))       ui.selection = Prefab::CreateNetworkTransformNode(world);
            if (ImGui::MenuItem("Network Animator"))        ui.selection = Prefab::CreateNetworkAnimatorNode(world);
            if (ImGui::MenuItem("Replication Manager"))     ui.selection = Prefab::CreateReplicationManagerNode(world, 30);
            if (ImGui::MenuItem("RPC Node"))                ui.selection = Prefab::CreateRPCNode(world);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("UI & HUD")) {
            if (ImGui::MenuItem("Canvas Layer"))            ui.selection = Prefab::CreateCanvasLayerNode(world, 0);
            if (ImGui::MenuItem("UI Panel"))                ui.selection = Prefab::CreateUIPanelNode(world, Vec2(200,150));
            if (ImGui::MenuItem("UI Container"))            ui.selection = Prefab::CreateUIContainerNode(world);
            if (ImGui::MenuItem("UI Button"))               ui.selection = Prefab::CreateUIButtonNode(world, "Play");
            if (ImGui::MenuItem("UI Label"))                ui.selection = Prefab::CreateUILabelNode(world, "Score: 0");
            if (ImGui::MenuItem("UI Image"))                ui.selection = Prefab::CreateUIImageNode(world, "icon.png");
            if (ImGui::MenuItem("World Space UI (Healthbar)")) ui.selection = Prefab::CreateWorldSpaceUINode(world, Vec3(0,2,0), "Boss Health");
            if (ImGui::MenuItem("Mini Map Node"))           ui.selection = Prefab::CreateMiniMapNode(world);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene & Optimization")) {
            if (ImGui::MenuItem("LOD Group"))               ui.selection = Prefab::CreateLODGroupNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("HLOD Proxy"))              ui.selection = Prefab::CreateHLODNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Occlusion Portal"))        ui.selection = Prefab::CreateOcclusionPortalNode(world, Vec3(0,1.5f,0));
            if (ImGui::MenuItem("World Partition Cell"))    ui.selection = Prefab::CreateWorldPartitionCellNode(world, Vec3(0,0,0));
            if (ImGui::MenuItem("Debug Draw Node"))         ui.selection = Prefab::CreateDebugDrawNode(world);
            if (ImGui::MenuItem("Timer Node (1s)"))         ui.selection = Prefab::CreateTimerNode(world, 1.0f);
            if (ImGui::MenuItem("Signal Bus Node"))         ui.selection = Prefab::CreateSignalBusNode(world);
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    DrawSceneGraph(world, ui, kNullEntity);

    // Drop on empty space to unparent
    ImGui::Dummy(ImGui::GetContentRegionAvail());
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
            Entity dropped = *static_cast<const Entity*>(payload->Data);
            entities.Remove<Parent>(dropped);
        }
        ImGui::EndDragDropTarget();
    }

    if (entities.Count() == 0) {
        ImGui::TextDisabled("Nothing here yet.");
        ImGui::TextDisabled("Load a model, or open a project.");
    }

    ImGui::End();
}

} // namespace lucida
