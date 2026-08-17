// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <unordered_map>

namespace lucida {

enum class Language {
    English,
    Ukrainian,
    Russian,
    German,
    French,
    Spanish
};

class Localization {
public:
    static void SetLanguage(Language lang);
    static Language GetLanguage();
    static const char* GetLanguageName(Language lang);

    // Look up translated string by key with fallback default text
    static const char* Get(const char* key, const char* fallback = nullptr);
};

// Convenience macro
inline const char* TR(const char* key, const char* fallback = nullptr) {
    return Localization::Get(key, fallback);
}

} // namespace lucida
