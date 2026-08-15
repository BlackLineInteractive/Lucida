# Lucida Engine — архітектура

Документ фіксує **правила**, за якими росте рушій. Усе, що суперечить цим правилам,
не потрапляє в дерево. Кожен модуль прив'язаний до конкретного розділу джерел:

| Джерело | Скорочення | Роль у проєкті |
|---|---|---|
| Jason Gregory, *Game Engine Architecture* (3rd ed.) | **GEA** | шарування рушія, склад підсистем |
| Robert Nystrom, *Game Programming Patterns* — gameprogrammingpatterns.com | **GPP** | патерни всередині шарів |
| Richard Fabian, *Data-Oriented Design* — dataorienteddesign.com/dodbook | **DOD** | розкладка пам'яті, SoA, ECS |

Розділи, на які спираємось, за фактичним змістом книг:

* **GPP гл.3 Sequencing** — Game Loop, Update Method, Double Buffer → `engine/runtime`
* **GPP гл.5 Decoupling** — Component, Event Queue, Service Locator → `engine/core`
* **GPP гл.6 Optimization** — Data Locality, Object Pool, Spatial Partition, Dirty Flag
* **DOD гл.2 Relational Databases** — нормалізація, первинні ключі → хендли замість вказівників
* **DOD гл.4 Component Based Objects** — ECS замість ієрархій
* **DOD гл.8 Optimisations** — SoA проти AoS → розкол `GPUTriPos` / `GPUTriAttr`
* **DOD гл.9 Helping the Compiler** — кеш, aliasing, передбачення переходів

---

## 1. Шари рантайму (GEA, розділ 1.6)

Знизу вгору. **Стрілки залежностей ідуть тільки вниз.** Верхній шар знає про нижній;
нижній про верхній — ніколи.

```
                         ┌──────────────────────────┐
     apps/               │  sandbox, benchmark      │  ігра/інструмент
                         └────────────┬─────────────┘
                                      │
                         ┌────────────┴─────────────┐
     framework/          │  UI/UX, редакторська     │  GEA 1.6.15 (Game-Specific)
                         │  оболонка, debug-меню    │  GPP: Command, State
                         └────────────┬─────────────┘
                                      │
   ┌──────────┬───────────┬───────────┼───────────┬─────────────┐
   │ runtime  │  render   │  physics  │ resource  │   input     │  GEA 1.6.9–1.6.14
   │ Game Loop│ front-end │ інтерфейс │ менеджер  │  HID layer  │
   │ World    │ RenderList│ бекендів  │ ресурсів  │             │
   └────┬─────┴─────┬─────┴─────┬─────┴─────┬─────┴──────┬──────┘
        │           │           │           │            │
        └───────────┴───────────┴─────┬─────┴────────────┘
                                      │
                         ┌────────────┴─────────────┐
     engine/core/        │ memory, containers, math │  GEA 1.6.5 (Core Systems)
                         │ diag, events, services   │  GEA 1.6.4 (Platform Independence)
                         │ ecs, platform            │  DOD 1–5
                         └──────────────────────────┘

     backends/           реалізації інтерфейсів: render_metal, render_vulkan, render_gl,
                         physics_jolt, physics_bullet, platform_sdl2
                         ↑ залежать від свого engine-модуля, і ні від чого більше
```

### Залізне правило залежностей

1. `core` не залежить **ні від чого**, окрім stdlib і `glm`.
2. Модулі рівня `engine/*` залежать тільки від `core` (і, де вказано, від `resource`).
3. `backends/*` залежать від **свого** інтерфейсного модуля. Бекенд ніколи не
   лінкується в інший бекенд.
4. Вибір бекенда робить **застосунок** (`apps/*`), а не рушій. Рушій знає лише інтерфейс.
5. Жоден `engine/*` не має `#include` на SDL2, Metal, Jolt, Bullet, ImGui.

CMake це підтримує механічно: кожен модуль — окрема ціль з `target_link_libraries`,
де перелічені **тільки** дозволені сусіди. Порушення = помилка лінкування, а не код-рев'ю.

---

## 2. Що звідки взято, а що написано

Правило: **математику, BVH/SAH і фізику не переписуємо.** Беремо найкраще з наявного
і загортаємо у власний фасад, щоб бібліотека була замінна.

