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
    { {"menu_play", Language::German}, "Simulation" },
    { {"menu_play", Language::French}, "Simulation" },
    { {"menu_play", Language::Spanish}, "Simulación" },

    { {"menu_demos", Language::English}, "Demos" },
    { {"menu_demos", Language::Ukrainian}, "Демо-сцени" },
    { {"menu_demos", Language::Russian}, "Демо-сцены" },
    { {"menu_demos", Language::German}, "Demos" },
    { {"menu_demos", Language::French}, "Démos" },
    { {"menu_demos", Language::Spanish}, "Demos" },

    { {"menu_renderer", Language::English}, "Renderer" },
    { {"menu_renderer", Language::Ukrainian}, "Рендерер" },
    { {"menu_renderer", Language::Russian}, "Рендерер" },
    { {"menu_renderer", Language::German}, "Renderer" },
    { {"menu_renderer", Language::French}, "Moteur de rendu" },
    { {"menu_renderer", Language::Spanish}, "Renderizador" },

    { {"menu_settings", Language::English}, "Settings" },
    { {"menu_settings", Language::Ukrainian}, "Налаштування" },
    { {"menu_settings", Language::Russian}, "Настройки" },
    { {"menu_settings", Language::German}, "Einstellungen" },
    { {"menu_settings", Language::French}, "Paramètres" },
    { {"menu_settings", Language::Spanish}, "Configuración" },

    { {"menu_help", Language::English}, "Help" },
    { {"menu_help", Language::Ukrainian}, "Довідка" },
    { {"menu_help", Language::Russian}, "Справка" },
    { {"menu_help", Language::German}, "Hilfe" },
    { {"menu_help", Language::French}, "Aide" },
    { {"menu_help", Language::Spanish}, "Ayuda" },

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
    { {"action_duplicate", Language::English}, "Duplicate (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::Ukrainian}, "Дублювати (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::Russian}, "Дублировать (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::German}, "Duplizieren (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::French}, "Dupliquer (Cmd+D / Ctrl+D)" },
    { {"action_duplicate", Language::Spanish}, "Duplicar (Cmd+D / Ctrl+D)" },

    { {"action_delete", Language::English}, "Delete (Del)" },
    { {"action_delete", Language::Ukrainian}, "Видалити (Del)" },
    { {"action_delete", Language::Russian}, "Удалить (Del)" },
    { {"action_delete", Language::German}, "Löschen (Del)" },
    { {"action_delete", Language::French}, "Supprimer (Del)" },
    { {"action_delete", Language::Spanish}, "Eliminar (Del)" },

    { {"action_delete_short", Language::English}, "Delete" },
    { {"action_delete_short", Language::Ukrainian}, "Видалити" },
    { {"action_delete_short", Language::Russian}, "Удалить" },
    { {"action_delete_short", Language::German}, "Löschen" },
    { {"action_delete_short", Language::French}, "Supprimer" },
    { {"action_delete_short", Language::Spanish}, "Eliminar" },

    { {"action_add", Language::English}, "Add..." },
    { {"action_add", Language::Ukrainian}, "Додати..." },
    { {"action_add", Language::Russian}, "Добавить..." },
    { {"action_add", Language::German}, "Hinzufügen..." },
    { {"action_add", Language::French}, "Ajouter..." },
    { {"action_add", Language::Spanish}, "Añadir..." },

    { {"action_join_short", Language::English}, "Join" },
    { {"action_join_short", Language::Ukrainian}, "Злити" },
    { {"action_join_short", Language::Russian}, "Объединить" },
    { {"action_join_short", Language::German}, "Verbinden" },
    { {"action_join_short", Language::French}, "Joindre" },
    { {"action_join_short", Language::Spanish}, "Unir" },

    { {"action_separate_short", Language::English}, "Separate" },
    { {"action_separate_short", Language::Ukrainian}, "Розділити" },
    { {"action_separate_short", Language::Russian}, "Разделить" },
    { {"action_separate_short", Language::German}, "Trennen" },
    { {"action_separate_short", Language::French}, "Séparer" },
    { {"action_separate_short", Language::Spanish}, "Separar" },

    { {"action_group_short", Language::English}, "Group" },
    { {"action_group_short", Language::Ukrainian}, "Група" },
    { {"action_group_short", Language::Russian}, "Группа" },
    { {"action_group_short", Language::German}, "Gruppe" },
    { {"action_group_short", Language::French}, "Groupe" },
    { {"action_group_short", Language::Spanish}, "Grupo" },

    { {"action_ungroup_short", Language::English}, "Ungroup" },
    { {"action_ungroup_short", Language::Ukrainian}, "Зняти" },
    { {"action_ungroup_short", Language::Russian}, "Снять" },
    { {"action_ungroup_short", Language::German}, "Auflösen" },
    { {"action_ungroup_short", Language::French}, "Dissocier" },
    { {"action_ungroup_short", Language::Spanish}, "Desagrupar" },

    { {"action_copy", Language::English}, "Copy" },
    { {"action_copy", Language::Ukrainian}, "Копія" },
    { {"action_copy", Language::Russian}, "Копия" },
    { {"action_copy", Language::German}, "Kopieren" },
    { {"action_copy", Language::French}, "Copier" },
    { {"action_copy", Language::Spanish}, "Copiar" },

    { {"action_boolean", Language::English}, "Boolean..." },
    { {"action_boolean", Language::Ukrainian}, "Boolean..." },
    { {"action_boolean", Language::Russian}, "Boolean..." },
    { {"action_boolean", Language::German}, "Boolesch..." },
    { {"action_boolean", Language::French}, "Booléen..." },
    { {"action_boolean", Language::Spanish}, "Booleano..." },

    { {"action_link", Language::English}, "Link" },
    { {"action_link", Language::Ukrainian}, "Зв'язати" },
    { {"action_link", Language::Russian}, "Связать" },
    { {"action_link", Language::German}, "Verknüpfen" },
    { {"action_link", Language::French}, "Lier" },
    { {"action_link", Language::Spanish}, "Vincular" },

    { {"action_unlink", Language::English}, "Unlink" },
    { {"action_unlink", Language::Ukrainian}, "Зняти" },
    { {"action_unlink", Language::Russian}, "Снять" },
    { {"action_unlink", Language::German}, "Trennen" },
    { {"action_unlink", Language::French}, "Délier" },
    { {"action_unlink", Language::Spanish}, "Desvincular" },

    { {"tree_frame", Language::English}, "Frame / Scene" },
    { {"tree_frame", Language::Ukrainian}, "Кадр / Сцена" },
    { {"tree_frame", Language::Russian}, "Кадр / Сцена" },
    { {"tree_frame", Language::German}, "Bild / Szene" },
    { {"tree_frame", Language::French}, "Image / Scène" },
    { {"tree_frame", Language::Spanish}, "Fotograma / Escena" },

    { {"search_entities", Language::English}, "Search entities..." },
    { {"search_entities", Language::Ukrainian}, "Пошук об'єктів..." },
    { {"search_entities", Language::Russian}, "Поиск объектов..." },
    { {"search_entities", Language::German}, "Objekte suchen..." },
    { {"search_entities", Language::French}, "Rechercher des entités..." },
    { {"search_entities", Language::Spanish}, "Buscar entidades..." },

    { {"empty_scene", Language::English}, "Empty scene." },
    { {"empty_scene", Language::Ukrainian}, "Порожня сцена." },
    { {"empty_scene", Language::Russian}, "Пустая сцена." },
    { {"empty_scene", Language::German}, "Leere Szene." },
    { {"empty_scene", Language::French}, "Scène vide." },
    { {"empty_scene", Language::Spanish}, "Escena vacía." },

    { {"empty_scene_hint", Language::English}, "Click '+ Add...' or import a 3D model." },
    { {"empty_scene_hint", Language::Ukrainian}, "Натисніть '+ Додати...' або завантажте модель." },
    { {"empty_scene_hint", Language::Russian}, "Нажмите '+ Добавить...' или загрузите модель." },
    { {"empty_scene_hint", Language::German}, "Klicken Sie auf '+ Hinzufügen...' oder importieren Sie ein 3D-Modell." },
    { {"empty_scene_hint", Language::French}, "Cliquez sur '+ Ajouter...' ou importez un modèle 3D." },
    { {"empty_scene_hint", Language::Spanish}, "Haga clic en '+ Añadir...' o importe un modelo 3D." },

    { {"tooltip_add", Language::English}, "Create new entity or primitive..." },
    { {"tooltip_add", Language::Ukrainian}, "Створити новий об'єкт або примітив..." },
    { {"tooltip_add", Language::Russian}, "Создать новый объект или примитив..." },
    { {"tooltip_add", Language::German}, "Neues Objekt oder Primitiv erstellen..." },
    { {"tooltip_add", Language::French}, "Créer une nouvelle entité ou primitive..." },
    { {"tooltip_add", Language::Spanish}, "Crear nueva entidad o primitiva..." },

    { {"tooltip_del", Language::English}, "Delete selected entities (Del)" },
    { {"tooltip_del", Language::Ukrainian}, "Видалити виділені об'єкти (Del)" },
    { {"tooltip_del", Language::Russian}, "Удалить выбранные объекты (Del)" },
    { {"tooltip_del", Language::German}, "Ausgewählte Objekte löschen (Del)" },
    { {"tooltip_del", Language::French}, "Supprimer les entités sélectionnées (Del)" },
    { {"tooltip_del", Language::Spanish}, "Eliminar entidades seleccionadas (Del)" },

    { {"tooltip_join", Language::English}, "Join selected meshes into one entity (Ctrl+J)" },
    { {"tooltip_join", Language::Ukrainian}, "Об'єднати виділені меші в одну сутність (Ctrl+J)" },
    { {"tooltip_join", Language::Russian}, "Объединить выбранные меши в один объект (Ctrl+J)" },
    { {"tooltip_join", Language::German}, "Ausgewählte Meshes verbinden (Ctrl+J)" },
    { {"tooltip_join", Language::French}, "Joindre les maillages sélectionnés (Ctrl+J)" },
    { {"tooltip_join", Language::Spanish}, "Unir mallas seleccionadas (Ctrl+J)" },

    { {"tooltip_unparent", Language::English}, "Unparent from parent entity" },
    { {"tooltip_unparent", Language::Ukrainian}, "Від'єднати від батьківського об'єкта" },
    { {"tooltip_unparent", Language::Russian}, "Отсоединить от родительского объекта" },
    { {"tooltip_unparent", Language::German}, "Vom übergeordneten Objekt trennen" },
    { {"tooltip_unparent", Language::French}, "Détacher de l'entité parente" },
    { {"tooltip_unparent", Language::Spanish}, "Desvincular de la entidad padre" },

    { {"tooltip_boolean", Language::English}, "CSG Boolean operations (Union, Difference, Intersection)" },
    { {"tooltip_boolean", Language::Ukrainian}, "CSG Boolean операції (Об'єднання, Віднімання, Перетин)" },
    { {"tooltip_boolean", Language::Russian}, "CSG Boolean операции (Объединение, Вычитание, Пересечение)" },
    { {"tooltip_boolean", Language::German}, "CSG Boolesche Operationen (Vereinigung, Differenz, Schnittmenge)" },
    { {"tooltip_boolean", Language::French}, "Opérations booléennes CSG (Union, Différence, Intersection)" },
    { {"tooltip_boolean", Language::Spanish}, "Operaciones booleanas CSG (Unión, Diferencia, Intersección)" },

    { {"tooltip_duplicate", Language::English}, "Duplicate selection (Cmd+D / Ctrl+D)" },
    { {"tooltip_duplicate", Language::Ukrainian}, "Дублювати виділення (Cmd+D / Ctrl+D)" },
    { {"tooltip_duplicate", Language::Russian}, "Дублировать выделение (Cmd+D / Ctrl+D)" },
    { {"tooltip_duplicate", Language::German}, "Auswahl duplizieren (Cmd+D / Ctrl+D)" },
    { {"tooltip_duplicate", Language::French}, "Dupliquer la sélection (Cmd+D / Ctrl+D)" },
    { {"tooltip_duplicate", Language::Spanish}, "Duplicar selección (Cmd+D / Ctrl+D)" },

    { {"tooltip_group", Language::English}, "Group selected entities (Ctrl+G)" },
    { {"tooltip_group", Language::Ukrainian}, "Згрупувати виділені об'єкти (Ctrl+G)" },
    { {"tooltip_group", Language::Russian}, "Сгруппировать выбранные объекты (Ctrl+G)" },
    { {"tooltip_group", Language::German}, "Ausgewählte Objekte gruppieren (Ctrl+G)" },
    { {"tooltip_group", Language::French}, "Grouper les entités sélectionnées (Ctrl+G)" },
    { {"tooltip_group", Language::Spanish}, "Agrupar entidades seleccionadas (Ctrl+G)" },

    { {"tooltip_ungroup", Language::English}, "Ungroup group (Ctrl+Alt+G)" },
    { {"tooltip_ungroup", Language::Ukrainian}, "Розгрупувати групу (Ctrl+Alt+G)" },
    { {"tooltip_ungroup", Language::Russian}, "Разгруппировать группу (Ctrl+Alt+G)" },
    { {"tooltip_ungroup", Language::German}, "Gruppe auflösen (Ctrl+Alt+G)" },
    { {"tooltip_ungroup", Language::French}, "Dissocier le groupe (Ctrl+Alt+G)" },
    { {"tooltip_ungroup", Language::Spanish}, "Desagrupar grupo (Ctrl+Alt+G)" },

    { {"tooltip_shortcuts", Language::English}, "Shortcuts:\n • A: Select All\n • Alt+A: Deselect All\n • Shift+Click: Multi-select\n • Ctrl+G: Group\n • Ctrl+Alt+G: Ungroup\n • Ctrl+J: Join Meshes\n • Cmd+D / Ctrl+D: Duplicate\n • Del: Delete" },
    { {"tooltip_shortcuts", Language::Ukrainian}, "Гарячі клавіші:\n • A: Виділити все\n • Alt+A: Зняти виділення\n • Shift+Клік: Мультиселекція\n • Ctrl+G: Згрупувати\n • Ctrl+Alt+G: Розгрупувати\n • Ctrl+J: Об'єднати меші\n • Cmd+D / Ctrl+D: Дублювати\n • Del: Видалити" },
    { {"tooltip_shortcuts", Language::Russian}, "Горячие клавиши:\n • A: Выделить все\n • Alt+A: Снять выделение\n • Shift+Клик: Мультиселекция\n • Ctrl+G: Сгруппировать\n • Ctrl+Alt+G: Разгруппировать\n • Ctrl+J: Объединить меши\n • Cmd+D / Ctrl+D: Дублировать\n • Del: Удалить" },
    { {"tooltip_shortcuts", Language::German}, "Tastaturkürzel:\n • A: Alles auswählen\n • Alt+A: Auswahl aufheben\n • Shift+Klick: Mehrfachauswahl\n • Ctrl+G: Gruppieren\n • Ctrl+Alt+G: Gruppe auflösen\n • Ctrl+J: Meshes verbinden\n • Cmd+D / Ctrl+D: Duplizieren\n • Del: Löschen" },
    { {"tooltip_shortcuts", Language::French}, "Raccourcis:\n • A: Tout sélectionner\n • Alt+A: Tout désélectionner\n • Shift+Clic: Sélection multiple\n • Ctrl+G: Grouper\n • Ctrl+Alt+G: Dissocier\n • Ctrl+J: Joindre les maillages\n • Cmd+D / Ctrl+D: Dupliquer\n • Del: Supprimer" },
    { {"tooltip_shortcuts", Language::Spanish}, "Atajos:\n • A: Seleccionar todo\n • Alt+A: Deseleccionar todo\n • Shift+Clic: Selección múltiple\n • Ctrl+G: Agrupar\n • Ctrl+Alt+G: Desagrupar\n • Ctrl+J: Unir mallas\n • Cmd+D / Ctrl+D: Duplicar\n • Del: Eliminar" },

    { {"boolean_union", Language::English}, "Union" },
    { {"boolean_union", Language::Ukrainian}, "Об'єднання" },
    { {"boolean_union", Language::Russian}, "Объединение" },
    { {"boolean_union", Language::German}, "Vereinigung" },
    { {"boolean_union", Language::French}, "Union" },
    { {"boolean_union", Language::Spanish}, "Unión" },

    { {"boolean_difference", Language::English}, "Difference" },
    { {"boolean_difference", Language::Ukrainian}, "Віднімання" },
    { {"boolean_difference", Language::Russian}, "Вычитание" },
    { {"boolean_difference", Language::German}, "Differenz" },
    { {"boolean_difference", Language::French}, "Différence" },
    { {"boolean_difference", Language::Spanish}, "Diferencia" },

    { {"boolean_intersection", Language::English}, "Intersection" },
    { {"boolean_intersection", Language::Ukrainian}, "Перетин" },
    { {"boolean_intersection", Language::Russian}, "Пересечение" },
    { {"boolean_intersection", Language::German}, "Schnittmenge" },
    { {"boolean_intersection", Language::French}, "Intersection" },
    { {"boolean_intersection", Language::Spanish}, "Intersección" },

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
