# log.md — хронология проекта

Append-only журнал по методу Карпатого (LLM Wiki): **что произошло и когда** — вехи, деплои,
опровергнутые гипотезы, решения. Дополняет вики: `docs/wiki/` отвечает на «как устроено сейчас»,
лог — на «как мы сюда пришли». Записи только добавляются в конец, никогда не редактируются и не
удаляются (опечатки — новой записью-поправкой).

Формат записи (парсится unix-утилитами, `grep "^## \[" log.md | tail -5` — последние 5 событий):

```
## [YYYY-MM-DD] тип | краткое описание
```

Типы: `feat` (новая механика), `fix`, `deploy` (выкатка), `docs`, `decision` (выбор направления,
отказ от идеи), `lint` (сверка вики/констант), `session` (итог рабочей сессии, если не покрыт
другими типами).

Лог **заменяет `handoff.md`**: при завершении сессии — запись `session` здесь плюс обновление
`docs/wiki/открытые-вопросы.md`.

---

## [2020-07-17] docs | Начата модернизация: Blunted2 в репозитории, единая сборка
Движок Blunted2 добавлен в репозиторий (`af00e69`), источники сведены в одну сборку (`081d919`).
Позже (2020-07-21) переезд на SDL2, удаление SGE/Glew; добавлены дефолтные данные.

## [2020-07-31] decision | macOS: цель — компилироваться, но не запускаться
Рендеринг обязан идти в main thread, а рендерер стартует в отдельном потоке — macOS собирается,
но не работает. Направление зафиксировано, решение не найдено (см. `docs/wiki/открытые-вопросы`).

## [2020-11-06] docs | Инструкции для Windows и macOS
README дополнен сборкой для Windows (vcpkg, триплеты `x86-windows`) и macOS (brew).

## [2021-04-21] feat | SDL2: получение доступных display modes
`80ef590` — запрос доступных режимов дисплея через SDL2 вместо жёстких дефолтов.

## [2021-07-20] fix | Сегфолт на выходе
`1415a33` — исправлен сегфолт при завершении игры (защита границ Get/Process/Put мьютексами).
Правило из ловушек AGENTS.md: не возвращать.

## [2021-07-20] decision | SQLite из системного пакета, а не из репозитория
`68159a2` — вендорные sqlite3-исходники удалены, сборка использует системный пакет SQLite3.

## [2026-08-09] docs | Развёрнут контур документации
AGENTS.md, `docs/wiki/` (архитектура, матч, база-данных, константы, открытые вопросы, глоссарий),
этот лог, корневой `CONTEXT.md`-указатель, хуки поддержки вики (`.agent/hooks/`, opencode-плагин)
и скилл `wiki-lint`. Затравка лога восстановлена из git-истории (вехи 2020-2021 по коммитам).

## [2026-08-09] deploy | Windows-сборка подтверждена end-to-end (MSVC + vcpkg)
`gameplayfootball.exe` собран на master в VS2022 Build Tools (MSVC 14.44, платформа Win32),
зависимости vcpkg `x86-windows`, системный SQLite3. Запуск проверен — процесс жив и рендерит.
В master из ветки `windows` перенесены точечные MSVC-фиксы: `NOMINMAX` в `defines.hpp`/`main.cpp`,
`<SDL2/SDL_opengl_glext.h>` вместо `wingdi.h` в рендерере, замена VLA на `std::vector`
(`aseloader.cpp`, `grid.cpp`, `humanoid.cpp`, `match.cpp`, `proceduralpitch.cpp`,
`teamAIcontroller.cpp`, `animcollection.cpp`, `opengl_renderer3d.cpp`), `Stat{...}` вместо
`(Stat){...}` в `utils.cpp`, сигнатура `main(int, char**)`, условное `dl`/`m` только под UNIX
в `CMakeLists.txt`. Ветку `windows` целиком не вливаем (проверено её содержимое: устаревший
вендорный sqlite3 и лишние правки).

## [2026-08-10] feat | Ворота №1: детерминированный headless-раннер (ветка `determinism-gate`)
Перенесены из GRF (ветка `google-brain` = GRF v2.10.1) только архитектурные механизмы, без
изменения геймплея: `EnvState`-сериализатор (`defines.hpp`), отвязка времени в `Match::Process`
(фиксированный шаг 10 мс вместо реальных часов), `randomize(seed)` (srand + boost + fastrandom),
`ProcessState` для ядра Match/Ball/Player/PlayerBase (только совпадающие поля, без
AI/humanoid-внутренностей). Вынесен игровой контекст из `main.cpp` в `src/gamecontext.*`.
Добавлен `tools/determinism` (режимы `run`/`check`) с эталоном `reference.txt`, `MockRenderer3D`
для headless-рендера (`graphics3d_renderer=mock`). При отладке найден и исправлен баг
неинициализированной памяти: `tacticalSituation.forwardRating` и `dynamicFormationEntry`
запасных игроков не инициализировались в ctor — это и был источник недетерминизма.
Оценка геймплея GRF: играется похоже, но Google меняли ощущения (автопилот, переключение,
пас) — игровую логику GRF в master не переносим.

## [2026-08-10] feat | Модернизация сборки: CMake 3.16+/C++17, SDL2→SDL3, CI (ветка `build-modernization`)
CMake поднят до 3.16, включён C++17 (`c3bdb61`); источники сведены в единый статический `blunted2`,
`sources.cmake` удалён (`858f09b`); SDL2→SDL3 в инклюдах и API, SDL_gfx убран полностью
(в vcpkg порта sdl3-gfx нет; заменён на `SDL_ScaleSurface`) (`e138cff`). Эталон детерминизма сделан
воспроизводимым между пересборками (`fb3a90f`): `determinism_runner` в начале фиксирует ключи конфига
(`graphics3d_renderer=mock`, `match_difficulty=0.8`, `match_duration=1.0`). Добавлен CI
`.github/workflows/build.yml` (4 job: linux сборка+check, windows-x86 check, windows-x64 capture,
macos сборка) и эталоны на платформу: x86 `reference.txt`=`372c4bbd...`, x64
`reference-windows-x64.txt`=`4e9ddb72...` (снят локально; отличается от x86 — эталон платформозависим),
`reference-linux.txt` снимается первым CI-прогоном (TODO, Task 6). Linux-job привязан к `ubuntu-26.04`
(apt-пакеты SDL3 есть только с 26.04; `ubuntu-latest` всё ещё 24.04). Обновлены вики, AGENTS.md,
README. Первый прогон CI — после push (Task 6).
