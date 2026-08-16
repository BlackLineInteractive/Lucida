// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "lucida/core/container/Handle.h"
#include "lucida/core/platform/Platform.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lucida {

LUCIDA_DECLARE_HANDLE(TextureHandle);

enum class TextureFormat : u8 {
    RGBA8_UNORM = 0,
    RGBA16_FLOAT,
    RGBA32_FLOAT,
    R8_UNORM,
    RGB8_UNORM
};

struct TextureInfo {
    TextureHandle handle;
    std::string   path;
    i32           width        = 0;
    i32           height       = 0;
    i32           channels     = 0;
    TextureFormat format       = TextureFormat::RGBA8_UNORM;
    usize         memory_bytes = 0;
    std::vector<u8> pixels;

    bool IsValid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager() = default;

    TextureHandle LoadTexture(const std::string& filepath, bool flip_y = false);
    TextureHandle RegisterTexture(const std::string& name, i32 width, i32 height, i32 channels,
                                  TextureFormat format, const std::vector<u8>& data);

    const TextureInfo* GetTexture(TextureHandle handle) const;
    const TextureInfo* FindTexture(const std::string& filepath) const;
    std::vector<const TextureInfo*> GetAllTextures() const;

    void UnloadTexture(TextureHandle handle);
    void Clear();

    static TextureManager& Instance();

private:
    u32 m_next_id = 0;
    std::unordered_map<u32, TextureInfo> m_textures;
    std::unordered_map<std::string, TextureHandle> m_path_to_handle;
};

} // namespace lucida
