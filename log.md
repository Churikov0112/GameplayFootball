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

## [2026-08-10] session | CI на GitHub Actions: Windows подтверждён, linux/macos отложены
Ветка `build-modernization` запушена, workflow запущен. Windows x86+x64 подтверждены end-to-end:
сборка на VS2026 + новый boost, determinism check/capture дают ровно локальные эталоны
(x86 `372c4bbd...`, x64 `4e9ddb72...`) — оба портативны между MSVC 2022 и 2026. По пути починены
три реальных бага: (1) дистрибутивный Boost (Ubuntu/Homebrew, b2) не ставит компонентные
CMake-конфиги и в 1.90+ не собирает `libboost_system` (header-only с 1.69) — в CMakeLists
`Boost_NO_BOOST_CMAKE=ON`, компоненты `thread filesystem`; (2) `MockAudioRenderer`
(`audio_renderer=mock`, `src/systems/audio/rendering/mock_audiorenderer.hpp`) — на CI-раннерах нет
аудио-устройства, `OpenALRenderer::CreateContext` падал фатально; (3) баг кавычек в PowerShell-шаге
capture (`$refFile = ..\..\...` без кавычек → `$null`). Linux/macos в CI нестабильны:
preview-раннер `ubuntu-26.04` гасится посреди сборки, контейнер `ubuntu:26.04` висит на teardown,
сборка SDL3 из исходников упёрлась в нехватку `libasound2-dev`, macOS build-only висит часами.
Linux/macos вынесены из CI, проверка переносится на локальные девайсы (см.
`docs/wiki/открытые-вопросы.md`). `reference-linux.txt` — TODO (снять на Linux-устройстве).
Малый boost-набор в vcpkg (component-порты) ускорил Windows-джобы с ~57 до ~10-15 мин.

## [2026-08-10] decision | GitHub Actions CI убран, проверка детерминизма — вручную
Windows x86+x64 собрались и подтвердили оба эталона на CI (VS2026: x86 `372c4bbd...`, x64
`4e9ddb72...`), но linux/macos-джобы оказались нестабильны: preview-раннер `ubuntu-26.04` гасится
посреди сборки, контейнер `ubuntu:26.04` зависает на teardown, сборка SDL3 из исходников упёрлась
в нехватку `libasound2-dev`, macOS build-only висит часами. Решение: `.github/workflows` удалён,
проверка детерминизма — вручную через `tools/determinism` на локальных машинах (у автора есть
Windows/Linux/Mac-девайсы). Эталоны остаются локальным инструментом; `reference-linux.txt` — TODO
(снять на Linux-девайсе). Полезные фиксы из CI-итераций остались в коде: `Boost_NO_BOOST_CMAKE` +
компоненты `thread filesystem` (дистрибутивный boost не даёт компонентных CMake-конфигов и не
собирает `libboost_system` с 1.69), `MockAudioRenderer` (`audio_renderer=mock`) для headless-запуска
без аудио-устройства.

## [2026-08-10] session | Linux (WSL2 Ubuntu 26.04): сборка зелёная, эталон снят, найдены порт-фиксы
На этом ПК поднят WSL2 с Ubuntu 26.04 LTS, ветка `build-modernization` собрана. Всплыли четыре
бага портируемости, которые MSVC не ловил: (1) циклический инклюд `defines.hpp`↔`log.hpp` скрывал
`blunted::Log` при включении `log.hpp` первым (gcc, two-phase lookup) — в `defines.hpp` добавлен
хелпер `blunted::EnvStateFatal()`, определён в `envstate.cpp`; (2) `settings.cpp` держал SDL2-код
перечисления display modes в не-Windows ветке (`SDL_GetNumDisplayModes`/`SDL_GetDisplayMode` убраны
из SDL3) — переведено на `SDL_GetDisplays` + `SDL_GetDesktopDisplayMode`/`SDL_GetCurrentDisplayMode`;
(3) `SDL_GL_GetProcAddress` в SDL3 возвращает `SDL_FunctionPointer`, а не `void*` — добавлен
`reinterpret_cast<void*>` в макрос `SDL_PROC` (`opengl_renderer3d.cpp`); (4) статические игровые
библиотеки с циклическими ссылками не линкуются на GNU ld — в CMakeLists добавлен
`-Wl,--start-group/-end-group` (UNIX AND NOT APPLE). После фиксов: Linux-сборка зелёная, игра
запускается в WSLg и корректно завершается, linux-эталон `reference-linux.txt` =
`a672aa0b81d6275e60a6469b1c41dfdf9acff138` (воспроизводим, check → 0). Windows-хэши не изменились
(x86 `372c4bbd...`, x64 `4e9ddb72...`).

