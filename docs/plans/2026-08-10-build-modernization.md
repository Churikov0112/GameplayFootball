# Build Modernization Implementation Plan

> **Для агентных исполнителей:** РЕКОМЕНДУЕТСЯ РЕАЛИЗОВАТЬ ЧЕРЕЗ `executing-plans`.
> Шаги используют чекбоксы (`- [ ]`). Каждая задача заканчивается сборкой и
> `determinism_runner check` (эталон `fa19b1875faeb951bf51c4018bed1c27fd27aa82`).

**Goal:** Модернизировать сборку (CMake 3.31, C++17, убрать sources.cmake-монолит),
перейти SDL2→SDL3 и добавить CI (GitHub Actions) с регрессионной проверкой
детерминизма. Boost НЕ трогаем (отложено).

**Architecture:** Собираем из существующих OBJECT-библиотек единый статический
`blunted2` + игровые библиотеки, переписываем CMakeLists на современный стиль
(`target_*`, `target_sources`), заменяем SDL2-инклюды/API на SDL3, добавляем
workflow на 3 платформы (Linux/Windows сборка+check, macOS сборка).

**Tech Stack:** CMake 3.31, C++17, SDL3 + SDL3_image/ttf/gfx, Boost (остаётся),
OpenAL, SQLite3, GitHub Actions.

## Global Constraints

- **Геймплей не трогаем**: ни одна константа `src/gamedefines.hpp` не меняется,
  логика `Match::Process` не переписывается.
- **Эталон детерминизма обязателен** после каждой задачи: `determinism_runner check
  fa19b1875faeb951bf51c4018bed1c27fd27aa82` → код 0 (Windows). На Linux эталон
  может разойтись — это отдельная задача (см. Task 6).
- **Boost НЕ сокращаем** (отложено): `boost::shared_ptr`, `boost::intrusive_ptr`,
  `boost::signals2`, `boost::thread` и т.д. остаются как есть.
- `src/defines.hpp` содержит `EnvState` и `randomize(seed)` (из ворот №1) — не ломать.
- Проверка сборки — из каталога сборки с копией `data/` (запуск относительный).
- Ветка `build-modernization` от `determinism-gate`.

---

## Файловая структура

| Файл | Роль |
|---|---|
| `CMakeLists.txt` | **переписать**: современный CMake, C++17, `target_*`, объединить таргеты. |
| `sources.cmake` | **удалить**: списки переезжают в CMakeLists как `target_sources`. |
| `CMakeModules/*` | **проверить**: FindSDL2→FindSDL3, FindPackageHandleStandardArgs. |
| `src/**/*.cpp|hpp` (15 файлов с SDL2-инклюдами + процедурный рендер) | **изменить**: SDL2→SDL3. |
| `.github/workflows/build.yml` | **создать**: CI на 3 платформы. |
| `docs/wiki/архитектура.md` | **обновить**: SDL3, C++17, CI. |
| `docs/wiki/константы.md` | **проверить**: ключи конфига `graphics3d_renderer` и т.п. не изменились. |
| `tools/determinism/reference.txt` | **не менять** (эталон). |

---

### Task 1: Современный CMake + C++17 (без смены таргетов)

**Files:**
- Modify: `CMakeLists.txt`, `sources.cmake`

**Interfaces:**
- Consumes: существующие списки `*_SOURCES`/`*_HEADERS` из `sources.cmake`.
- Produces: тот же набор таргетов, но с `cmake_minimum_required(3.16)`,
  `CMAKE_CXX_STANDARD 17`, `target_*` вместо глобальных `include_directories`.

**Почему:** первый безопасный шаг — поднять CMake и стандарт, не меняя структуру.
После него проект должен собираться так же, но с C++17.

- [ ] **Step 1: Поднять версию CMake и C++17**

В `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
...
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
```

- [ ] **Step 2: Проверить сборку**

Run (Windows): `cmake --build . --parallel --config Release`
Expected: сборка успешна; игра и `determinism_runner` работают.

- [ ] **Step 3: Проверить эталон**

