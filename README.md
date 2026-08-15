# Lucida Engine

Модульний ігровий рушій із трасуванням променів у реальному часі.
Архітектура — строго за трьома джерелами: **Game Engine Architecture** (Gregory) для
шарів і складу підсистем, **Game Programming Patterns** (Nystrom) для патернів
усередині шарів, **Data-Oriented Design** (Fabian) для розкладки пам'яті.

Повний опис і правила — [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Збірка

Залежності **не зберігаються в репозиторії** — CMake тягне їх сам під час конфігурації
(glm, bvh v2, Dear ImGui, ImGuiFileDialog, stb, Jolt; SDL2 та assimp беруться системні,
якщо знайдені, інакше збираються з джерел).

```bash
cmake -B build
cmake --build build -j
./build/lucida_sandbox
```

### Платформи

| Платформа | Архітектури | Рендер | Стан |
|---|---|---|---|
| macOS 13+ | x86_64, arm64 | Metal + MetalFX | ✅ повний шлях трасування |
| Linux (Debian/Ubuntu) | x86_64, arm64 | OpenGL 4.3 compute | ⏳ бекенд ще не переведений на `IRenderBackend` |
| Windows 10+ | x86_64, x86 | OpenGL 4.3 / Vulkan | ⏳ те саме |

Ядро, рантайм, ресурси, фізика, ввід і фреймворк збираються на всіх трьох системах.
Готовий до роботи бекенд рендеру наразі один — Metal; GL і Vulkan лежать у
`backends/render_gl` та `backends/render_vulkan` у вигляді, успадкованому з
RayTracer_Unified, і ще не підключені до нового інтерфейсу.

**Debian/Ubuntu:**
```bash
sudo apt install build-essential cmake ninja-build libsdl2-dev libassimp-dev
```

**Windows (MSVC):**
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config RelWithDebInfo
```

**macOS universal (x86_64 + arm64):**
```bash
cmake -B build -DLUCIDA_MACOS_UNIVERSAL=ON
```

### Опції CMake

| Опція | Типово | Що робить |
|---|---|---|
| `LUCIDA_RENDER_METAL` | ON на Apple | Metal-бекенд |
| `LUCIDA_RENDER_GL` | ON поза Apple | OpenGL 4.3 compute |
| `LUCIDA_RENDER_VULKAN` | OFF | Vulkan (заготовка) |
| `LUCIDA_PHYSICS_JOLT` | ON | Jolt |
| `LUCIDA_PHYSICS_BULLET` | OFF | Bullet (взаємовиключно з Jolt) |
| `LUCIDA_PREFER_SYSTEM_DEPS` | ON | Брати системні SDL2/assimp, якщо є |
| `LUCIDA_MACOS_UNIVERSAL` | OFF | Універсальний бінарник macOS |

---

## Запуск

```bash
./build/lucida_sandbox --mesh model.glb        # завантажити модель на старті
./build/lucida_sandbox --bench 90              # 90 кадрів, вивести часи
./build/lucida_sandbox --bench 90 --shot f.png # те саме + знімок кадру
./build/lucida_sandbox --verbose               # debug-рівень логів
```

`--bench` працює без спостерігача: це спосіб перевірити, що рендер живий, не
дивлячись у вікно.

### Керування

| Клавіші | Дія |
|---|---|
| W A S D | рух |
| Shift | спринт |
| Space / Ctrl | стрибок і присідання (walk) або вгору-вниз (fly) |
| F | перемкнути walk / fly |
| Tab, Esc | меню й курсор |
| V | туман |
| F11 | повний екран |

---

## Структура

```
Lucida/
├── cmake/Dependencies.cmake   завантаження сторонніх бібліотек
├── engine/
│   ├── core/       пам'ять, контейнери, математика, лог, події, сервіси
│   ├── runtime/    Game Loop з фіксованим кроком, World, системи
│   ├── render/     RenderScene, камера, інтерфейс IRenderBackend
│   ├── resource/   імпорт моделей, текстурні масиви, побудова BLAS
│   ├── physics/    IPhysicsBackend
│   └── input/      шар HID: дії замість скан-кодів
├── backends/
│   ├── render_metal/    Metal + MetalFX
│   ├── platform_sdl2/   вікно, ввід, поверхня
│   └── physics_jolt/    Jolt
├── framework/      UI/UX: ImGui-оболонка, контролер камери
└── apps/sandbox/   демо, яке зшиває конкретні бекенди
```

Правило, що тримає все разом: **модуль бачить лише те, що перелічено в його
`target_link_libraries`.** Жоден `engine/*` не має `#include` на SDL2, Metal, Jolt
чи ImGui — вибір бекендів робить застосунок.
