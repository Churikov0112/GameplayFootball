# Render Modernization Implementation Plan (ворота №3)

> **Для агентных исполнителей:** РЕКОМЕНДУЕТСЯ РЕАЛИЗОВАТЬ ЧЕРЕЗ `executing-plans`.
> Каждая задача заканчивается сборкой и `determinism_runner check`
> (эталон Windows x86 = `372c4bbdf78e294a05dbbc9176f86741054f46fb`).

**Goal:** Перевести рендерер `OpenGLRenderer3D` с legacy OpenGL (контекст без
профиля, fixed-function) на modern core profile 3.2. Вся геометрия и оверлеи уже
рисуются через VAO/VBO + шейдеры; остаётся (а) включить core profile при создании
контекста, (б) убрать все legacy-вызовы fixed-function, (в) почистить лоадер
GL-функций, чтобы он не запрашивал функции, отсутствующие в core profile.

**Architecture:** Деferred-рендер (GBuffer → accumulation → postprocess) остаётся
как есть. Свет — уже шейдерный (`lighting`/`ambient`), не fixed-function. Единственные
АКТИВНЫЕ legacy-вызовы сидят в мёртвых методах интерфейса `Renderer3D`
(`SetColor`→`glColor4f`, `SetClientTextureUnit`→`glClientActiveTexture`,
`PushAttribute`/`PopAttribute`→`glPushAttrib`/`glPopAttrib`); всё остальное
(`glBegin`/`glEnd`/`glLightfv`/матричный стек) — уже в закомментированных телах.
План: удалить мёртвые legacy-методы из интерфейса и обеих реализаций, включить
core profile, пометить deprecated-функции в `sdl_glfuncs.h` как `SDL_PROC_UNUSED`
(иначе `SDL_GL_GetProcAddress` вернёт NULL под core profile и лоадер сделает
`exit(1)`).

**Tech Stack:** OpenGL 3.2 core, GLSL `#version 150` (уже такой), SDL3, C++17.

## Global Constraints

- **Геймплей не трогаем**: `src/gamedefines.hpp`, `Match::Process`, AI, мяч —
  без изменений. Рендер не влияет на симуляцию.
- **Эталон детерминизма обязателен** после каждой задачи:
  `determinism_runner check 372c4bbdf78e294a05dbbc9176f86741054f46fb` → код 0
  (Windows x86). Используется `graphics3d_renderer=mock`, так что рендер не в
  пути симуляции — проверка чисто регрессионная.
- **Boost не сокращаем.**
- **Ассеты (PNG/ASE/object) не меняем** — modern-рендер не требует новых ассетов.
- **Визуальный регресс** освещения/цветов возможен — ловится глазами (запуск игры)
  и правится в шейдере, не в константах.
- Шейдеры НЕ переводим на `#version 300 es` в этой ветке: GL 3.2 core принимает
  только `#version 150`, GLES-перевод требует двух наборов шейдеров и вынесен в
  отдельную задачу (см. `docs/wiki/открытые-вопросы`). Этот гейт делает стек
  core-profile-совместимым, что и является предпосылкой GLES.
- Проверка сборки — из каталога сборки с копией `data/`.
- Ветка `render-modernization` от `master`.

---

## Файловая структура

| Файл | Роль |
|---|---|
| `src/systems/graphics/rendering/interface_renderer3d.hpp` | **удалить** мёртвые legacy-методы интерфейса. |
| `src/systems/graphics/rendering/opengl_renderer3d.hpp` | **удалить** те же объявления. |
| `src/systems/graphics/rendering/opengl_renderer3d.cpp` | **удалить** legacy-методы и `drawSphere`; включить core profile в `CreateContext`. |
| `src/systems/graphics/rendering/mock_renderer3d.hpp` | **удалить** те же методы из mock-реализации. |
| `src/systems/graphics/rendering/sdl_glfuncs.h` | **пометить** deprecated-функции как `SDL_PROC_UNUSED`. |
| `docs/wiki/архитектура.md` | **обновить**: core profile 3.2, убранные методы. |
| `docs/wiki/открытые-вопросы.md` | **обновить**: GLES-перевод шейдеров — следующая задача. |
| `docs/plans/2026-08-11-render-modernization.md` | этот план. |

---

### Task 1: Удалить мёртвые legacy-методы из интерфейса и реализаций

**Files:**
- Modify: `src/systems/graphics/rendering/interface_renderer3d.hpp`
- Modify: `src/systems/graphics/rendering/opengl_renderer3d.hpp`
- Modify: `src/systems/graphics/rendering/opengl_renderer3d.cpp`
- Modify: `src/systems/graphics/rendering/mock_renderer3d.hpp`

**Почему:** единственные активные fixed-function вызовы (`glColor4f`,
`glClientActiveTexture`, `glPushAttrib`/`glPopAttrib`) живут в этих методах, а сами
методы нигде не вызываются (проверено grep по `GetRenderer3D()`-потребителям и
всему репо). Удаление убирает legacy-поверхность интерфейса целиком.

Удаляем из `Renderer3D` (и из `OpenGLRenderer3D` + `MockRenderer3D`):
- `SetTextureMode(e_TextureMode)` — тело уже закомментировано;
- `SetColor(Vector3, float)` — `glColor4f`;
- `SetColorMask(bool,bool,bool,bool)` — мёртв (метод и вызовы отсутствуют);
- `RenderAABB(std::list<VertexBufferQueueEntry>&)` — тело уже закомментировано (`glBegin/glEnd`);
- `RenderAABB(std::list<LightQueueEntry>&)` — то же;
- `SetLight(Vector3, Vector3, float)` — тело уже закомментировано (`glLightfv`);
- `SetClientTextureUnit(int)` — `glClientActiveTexture`;
- `PushAttribute(int)` / `PopAttribute()` — `glPushAttrib`/`glPopAttrib`;
- `HDRCaptureOverallBrightness()` / `HDRGetOverallBrightness()` — мёртвы (только
  закомментированные ссылки в `r3d_messages.cpp`).