Run: `.\Release\determinism_runner.exe check fa19b1875faeb951bf51c4018bed1c27fd27aa82`
Expected: `OK fa19b187...`, код 0.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: bump CMake to 3.16 and C++17"
```

---

### Task 2: Убрать sources.cmake-монолит (объединить таргеты)

**Files:**
- Modify: `CMakeLists.txt`
- Delete: `sources.cmake`

**Interfaces:**
- Consumes: списки из `sources.cmake` (переносятся внутрь CMakeLists).
- Produces: единый статический `blunted2` (объединение всех `*lib` OBJECT-таргетов),
  игровые `gamelib`/`menulib`/`hidlib`/`datalib`/`leaguelib`/`gamecontextlib`,
  исполняемые `gameplayfootball` + `determinism_runner`.

**Почему:** 12 OBJECT-библиотек — артефакт старого стиля; объединяем в один
статический `blunted2`, чтобы CMakeLists стал читаемым.

- [ ] **Step 1: Перенести списки `sources.cmake` в CMakeLists**

Заменить `include(sources.cmake)` на прямые `set(SOURCE_... ...)` списки (перенести
содержимое), или, если объём велик, оставить `sources.cmake` как `set()`-файл, но
перейти с 12 OBJECT-таргетов на 1 статический:

```cmake
add_library(blunted2 STATIC ${BASE_SOURCES} ${BASE_HEADERS} ${BASE_GEOMETRY_HEADERS}
        ${BASE_MATH_HEADERS} ${SYSTEMS_COMMON_SOURCES} ${SYSTEMS_COMMON_HEADERS}
        ${SYSTEMS_GRAPHICS_SOURCES} ${SYSTEMS_GRAPHICS_HEADERS} ... все списки ...)
