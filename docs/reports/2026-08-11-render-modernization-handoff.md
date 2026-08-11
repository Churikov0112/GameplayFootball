# Handoff: ворота №3 — модернизированный рендер (modern OpenGL) в новой ветке

Дата: 2026-08-11. Тип: handoff (одноразовый пакет для продолжения в свежей
сессии). Датированный снимок — не редактировать; актуальная картина — в вики.

Этот файл — промпт для новой сессии: скопируй его целиком в начало разговора.

---

```markdown
# Новая сессия: модернизированный рендер (modern OpenGL) в новой ветке

Ты работаешь в репозитории GameplayFootball (3D-футбол на C++17/CMake 3.16+,
форк заброшенного GameplayFootball, движок Blunted2 лежит в src/). ВАЖНО: сначала
прочитай `AGENTS.md` (правила проекта и ловушки) и `docs/wiki/index.md` (актуальная
картина проекта). Общение на русском, код/комментарии на английском.

## Контекст: что уже сделано (всё влито в master)

1. Ворота №1 — детерминизм симуляции + регрессионный эталон:
   - `Match::Process` отвязан от реального времени (шаг 10 мс), `randomize(seed)`.
   - `EnvState`-сериализация + `Match::ProcessState`/`Ball`/`Player`/`PlayerBase`
     (только совпадающие поля, без AI-внутренностей).
   - `tools/determinism` — headless-раннер `run`/`check`. Эталоны:
     `tools/determinism/reference.txt` (Windows x86 = `372c4bbd...`),
     `reference-windows-x64.txt`, `reference-linux.txt`.
   - `MockRenderer3D` для headless (ключ конфига `graphics3d_renderer=mock`).
2. Ворота №2 — модернизация сборки:
   - CMake 3.16+ / C++17, `sources.cmake` удалён, единый статический `blunted2`.
   - SDL2 → SDL3 (SDL3_image/ttf, SDL_gfx убран).
   - Boost НЕ сокращён (отложено): `boost::shared_ptr`, `intrusive_ptr`,
     `signals2`, `thread` остаются.
   - Портируемость на Linux (GCC) подтверждена, эталон Linux записан.
   - macOS: собирается, но НЕ запускается (рендер не в main thread) — зафиксировано.

Спека детерминизма: `docs/specs/2026-08-09-determinism-gate-design.md`.
План сборки (выполнен): `docs/plans/2026-08-10-build-modernization.md`.
Хронология: `log.md` (append-only).

## Твоя задача: ворота №3 — Modern-рендер (legacy GL → core/GLES)

Отпочкуй новую ветку от master (например `render-modernization`) и спроектируй +
реализуй перевод рендерера с legacy OpenGL на modern core profile. Цель — сделать
стек рендера совместимым с OpenGL ES (обязательно для будущих мобильных) и
современным GL. НЕ менять геймплей: симуляция живёт в `src/onthepitch` и не
зависит от рендера, эталон детерминизма должен оставаться зелёным.

## Текущее состояние рендерера (проверено по коду, гибрид)

Файл: `src/systems/graphics/rendering/opengl_renderer3d.cpp` (+ `.hpp`,
`sdl_glfuncs.h` — свой лоадер GL-функций, остаётся).

Уже modern (не трогаем): меши через VAO/VBO + шейдеры
(`CreateSimpleVertexBuffer`, `glVertexAttribPointer`, компиляция программ),
2D-overlay на буферах, FBO/рендер-таргеты, текстуры/mipmap.

Legacy fixed-function (объём работы, ~63 вызова):
- `glBegin(GL_QUAD_STRIP)` — билборды/спрайты (строка ~166);
- `glBegin(GL_LINES)` ×2 — AABB-отладка (строки ~1320, ~1377) — большой блок;
- `glLightfv(GL_LIGHT0, GL_POSITION/DIFFUSE/SPECULAR)` + `glEnable(GL_LIGHT0)` —
  весь свет в игре (строки ~1439-1448);
- матричный стек: `glMatrixMode`/`glLoadIdentity`/`glPushMatrix`/`glPopMatrix`/
  `glOrtho`/`glTranslatef`;
- `glColor4f`, `glTexCoord2f`, `glEnable/glDisable(GL_TEXTURE_2D)`;
- контекст без core-profile: `SDL_GL_CONTEXT_MAJOR_VERSION`/
  `SDL_GL_CONTEXT_PROFILE_CORE` закомментированы (строки ~365-372).

## Ограничения

- НЕ менять константы `src/gamedefines.hpp`, логику `Match::Process`, AI, мяч.
- Boost не сокращать.
- Проверка после каждого шага: сборка + `tools/determinism check <эталон для платформы>`.
- Ассеты (PNG/ASE/object) НЕ менять — modern-рендер не требует новых ассетов.
- Риск: визуальный регресс освещения/цветов (fixed-function свет → шейдер) —
  ловится глазами, правится в шейдере.

## Процесс

1. Прочитай AGENTS.md, вики (`docs/wiki/архитектура.md`, `матч.md`),
   глоссарий (`docs/wiki/глоссарий.md`), `log.md`.
2. Используй скилл `brainstorming` для дизайна (вопросы по одному, предложи
   подходы, секции дизайна). Затем `writing-plans` → план в `docs/plans/`.
3. Реализуй по плану (executing-plans). Каждый этап: компиляция + check эталона.
4. Обнови вики (`docs/wiki/архитектура.md` и связанные) и допиши `log.md`.
5. Работай в новой ветке, НЕ в master, не вливая в него без явного запроса.
```