## [2026-08-10] session | macOS-девайс: сборка собралась, игра не завелась; ветка влита в master
На MacBook Air M2 (arm64, 8 ГБ) ветка `build-modernization` собралась после порт-фиксов
(brew sdl3/sdl3_image/sdl3_ttf/boost/openal-soft; сборка без параллелизма — 8 ГБ RAM). Запуск
подтвердил известный блокер: окно/GL-контекст создаются в отдельном потоке
(`GraphicsSystem::Initialize` → `OpenGLRenderer3D::Run`), а AppKit требует main thread → игра не
стартует. Фикс (создание окна/контекста в main thread) отложен; статус перенесён в
«Отложено осознанно» в вики. Детерминизм на macOS не снимался (игра не работает, эталон не нужен).
Порт-фиксы, найденные на Linux/WSL и подтвердившиеся на macOS: `EnvStateFatal()` вместо
`blunted::Log` в шаблоне (циклический инклюд defines↔log), SDL3 display-mode API в `settings.cpp`,
`reinterpret_cast<void*>` для `SDL_GL_GetProcAddress`, `-Wl,--start-group` для GNU ld.
Ветка `build-modernization` влита в `master`.

## [2026-08-11] feat | Ворота №3: рендер на OpenGL 3.2 core profile (ветка `render-modernization`)
Рендерер `OpenGLRenderer3D` переведён с legacy (compatibility) контекста на **core profile 3.2**:
в `CreateContext` включены `SDL_GL_CONTEXT_MAJOR_VERSION=3`, `MINOR=2`,
`SDL_GL_CONTEXT_PROFILE_CORE`. Деferred-конвейер (GBuffer → accumulation → postprocess) был уже
шейдерным (`#version 150`) и VAO/VBO-ориентированным; единственные активные fixed-function-вызовы
сидели в мёртвых методах интерфейса `Renderer3D`. Убраны legacy-методы и их реализации:
`SetColor` (`glColor4f`), `SetTextureMode`, `RenderAABB`×2 (`glBegin/glEnd`, тело уже в комментарии),
`SetLight` (`glLightfv`, тело уже в комментарии), `SetClientTextureUnit` (`glClientActiveTexture`),
`PushAttribute`/`PopAttribute` (`glPushAttrib`/`glPopAttrib`), `SetColorMask`, HDR-захват яркости;
удалены `drawSphere` и point-light-ветка `RenderLights` (light.type всегда 0). `sdl_glfuncs.h`:
28 deprecated-функций переведены `SDL_PROC`→`SDL_PROC_UNUSED` — под core profile
`SDL_GL_GetProcAddress` вернул бы NULL и лоадер упал бы с `exit(1)`. Проверки: полная сборка MSVC
(Win32) зелёная, `determinism_runner check 372c4bbd...` → 0, запуск игры подтверждает
`Using OpenGL version 3.2 ... Core Profile Context` без ошибок/предупреждений (меню рендерится,
FBO complete). GLES-перевод шейдеров (`#version 300 es`) — отдельная задача, в
`docs/wiki/открытые-вопросы`. План: `docs/plans/2026-08-11-render-modernization.md`. Ветка в master
не влита.
## [2026-08-12] feat | Ввод с геймпада: SDL3 SDL_Gamepad (semantic indices), фикс instance ID (Xbox Series), пресеты PES/FIFA на выборе сторон, GUI-навигация стик+крестовина, hot-plug (пауза+выбор сторон при отключении в матче), удалена калибровка джойстика. Эталон determinism 372c4bbd... не сдвинулся. Спека docs/specs/2026-08-12-gamepad-input-design.md, план docs/plans/2026-08-12-gamepad-input.md. Ручная проверка на Xbox Series отложена (геймпада не было на машине).
