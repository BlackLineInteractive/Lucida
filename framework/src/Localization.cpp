// Lucida Engine
// Copyright (C) 2026 BlackLine Interactive
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lucida/framework/Localization.h"
#include <unordered_map>
#include <string>

namespace lucida {

static Language s_current_language = Language::English;

struct TranslationKeyHash {
    std::size_t operator()(const std::pair<std::string, Language>& p) const {
        return std::hash<std::string>()(p.first) ^ (std::hash<int>()(static_cast<int>(p.second)) << 1);
    }
};

static const std::unordered_map<std::pair<std::string, Language>, const char*, TranslationKeyHash> s_dictionary = {
    // Menu Bar
    { {"menu_file", Language::English}, "File" },
    { {"menu_file", Language::Ukrainian}, "Файл" },
    { {"menu_file", Language::Russian}, "Файл" },
    { {"menu_file", Language::German}, "Datei" },
    { {"menu_file", Language::French}, "Fichier" },
    { {"menu_file", Language::Spanish}, "Archivo" },

    { {"menu_edit", Language::English}, "Edit" },
    { {"menu_edit", Language::Ukrainian}, "Правка" },
    { {"menu_edit", Language::Russian}, "Правка" },
    { {"menu_edit", Language::German}, "Bearbeiten" },
    { {"menu_edit", Language::French}, "Édition" },
    { {"menu_edit", Language::Spanish}, "Editar" },

    { {"menu_view", Language::English}, "View" },
    { {"menu_view", Language::Ukrainian}, "Вигляд" },
    { {"menu_view", Language::Russian}, "Вид" },
    { {"menu_view", Language::German}, "Ansicht" },
    { {"menu_view", Language::French}, "Affichage" },
    { {"menu_view", Language::Spanish}, "Ver" },

    { {"menu_scene", Language::English}, "Scene" },
    { {"menu_scene", Language::Ukrainian}, "Сцена" },
    { {"menu_scene", Language::Russian}, "Сцена" },
    { {"menu_scene", Language::German}, "Szene" },
    { {"menu_scene", Language::French}, "Scène" },
    { {"menu_scene", Language::Spanish}, "Escena" },

    { {"menu_play", Language::English}, "Play" },
    { {"menu_play", Language::Ukrainian}, "Симуляція" },
    { {"menu_play", Language::Russian}, "Симуляция" },

    { {"menu_help", Language::English}, "Help" },
    { {"menu_help", Language::Ukrainian}, "Довідка" },
    { {"menu_help", Language::Russian}, "Справка" },

    { {"menu_language", Language::English}, "Language" },
    { {"menu_language", Language::Ukrainian}, "Мова" },
    { {"menu_language", Language::Russian}, "Язык" },
    { {"menu_language", Language::German}, "Sprache" },
    { {"menu_language", Language::French}, "Langue" },
    { {"menu_language", Language::Spanish}, "Idioma" },

    // Windows & Panels
    { {"panel_viewport", Language::English}, "Viewport" },
    { {"panel_viewport", Language::Ukrainian}, "В'юпорт" },
    { {"panel_viewport", Language::Russian}, "Вьюпорт" },

    { {"panel_hierarchy", Language::English}, "Hierarchy" },
    { {"panel_hierarchy", Language::Ukrainian}, "Ієрархія" },
    { {"panel_hierarchy", Language::Russian}, "Иерархия" },

    { {"panel_inspector", Language::English}, "Inspector" },
    { {"panel_inspector", Language::Ukrainian}, "Інспектор" },
    { {"panel_inspector", Language::Russian}, "Инспектор" },

    { {"panel_console", Language::English}, "Console" },
    { {"panel_console", Language::Ukrainian}, "Консоль" },
    { {"panel_console", Language::Russian}, "Консоль" },

    { {"panel_mesh_editor", Language::English}, "Mesh Editor" },
    { {"panel_mesh_editor", Language::Ukrainian}, "Редактор мешу" },
    { {"panel_mesh_editor", Language::Russian}, "Редактор меша" },

    { {"panel_content_browser", Language::English}, "Content Browser" },
    { {"panel_content_browser", Language::Ukrainian}, "Файловий менеджер" },
    { {"panel_content_browser", Language::Russian}, "Файловый браузер" },

    { {"panel_texture_maps", Language::English}, "Texture Maps" },
    { {"panel_texture_maps", Language::Ukrainian}, "Текстури" },
    { {"panel_texture_maps", Language::Russian}, "Текстуры" },

    { {"panel_statistics", Language::English}, "Statistics" },
    { {"panel_statistics", Language::Ukrainian}, "Статистика" },
    { {"panel_statistics", Language::Russian}, "Статистика" },

    { {"panel_graphics", Language::English}, "Graphics Settings" },
    { {"panel_graphics", Language::Ukrainian}, "Налаштування графіки" },
    { {"panel_graphics", Language::Russian}, "Настройки графики" },

    { {"panel_gameplay", Language::English}, "Gameplay Debugger" },
    { {"panel_gameplay", Language::Ukrainian}, "Геймплейний дебагер" },
    { {"panel_gameplay", Language::Russian}, "Геймплейный дебаггер" },

    // Selection & Actions
    { {"action_select_all", Language::English}, "Select All (A)" },
    { {"action_select_all", Language::Ukrainian}, "Виділити все (A)" },
    { {"action_select_all", Language::Russian}, "Выделить все (A)" },

    { {"action_deselect_all", Language::English}, "Deselect All (Alt+A)" },
    { {"action_deselect_all", Language::Ukrainian}, "Зняти виділення (Alt+A)" },
    { {"action_deselect_all", Language::Russian}, "Снять выделение (Alt+A)" },

    { {"action_group", Language::English}, "Group (Ctrl+G)" },
    { {"action_group", Language::Ukrainian}, "Згрупувати (Ctrl+G)" },
    { {"action_group", Language::Russian}, "Сгруппировать (Ctrl+G)" },

    { {"action_ungroup", Language::English}, "Ungroup (Ctrl+Alt+G)" },
    { {"action_ungroup", Language::Ukrainian}, "Розгрупувати (Ctrl+Alt+G)" },
    { {"action_ungroup", Language::Russian}, "Разгруппировать (Ctrl+Alt+G)" },

    { {"action_join", Language::English}, "Join Meshes (Ctrl+J)" },
    { {"action_join", Language::Ukrainian}, "Об'єднати меші (Ctrl+J)" },
    { {"action_join", Language::Russian}, "Объединить меши (Ctrl+J)" },

    { {"action_separate", Language::English}, "Separate Mesh (P)" },
    { {"action_separate", Language::Ukrainian}, "Розділити меш (P)" },
    { {"action_separate", Language::Russian}, "Разделить меш (P)" },

    { {"action_duplicate", Language::English}, "Duplicate (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::Ukrainian}, "Дублювати (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::Russian}, "Дублировать (Cmd+D / Ctrl+D)" },

    { {"action_delete", Language::English}, "Delete (Del)" },
    { {"action_delete", Language::Ukrainian}, "Видалити (Del)" },
    { {"action_delete", Language::Russian}, "Удалить (Del)" },

    { {"action_add", Language::English}, "Add..." },
    { {"action_add", Language::Ukrainian}, "Додати..." },
    { {"action_add", Language::Russian}, "Добавить..." },

    { {"action_copy", Language::English}, "Copy" },
    { {"action_copy", Language::Ukrainian}, "Копіювати" },
    { {"action_copy", Language::Russian}, "Копировать" },

    { {"action_boolean", Language::English}, "Boolean..." },
    { {"action_boolean", Language::Ukrainian}, "Boolean..." },
    { {"action_boolean", Language::Russian}, "Boolean..." },

    { {"action_link", Language::English}, "Link" },
    { {"action_link", Language::Ukrainian}, "Зв'язати" },
    { {"action_link", Language::Russian}, "Создать" },

    { {"action_unlink", Language::English}, "Unlink" },
    { {"action_unlink", Language::Ukrainian}, "Зняти" },
    { {"action_unlink", Language::Russian}, "Снять" },

    { {"tree_frame", Language::English}, "Frame / Scene" },
    { {"tree_frame", Language::Ukrainian}, "Кадр / Сцена" },
    { {"tree_frame", Language::Russian}, "Кадр / Сцена" },

    // Selection Tools
    { {"tool_select_point", Language::English}, "Point Select" },
    { {"tool_select_point", Language::Ukrainian}, "Виділення точкою" },
    { {"tool_select_point", Language::Russian}, "Выделение точкой" },

    { {"tool_select_box", Language::English}, "Box Select" },
    { {"tool_select_box", Language::Ukrainian}, "Рамка виділення" },
    { {"tool_select_box", Language::Russian}, "Прямоугольное выделение" },

    { {"tool_select_lasso", Language::English}, "Lasso Select" },
    { {"tool_select_lasso", Language::Ukrainian}, "Ласо" },
    { {"tool_select_lasso", Language::Russian}, "Лассо" },

    // Repeaters (Roadmap terms)
    { {"repeater_title", Language::English}, "Repeaters" },
    { {"repeater_title", Language::Ukrainian}, "Репітери" },
    { {"repeater_title", Language::Russian}, "Репитеры" },

    { {"repeater_array", Language::English}, "Array / Grid" },
    { {"repeater_array", Language::Ukrainian}, "Масив" },
    { {"repeater_array", Language::Russian}, "Массив" },

    { {"repeater_curves", Language::English}, "Curves" },
    { {"repeater_curves", Language::Ukrainian}, "Криві" },
    { {"repeater_curves", Language::Russian}, "Кривые" },

    { {"repeater_radial", Language::English}, "Radial" },
    { {"repeater_radial", Language::Ukrainian}, "Радіально" },
    { {"repeater_radial", Language::Russian}, "Радиально" },

    { {"repeater_mirror", Language::English}, "Mirror" },
    { {"repeater_mirror", Language::Ukrainian}, "Віддзеркалення" },
    { {"repeater_mirror", Language::Russian}, "Отзеркаливание" },

    // Play Mode Controls
    { {"btn_play", Language::English}, "Play" },
    { {"btn_play", Language::Ukrainian}, "Грати" },
    { {"btn_play", Language::Russian}, "Играть" },

    { {"btn_stop", Language::English}, "Stop" },
    { {"btn_stop", Language::Ukrainian}, "Зупинити" },
    { {"btn_stop", Language::Russian}, "Стоп" },

    { {"btn_pause", Language::English}, "Pause" },
    { {"btn_pause", Language::Ukrainian}, "Пауза" },
    { {"btn_pause", Language::Russian}, "Пауза" },

    { {"btn_resume", Language::English}, "Resume" },
    { {"btn_resume", Language::Ukrainian}, "Продовжити" },
    { {"btn_resume", Language::Russian}, "Продолжить" },

    { {"btn_step", Language::English}, "Step" },
    { {"btn_step", Language::Ukrainian}, "Крок" },
    { {"btn_step", Language::Russian}, "Шаг" },
};

void Localization::SetLanguage(Language lang) {
    s_current_language = lang;
}

Language Localization::GetLanguage() {
    return s_current_language;
}

const char* Localization::GetLanguageName(Language lang) {
    switch (lang) {
    case Language::English:   return "English";
    case Language::Ukrainian: return "Українська";
    case Language::Russian:   return "Русский";
    case Language::German:    return "Deutsch";
    case Language::French:    return "Français";
    case Language::Spanish:   return "Español";
    default:                  return "English";
    }
}

const char* Localization::Get(const char* key, const char* fallback) {
    if (!key) return fallback ? fallback : "";

    auto it = s_dictionary.find({std::string(key), s_current_language});
    if (it != s_dictionary.end()) {
        return it->second;
    }

    // Fallback to English if not translated in current language
    if (s_current_language != Language::English) {
        auto it_en = s_dictionary.find({std::string(key), Language::English});
        if (it_en != s_dictionary.end()) {
            return it_en->second;
        }
    }

    return fallback ? fallback : key;
}

} // namespace lucida
