// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/resource/TextureManager.h"
#include "lucida/core/diag/Log.h"

#include <stb_image.h>
#include <algorithm>

namespace lucida {

TextureManager& TextureManager::Instance() {
    static TextureManager s_instance;
    return s_instance;
}

TextureHandle TextureManager::LoadTexture(const std::string& filepath, bool flip_y) {
    if (filepath.empty()) return TextureHandle{};

    auto it = m_path_to_handle.find(filepath);
    if (it != m_path_to_handle.end()) {
        return it->second;
    }

    stbi_set_flip_vertically_on_load(flip_y ? 1 : 0);

    int width = 0, height = 0, channels = 0;
    stbi_uc* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
    if (!data) {
        LUCIDA_WARN(Resource, "Failed to load texture file: %s (reason: %s)", filepath.c_str(), stbi_failure_reason());
        return TextureHandle{};
    }

    TextureHandle handle{++m_next_id, 1};
    TextureInfo info;
    info.handle = handle;
    info.path = filepath;
    info.width = width;
    info.height = height;
    info.channels = 4;
    info.format = TextureFormat::RGBA8_UNORM;
    info.memory_bytes = static_cast<usize>(width * height * 4);
    info.pixels.assign(data, data + info.memory_bytes);

    stbi_image_free(data);

    m_textures[handle.index] = std::move(info);
    m_path_to_handle[filepath] = handle;

    LUCIDA_INFO(Resource, "Loaded texture [%u]: %s (%dx%d, 4ch, %.1f KB)",
                handle.index, filepath.c_str(), width, height, info.memory_bytes / 1024.0f);

    return handle;
}

TextureHandle TextureManager::RegisterTexture(const std::string& name, i32 width, i32 height, i32 channels,
                                              TextureFormat format, const std::vector<u8>& data) {
    TextureHandle handle{++m_next_id, 1};
    TextureInfo info;
    info.handle = handle;
    info.path = name;
    info.width = width;
    info.height = height;
    info.channels = channels;
    info.format = format;
    info.memory_bytes = data.size();
    info.pixels = data;

    m_textures[handle.index] = std::move(info);
    if (!name.empty()) {
        m_path_to_handle[name] = handle;
    }

    return handle;
}

const TextureInfo* TextureManager::GetTexture(TextureHandle handle) const {
    if (!handle.IsValid()) return nullptr;
    auto it = m_textures.find(handle.index);
    if (it != m_textures.end() && it->second.handle.generation == handle.generation) {
        return &it->second;
    }
    return nullptr;
}

const TextureInfo* TextureManager::FindTexture(const std::string& filepath) const {
    auto it = m_path_to_handle.find(filepath);
    if (it != m_path_to_handle.end()) {
        return GetTexture(it->second);
    }
    return nullptr;
}

std::vector<const TextureInfo*> TextureManager::GetAllTextures() const {
    std::vector<const TextureInfo*> list;
    list.reserve(m_textures.size());
    for (const auto& [id, info] : m_textures) {
        list.push_back(&info);
    }
    return list;
}

void TextureManager::UnloadTexture(TextureHandle handle) {
    if (!handle.IsValid()) return;
    auto it = m_textures.find(handle.index);
    if (it != m_textures.end()) {
        m_path_to_handle.erase(it->second.path);
        m_textures.erase(it);
    }
}

void TextureManager::Clear() {
    m_textures.clear();
    m_path_to_handle.clear();
}

} // namespace lucida
