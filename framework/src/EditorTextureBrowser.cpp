// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "EditorCommon.h"

namespace lucida {

void EditorUI::DrawTextureBrowser(UiState& ui, SceneAssets& assets) {
    if (!ImGui::Begin("Texture Maps", &ui.show_texture_browser)) {
        ImGui::End();
        return;
    }

    // Top action bar
    if (ImGui::Button("+ Import Texture...")) {
        IGFD::FileDialogConfig config;
        config.path = "assets";
        ImGuiFileDialog::Instance()->OpenDialog("ImportTexture", "Select Image Texture",
                                                ".png,.jpg,.jpeg,.hdr,.tga,.bmp", config);
    }
    DrawTooltip("Import an image texture (PNG, JPG, HDR, TGA, BMP) into GPU memory.");
    ImGui::SameLine();

    if (ImGui::Button("Route Packed ORM...")) {
        IGFD::FileDialogConfig config;
        config.path = "assets";
        ImGuiFileDialog::Instance()->OpenDialog("RouteORM", "Select Packed ORM Texture (R=AO, G=Rough, B=Metal)",
                                                ".png,.jpg,.jpeg,.hdr,.tga", config);
    }
    DrawTooltip("Auto-assign packed texture channels:\nRed -> AO\nGreen -> Roughness\nBlue -> Metallic");

    ImGui::Separator();

    // Helper to render a clean, standard map slot
    auto draw_slot = [&](const char* label, const char* tag, UiState::TextureMapSlot& slot, auto custom_controls) {
        if (!BeginSection(label, true)) {
            EndSection();
            return;
        }

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 76.0f);

        // 1. Texture slot box (supports drag-and-drop)
        ImVec2 slot_size(68.0f, 68.0f);
        std::string btn_label = slot.path.empty() ? std::string(tag) : std::string("[") + tag + "]";
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(28, 28, 32, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(45, 45, 52, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, slot.path.empty() ? IM_COL32(50, 50, 56, 255) : IM_COL32(90, 120, 160, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        ImGui::Button(btn_label.c_str(), slot_size);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                const char* dropped = static_cast<const char*>(payload->Data);
                if (dropped) {
                    slot.path = dropped;
                    TextureManager::Instance().LoadTexture(dropped);
                    LUCIDA_INFO(Resource, "Assigned '%s' to %s map", dropped, tag);
                }
            }
            ImGui::EndDragDropTarget();
        }
        DrawTooltip("Drag and drop an image texture here from the Content Browser.");

        ImGui::NextColumn();

        // 2. Path display and controls
        std::string filename = slot.path.empty() ? "None (Default)" : std::filesystem::path(slot.path).filename().string();
        ImGui::TextUnformatted(filename.c_str());

        std::string browse_id = std::string("Browse##") + tag;
        std::string clear_id  = std::string("Clear##") + tag;
        std::string dlg_id    = std::string("Dlg_") + tag;

        if (ImGui::SmallButton(browse_id.c_str())) {
            IGFD::FileDialogConfig config;
            config.path = "assets";
            ImGuiFileDialog::Instance()->OpenDialog(dlg_id, "Select Texture",
                                                    ".png,.jpg,.jpeg,.hdr,.tga,.bmp", config);
        }
        ImGui::SameLine();
        if (!slot.path.empty() && ImGui::SmallButton(clear_id.c_str())) {
            slot.path.clear();
        }

        // Custom map parameters
        custom_controls();

        ImGui::Columns(1);
        EndSection();

        if (ImGuiFileDialog::Instance()->Display(dlg_id.c_str(), ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                slot.path = ImGuiFileDialog::Instance()->GetFilePathName();
                TextureManager::Instance().LoadTexture(slot.path);
            }
            ImGuiFileDialog::Instance()->Close();
        }
    };

    // 1. Albedo
    draw_slot("Albedo / Base Color", "Albedo", ui.map_albedo, [&]() {
        ImGui::ColorEdit4("Tint", glm::value_ptr(ui.map_albedo.tint), ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::Checkbox("sRGB", &ui.map_albedo.srgb);
    });

    // 2. Normal
    draw_slot("Normal Map", "Normal", ui.map_normal, [&]() {
        ImGui::SliderFloat("Strength", &ui.map_normal.normal_strength, 0.0f, 3.0f, "%.2fx");
        ImGui::Checkbox("Flip Green (DirectX -Y)", &ui.map_normal.flip_green_normal);
    });

    // 3. Metallic
    draw_slot("Metallic Map", "Metallic", ui.map_metallic, [&]() {
        ImGui::SliderFloat("Factor##Metal", &ui.map_metallic.factor, 0.0f, 1.0f, "%.2f");
        const char* chs[] = { "RGB", "Red (R)", "Green (G)", "Blue (B)", "Alpha (A)" };
        ImGui::Combo("Channel##Metal", &ui.map_metallic.channel, chs, 5);
    });

    // 4. Roughness
    draw_slot("Roughness Map", "Roughness", ui.map_roughness, [&]() {
        ImGui::SliderFloat("Factor##Rough", &ui.map_roughness.factor, 0.0f, 1.0f, "%.2f");
        const char* chs[] = { "RGB", "Red (R)", "Green (G)", "Blue (B)", "Alpha (A)" };
        ImGui::Combo("Channel##Rough", &ui.map_roughness.channel, chs, 5);
        ImGui::Checkbox("Invert Roughness", &ui.map_roughness.invert);
    });

    // 5. Ambient Occlusion
    draw_slot("Ambient Occlusion (AO)", "AO", ui.map_ao, [&]() {
        ImGui::SliderFloat("Intensity##AO", &ui.map_ao.factor, 0.0f, 2.0f, "%.2fx");
        const char* chs[] = { "RGB", "Red (R)", "Green (G)", "Blue (B)", "Alpha (A)" };
        ImGui::Combo("Channel##AO", &ui.map_ao.channel, chs, 5);
    });

    // 6. Emissive
    draw_slot("Emissive Map", "Emissive", ui.map_emissive, [&]() {
        ImGui::ColorEdit3("Color##Emis", glm::value_ptr(ui.map_emissive.tint), ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::DragFloat("Luminance", &ui.map_emissive.emissive_intensity, 0.1f, 0.0f, 100.0f, "%.1fx");
    });

    // 7. Height / Displacement
    draw_slot("Height / Displacement (POM)", "Height", ui.map_height, [&]() {
        ImGui::SliderFloat("Depth Scale", &ui.map_height.height_scale, 0.0f, 0.2f, "%.3f m");
        ImGui::SliderInt("Steps", &ui.map_height.pom_steps, 4, 64);
    });

    // Handle Route ORM dialog
    if (ImGuiFileDialog::Instance()->Display("RouteORM", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string p = ImGuiFileDialog::Instance()->GetFilePathName();
            TextureManager::Instance().LoadTexture(p);
            ui.map_ao.path        = p; ui.map_ao.channel        = 1; // R
            ui.map_roughness.path = p; ui.map_roughness.channel = 2; // G
            ui.map_metallic.path  = p; ui.map_metallic.channel  = 3; // B
            LUCIDA_INFO(Resource, "Auto-routed packed ORM texture '%s' to AO(R), Roughness(G), Metallic(B)", p.c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle Import Texture dialog
    if (ImGuiFileDialog::Instance()->Display("ImportTexture", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string p = ImGuiFileDialog::Instance()->GetFilePathName();
            TextureManager::Instance().LoadTexture(p);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // GPU Texture Cache table
    if (BeginSection("GPU Texture Cache", false)) {
        auto textures = TextureManager::Instance().GetAllTextures();
        size_t total_vram = TextureManager::Instance().GetTotalMemoryBytes();

        ImGui::Text("Textures: %zu | VRAM: %.2f MB", textures.size(), total_vram / (1024.0f * 1024.0f));
        ImGui::Separator();

        if (textures.empty()) {
            ImGui::TextDisabled("No textures loaded in GPU memory.");
        } else {
            if (ImGui::BeginTable("TexVramTable", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 140))) {
                ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Memory", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();

                for (const auto* tex : textures) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(tex->path.c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text("%dx%d", tex->width, tex->height);

                    ImGui::TableNextColumn();
                    ImGui::Text("%d ch", tex->channels);

                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f KB", tex->memory_bytes / 1024.0f);
                }
                ImGui::EndTable();
            }
        }
        EndSection();
    }

    ImGui::End();
}

} // namespace lucida