| Підсистема | Рішення | Чому |
|---|---|---|
| Векторна математика | `glm` | de-facto стандарт, SIMD-шляхи, GLSL-семантика |
| BVH + binned SAH | `bvh v2` (madmann91) | багатопотокова побудова, перевірена якість дерева |
| Фізика | `Jolt` (типово), `Bullet` (опція) | Jolt: багатопотоковий, кеш-дружній, AAA-походження |
| Імпорт моделей | `assimp` | glTF 2.0 / GLB / OBJ |
| UI | `Dear ImGui` | immediate-mode, нульова інфраструктура |
| Вікно/ввід | `SDL2` | ізольовано в `backends/platform_sdl2` |

Написано з нуля тільки те, чого не купити готовим: шари, алокатори, ECS, ігровий цикл,
рендер-фронтенд і трасувальні ядра.

---

## 3. Модулі

### `engine/core` — GEA 1.6.4–1.6.5, DOD
| Тека | Вміст | Джерело |
|---|---|---|
| `platform/` | типи фіксованої ширини, детект платформи, монотонний час | GEA 1.6.4, 8.5 |
| `memory/` | `LinearAllocator` з маркерами, `PoolAllocator`, `FrameArena` | GEA 6.2 |
| `container/` | `Handle` з поколіннями, `HandleTable` зі щільним сховищем | DOD 2; GPP: Object Pool |
| `math/` | фасад над glm + `AABB`, `Ray`, `Transform` | — |
| `diag/` | `LUCIDA_ASSERT`/`VERIFY`, канальний лог, скоуп-профайлер | GEA 3.3.3, 3.5 |
| `event/` | `EventQueue` фіксованої місткості | GPP гл.5 |
| `service/` | `Locator<T>` | GPP гл.5 |
| `ecs/` | сховище сутностей у SoA, архетипи | DOD 4 — **ще не написано** |

### `engine/runtime` — GPP: Game Loop, Update Method
Фіксований крок симуляції + інтерполяція рендера, `World`, системи, `Application`.

### `engine/render`
Front-end: `RenderScene` (SoA), камера, список інстансів, інтерфейс `IRenderBackend`.
Жодного API графіки тут немає.

### `engine/physics`
`IPhysicsBackend`, `RigidBodyDesc`, `VehicleDesc` — чиста абстракція над Jolt/Bullet.

### `engine/resource` — GEA 6.2 (Resource Manager)
Завантаження мешів, пакування текстурних масивів, побудова BLAS.

### `framework`
Оболонка UI/UX: debug-меню, статистика, файловий діалог, гарячі клавіші.

---

## 4. Правила даних (DOD)

* Гарячі дані — **SoA**, а не масив об'єктів. `GPUTriPos` окремо від `GPUTriAttr`:
  обхід BVH читає 48 байт замість 128.
* Ідентифікація — цілочисельні хендли, не вказівники. Хендл переживає relocation.
* Віртуальні виклики дозволені **на межі підсистем** (раз на кадр), заборонені в
  пер-елементних циклах.
* Виділення пам'яті в кадровому циклі — тільки з арени, що скидається одним `Reset()`.

---

## 5. Статус

| Віха | Зміст | Стан |
|---|---|---|
| M0 | скелет, CMake-граф, документ | ✅ |
| M1 | `core`: пам'ять, контейнери, діагностика, події | ✅ |
| M2 | `runtime`: Game Loop, World, Update Method | ✅ |
| M3 | платформа SDL2 + framework UI | ✅ |
| M4 | `render` + Metal-бекенд трасування | ✅ |
| M5 | `physics` + Jolt | ✅ |
| M6 | sandbox збирається й запускається | ✅ |
| M7 | ECS-архетипи, GL/Vulkan бекенди на новому інтерфейсі, Bullet, FSR | ⏳ |

Перевірено: `lucida_sandbox --bench 90 --shot f.png` дає 57.8 fps на 1971×1065
променів (Intel Mac, Metal), знімок кадру записується.

### Борг, свідомо залишений

* Демо-сцени (`SetDemoScene`) досі всередині Metal-бекенду — авторинг сцен має
  переїхати в `engine/render`. Це шов, по якому різатимемо.
* `LoadMesh` як окремий шлях поряд з `AddMesh`/`AddInstance` — лишився заради
  сумісності, підлягає видаленню.
* `IPhysicsBackend::CreateBody` у Jolt-бекенді ще не реалізовано: світ поки
  вміє лише транспорт і статичну землю.
* GL і Vulkan бекенди лежать у дереві в старому вигляді й не збираються.