target_include_directories(blunted2 PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(blunted2 PUBLIC Boost::... SDL3::SDL3 ...)
```

(Заменить `set(OWN_LIBRARIES $<TARGET_OBJECTS:...>...)` и `add_library(blunted2 ... ${OWN_LIBRARIES})`
на единый статический таргет с прямыми списками исходников.)

- [ ] **Step 2: Удалить `sources.cmake`**

```bash
git rm sources.cmake
```

(если списки перенесены внутрь CMakeLists; иначе оставить файл как `set()`-библиотеку —
выбор описан в Step 1.)

- [ ] **Step 3: Проверить сборку + эталон**

Run: `cmake --build . --parallel --config Release`; `determinism_runner check fa19b187...`
Expected: сборка успешна, `check` → 0.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git rm sources.cmake
git commit -m "build: merge object libraries into single blunted2 static lib"
```

---

### Task 3: SDL3 — инклюды

**Files:**
- Modify: 15 файлов с `#include <SDL2/...>` + `src/systems/graphics/rendering/opengl_renderer3d.cpp`
  и `.hpp`.

**Interfaces:**
- Consumes: SDL3 в vcpkg (`sdl3` порт существует).
- Produces: все инклюды `SDL2/...` → `SDL3/...`.

**Почему:** SDL3 меняет пути инклюдов и часть API. Начинаем с путей.

- [ ] **Step 1: Заменить инклюды в 15 файлах**

Механическая замена во всех файлах:
- `#include <SDL2/SDL.h>` → `#include <SDL3/SDL.h>`
- `#include <SDL2/SDL_ttf.h>` → `#include <SDL3/SDL_ttf.h>`
- `#include <SDL2/SDL_opengl_glext.h>` → `#include <SDL3/SDL_opengl.h>` (проверить имя заголовка в SDL3)
- `#include <SDL2/SDL_syswm.h>` → `#include <SDL3/SDL_syswm.h>`
- `#include <SDL2/SDL2_rotozoom.h>` → `#include <SDL3/SDL_rotozoom.h>` (из SDL3_gfx; проверить имя)

Файлы: `src/hid/keyboard.hpp`, `src/managers/environmentmanager.cpp`,
`src/managers/usereventmanager.hpp`, `src/menu/ingame/playerhud.cpp`, `radar.cpp`,
`scoreboard.cpp`, `tacticsdebug.cpp`, `src/scene/objects/image2d.hpp`,
`src/systems/graphics/rendering/opengl_renderer3d.{cpp,hpp}`,
`src/utils/gui2/events.hpp`, `guitask.hpp`, `src/utils/text2d.hpp`, `threadhud.hpp`,
`src/gamedefines.hpp`.

- [ ] **Step 2: Обновить FindSDL2→FindSDL3 в CMake**

В `CMakeLists.txt`:
```cmake
FIND_PACKAGE(SDL3 REQUIRED)
FIND_PACKAGE(SDL3_image REQUIRED)
FIND_PACKAGE(SDL3_ttf REQUIRED)
FIND_PACKAGE(SDL3_gfx REQUIRED)
```
и заменить `${SDL2_*}` на `${SDL3_*}` в `target_link_libraries`.

- [ ] **Step 3: Собрать — ожидаем ошибки API**

Run: `cmake --build . --parallel --config Release`
Expected: сборка падает на SDL-API-несовместимостях (это нормально, их чиним в Task 4).
Фиксируем список ошибок.

- [ ] **Step 4: Commit (инклюды, даже если сборка ещё красная)**

```bash
git add CMakeLists.txt src/
git commit -m "build: switch SDL2 includes to SDL3"
```

---

### Task 4: SDL3 — API (по ошибкам компиляции)

**Files:**
- Modify: файлы, где SDL-API изменился (из списка ошибок Task 3 Step 3).

**Interfaces:**
- Consumes: список ошибок компиляции из Task 3.
- Produces: компилирующийся проект на SDL3.

**Почему:** SDL3 изменил часть API: `SDL_CreateRGBSurface` (без `SDL_SWSURFACE`/
`SDL_RLEACCEL`), `SDL_Init` возвращает bool, `SDL_NumJoysticks`, и др. Чиним по
фактическим ошибкам.

- [ ] **Step 1: Пройтись по ошибкам компиляции**

Для каждой ошибки из Task 3 Step 3 исправить вызов. Типовые правки SDL2→SDL3:

- `SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_RLEACCEL, w, h, 32, r, g, b, a)` →
  `SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32)` (проверить сигнатуру SDL3;
  иначе убрать флаги `SDL_SWSURFACE|SDL_RLEACCEL`).
- `SDL_Init(SDL_INIT_VIDEO)` → `SDL_InitSubSystem(SDL_INIT_VIDEO)` или `SDL_Init(0)` +
  подсистема (проверить).
- `SDL_NumJoysticks()` → сигнатура может отличаться (проверить).
- `SDL_MapRGB`/`SDL_MapRGBA`/`SDL_ISPIXELFORMAT_ALPHA` — проверить доступность в SDL3
  (могут требовать `SDL_pixels.h`).

- [ ] **Step 2: Собрать до зелёного**

Run: `cmake --build . --parallel --config Release`
Expected: сборка успешна.

- [ ] **Step 3: Проверить игру и эталон**

Run: запуск `gameplayfootball.exe` (визуально окно + матч), затем
`determinism_runner check fa19b187...`
Expected: игра работает, `check` → 0.

- [ ] **Step 4: Commit**

```bash
git add src/
git commit -m "build: fix SDL3 API changes"
```

---

### Task 5: CI — GitHub Actions (3 платформы)

**Files:**
- Create: `.github/workflows/build.yml`

**Interfaces:**
- Consumes: `determinism_runner` (собирается из `tools/determinism/main.cpp`),
  эталон `tools/determinism/reference.txt`.
- Produces: workflow на push/PR: Linux (сборка + check), Windows (сборка + check),
  macOS (сборка, без check).

**Почему:** автоматизировать регрессионную проверку эталона и подтвердить
переносимость сборки.

- [ ] **Step 1: Написать `.github/workflows/build.yml`**

```yaml
name: build

on: [push, pull_request]

jobs:
  linux:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: sudo apt-get update && sudo apt-get install -y cmake build-essential
          libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev libsdl3-gfx-dev
          libboost-all-dev libopenal-dev libsqlite3-dev libgl1-mesa-dev
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build --parallel
      - name: Prepare data
        run: cp -R data/. build/
      - name: Determinism check
        working-directory: build
        run: ./determinism_runner check $(cat ../tools/determinism/reference.txt)

  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install vcpkg deps
        run: |
          vcpkg install sdl3 sdl3-image[libjpeg-turbo] sdl3-ttf sdl3-gfx
            boost openal-soft sqlite3 --triplet x64-windows
      - name: Configure
        run: cmake -B build -DCMAKE_GENERATOR_PLATFORM=x64
          -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake"
      - name: Build
        run: cmake --build build --config Release --parallel
      - name: Prepare data
        run: xcopy /e /i data build\Release
      - name: Determinism check
        working-directory: build/Release
        run: .\determinism_runner.exe check (Get-Content ..\..\tools\determinism\reference.txt)

  macos:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: brew install cmake sdl3 sdl3_image sdl3_ttf sdl3_gfx boost openal-soft
      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build --parallel
```

*Примечание:* названия пакетов (sdl3, sdl3-image, sdl3-gfx) и сигнатуры команд
сверяются с фактическими пакетами на момент реализации; имена могут отличаться
(`libsdl3-dev` vs `sdl3`). Эталон `check` на Linux/Windows может не совпасть с
`fa19b187...` — это ожидаемо и чинится в Task 6.

- [ ] **Step 2: Запушить ветку и убедиться, что workflow запускается**

Run: `git push -u origin build-modernization`
Expected: GitHub Actions запускает 3 job; смотрим статус в Actions.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/build.yml
git commit -m "ci: add 3-platform build workflow with determinism check"
```

---

### Task 6: Переносимость эталона (если CI красный из-за check)

**Files:**
- Modify: `tools/determinism/reference.txt` (или CI-логика)

**Interfaces:**
- Consumes: результат CI из Task 5.
- Produces: решение по эталону на разных платформах.

**Почему:** эталон `fa19b187...` сгенерирован на Windows/MSVC. На Linux/Windows-x64
он может разойтись из-за float/выравнивания. Нужно определить: эталон один на все
платформы или свой на платформу.

- [ ] **Step 1: Сравнить хэши на платформах**

Из логов CI: собрать хэши `determinism_runner run` на Linux, Windows, (macOS если
раннер запустился). Если все три совпадают с `fa19b187...` — эталон переносим, Task 6
закрыт. Если нет:

- [ ] **Step 2: Зафиксировать решение по эталону**

Варианты:
- (а) эталон один, но недетерминизм на платформе — ищем источник (float, RNG);
- (б) эталон на платформу: `reference-windows.txt`, `reference-linux.txt`, CI
  сравнивает со своим файлом.

Выбор фиксируется в `docs/wiki/открытые-вопросы.md`.

- [ ] **Step 3: Commit**

```bash
git add tools/determinism/ .github/workflows/build.yml docs/wiki/
git commit -m "ci: reconcile determinism reference across platforms"
```

---

### Task 7: Документация

**Files:**
- Modify: `docs/wiki/архитектура.md`, `docs/wiki/константы.md`, `docs/plans/` новый план

**Interfaces:**
- Consumes: результаты Tasks 1-6.
- Produces: актуальная вики.

- [ ] **Step 1: Обновить `docs/wiki/архитектура.md`**

- Стек: C++17, CMake 3.16+, SDL3/SDL3_image/ttf/gfx.
- CI: GitHub Actions, 3 платформы.
- Boost остаётся (отложено сокращение).

- [ ] **Step 2: Проверить `docs/wiki/константы.md`**

Ключи конфига не изменились; если SDL3 поменял дефолты (`context_x` и т.п.) —
поправить.

- [ ] **Step 3: Запись в `log.md`**

```
## [2026-08-10] feat | Модернизация сборки: CMake 3.16+/C++17, SDL2→SDL3, CI
```

- [ ] **Step 4: Commit**

```bash
git add docs/wiki/ log.md
git commit -m "docs: update wiki for build modernization"
```

---

## Self-Review

**1. Spec coverage:**
- CMake современный + C++17 → Task 1.
- sources.cmake-монолит → Task 2.
- SDL2→SDL3 → Tasks 3, 4.
- CI (Linux check + Windows check + macOS сборка) → Task 5.
- Переносимость эталона → Task 6.
- Вики/лог → Task 7.

**2. Placeholder scan:** все шаги содержат конкретные действия. Task 4 намеренно
открыт по факту ошибок компиляции (SDL3 API известен только после первой сборки) —
это не «сделать позже», а итеративная механика; типовые правки приведены.

**3. Type consistency:** SDL3-имена пакетов/инклюдов сверяются на каждом шаге с
фактическим SDL3 (версия в vcpkg может отличаться от документации); заложена
сверка в Steps.

**Открытые риски:**
- SDL3 API изменился сильнее, чем инклюды — Task 4 может занять несколько итераций.
- Названия пакетов SDL3 в apt/brew/vcpkg могут отличаться от приведённых — сверяется
  в Task 5 Step 1.
- Эталон может не совпасть на Linux — Task 6.