В `opengl_renderer3d.cpp` также удаляем неиспользуемую функцию `drawSphere`
(целиком в комментарии, `glBegin(GL_QUAD_STRIP)`).

**Verify:** сборка `determinism_runner` (Release) + `check 372c4bbd...` → 0.
Ожидаются ошибки компиляции в `sdl_glfuncs.h`? Нет — удаление методов убирает
ссылки на `glColor4f`/`glClientActiveTexture`/`glPushAttrib`/`glPopAttrib`, но
декларации в `sdl_glfuncs.h` остаются до Task 3.

### Task 2: Включить core profile при создании контекста

**Files:**
- Modify: `src/systems/graphics/rendering/opengl_renderer3d.cpp` (`CreateContext`)

**Почему:** сейчас атрибуты контекста закомментированы — SDL создаёт legacy
(compatibility) контекст. Включаем OpenGL 3.2 core:

```cpp
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
```

Проверка версии в `CreateContext` (`glVersion >= 3.2`) уже есть — остаётся как есть.

**Verify:** сборка `determinism_runner` (Release) + `check` → 0. Дополнительно —
запуск `gameplayfootball` из `build/` на ~5 сек: в `log.txt` должно появиться
`Using OpenGL version 4.x`/`3.x` и отсутствовать `FatalError`. Это требует окна на
машине; финальная визуальная проверка — Task 5.

### Task 3: Почистить лоадер `sdl_glfuncs.h`

**Files:**
- Modify: `src/systems/graphics/rendering/sdl_glfuncs.h`

**Почему:** под core profile `SDL_GL_GetProcAddress` для удалённых из GL функций
возвращает NULL, а макрос `SDL_PROC` в `CreateContext` при NULL делает `exit(1)`.
Функции, удалённые в core profile, помечаем `SDL_PROC_UNUSED` (не запрашиваются,
структурные поля не создаются). Оставляем `SDL_PROC` только для core-совместимых.

Пометить как `SDL_PROC_UNUSED` (removed in core 3.2):
`glBegin`, `glColor3f`, `glColor4f`, `glColorMaterial`, `glClientActiveTexture`,
`glEnd`, `glLightfv`, `glLoadIdentity`, `glLoadMatrixf`, `glMatrixMode`,
`glMaterialfv`, `glMateriali`, `glNormal3f`, `glOrtho`, `glPopAttrib`,
`glPopClientAttrib`, `glPopMatrix`, `glPushAttrib`, `glPushClientAttrib`,
`glPushMatrix`, `glShadeModel`, `glTexCoord2f`, `glTexCoord3f`, `glTexEnvf`,
`glTranslatef`, `glVertex2f`, `glVertex2i`, `glVertex3f`.

Остаются `SDL_PROC` (core-совместимы): `glClearDepth`, `glColorMask`,
`glGetFloatv`, `glPixelStorei`, `glPolygonMode`, `glReadPixels`, `glFlush`,
`glGetError` и весь современный набор (VAO/VBO/FBO/шейдеры/текстуры).

**Verify:** сборка `determinism_runner` (Release) + `check` → 0. Сборка
`gameplayfootball` (Release). Компиляция ловит любую ссылку на убранный
`mapping.glX`.

### Task 4: Полная сборка + регрессия

**Files:** нет правок.

**Verify:**
- `cmake --build build --config Release` (все таргеты: `gameplayfootball`,
  `determinism_runner`) → без ошибок.
- `determinism_runner check 372c4bbdf78e294a05dbbc9176f86741054f46fb` → 0.
- `rg "glBegin|glEnd|glLightfv|glMatrixMode|glLoadIdentity|glOrtho|glTranslatef|
  glColor4f|glTexCoord2f|GL_LIGHT0|GL_TEXTURE_2D"` по `src/systems/graphics` →
  только комментарии/имена-строки, не активные вызовы (оформить проверку вручную).

### Task 5: Визуальная проверка игры (core profile)

**Files:** нет правок.

**Verify:** запустить `build/Release/gameplayfootball.exe` из `build/`; в `log.txt`:
`Using OpenGL version <3.2+>` и `major/minor 3.2+`, без `FatalError` и шейдерных
ошибок. Пользователь глазами подтверждает картинку (освещение/цвета).

### Task 6: Документация и хронология

**Files:**
- Modify: `docs/wiki/архитектура.md` — рендер: core profile 3.2, убранные
  legacy-методы, слой совместимости не используется.
- Modify: `docs/wiki/открытые-вопросы.md` — добавить «GLES-перевод шейдеров
  (`#version 300 es`, две ветки шейдеров) — следующая задача после core profile».
- Modify: `log.md` — запись `feat`/`session` по итогу ворот №3.

**Verify:** `grep "^## \[" log.md | tail -3`; страницы вики ссылаются корректно.

---

## Риски

- **Визуальный регресс** освещения/цветов при переключении на core profile.
  Митигируется тем, что свет уже шейдерный и шейдеры не меняются; проверка — Task 5.
- **Контекст 3.2 core недоступен** на очень старом GPU/драйвере. Текущая проверка
  в `CreateContext` уже предупреждает об этом; поведение то же, что и при
  legacy-контексте ниже 3.2 (лог `Warning`).
- **Скрытая зависимость от fixed-function** в другом месте движка — ловится
  компиляцией после Task 3 (убраны `mapping.glX`) и `rg`-проверкой.
