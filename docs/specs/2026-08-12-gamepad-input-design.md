# Дизайн: ввод с геймпада — SDL3 SDL_Gamepad, настраиваемая раскладка, hot-plug

> **Номенклатура SDL3:** в SDL3.4 тип называется `SDL_Gamepad`, события
> `SDL_EVENT_GAMEPAD_*`, функции `SDL_OpenGamepad`/`SDL_CloseGamepad`/
> `SDL_GetGamepadName`/`SDL_GamepadHasAxis`. Старые имена `SDL_GameController*`
> доступны как алиасы через `SDL_oldnames.h` (включается автоматически из
> `SDL3/SDL.h`). Далее по тексту используется актуальное имя `SDL_Gamepad`.

Дата: 2026-08-12. Репозиторий: `C:\Users\User\Desktop\projects\GameplayFootball` (master).
Статус: **дизайн согласован с пользователем, реализация не начата.**

Этот документ — неизменяемый снимок (см. `docs/specs/`). Он фиксирует дизайн
решения на дату; *как система устроена сейчас* после реализации описывает вики
(`docs/wiki/`), а обновление вики — обязательная часть плана.

## Проблема

Геймпад Xbox Series определяется системой, но не работает вообще: не реагирует
ни меню, ни матч («невозможно матч начать»).

### Корневая причина

В `UserEventManager::InputSDLEvent` (`src/managers/usereventmanager.cpp:100,105,110`)
индексом массивов `joyButtonPressed`/`joyAxis` служит `event.jbutton.which` /
`event.jaxis.which`. В SDL3 это **instance ID устройства** (`SDL_JoystickID`),
а не порядковый индекс. Xbox Series через HIDAPI-драйвер получает instance ID,
отличный от 0. События пишутся в ячейку `[which]`, а `HIDGamepad(0)` и GUI читают
ячейку `[0]` — пустоту. Отсюда «геймпад есть, но не отвечает».

### Вторичные проблемы

1. Код использует `SDL_Joystick` (сырые оси/кнопки), а не `SDL_GameController`
   со стандартной раскладкой `SDL_GAMEPAD_BUTTON_*` / `SDL_GAMEPAD_AXIS_*`.
2. Триггеры Xbox Series — отдельные небиполярные оси (0..32767), а текущая
   логика «полуоси» (`axisID, sign`) даёт неверные значения.
3. Раскладка завязана на числовые магические константы, а не на семантику.
4. `analogStickDeadzone = 0.75` (`src/gamedefines.hpp:31`) — направление срабатывает
   только при отклонении стика >75%, слишком жёстко.
5. Hot-plug отсутствует: `gamecontext.cpp:256-261` создаёт геймпады один раз на
   старте, события добавления/удаления не слушаются.

## Решение (вариант А)

Ввести слой семантического ввода. Ключевая идея: **семантика `SDL_GAMEPAD_*`
живёт в индексах массивов `UserEventManager`**, а не в прослойке над сырыми
числами. Тогда GUI-ввод, привязанный к тем же массивам (`guitask.cpp:157,179`,
`main.cpp:185`), чинится тем же движением и не требует второго маппинга.

```
SDL_GameController → UserEventManager (семантика) → HIDGamepad (функции) → игра
```

Потребители (`HIDGamepad::GetButton(e_ButtonFunction_*)`, GUI `JoystickEvent`,
`main.cpp:185`) сохраняют интерфейс; меняется только смысл индексов и внутренности.

### Область применения

Ориентируемся на геймпады с левым аналоговым стиком. Simple-режим и поддержка
геймпадов без стиков — **только задел** (флаг в конфиге и константа), реализация
откладывается.

## Секция 1. `UserEventManager` — слой событий

- Инициализация: `SDL_InitSubSystem(SDL_INIT_GAMEPAD)` вместо `SDL_INIT_JOYSTICK`.
  Перечисление через `SDL_GetJoysticks()`, для каждого — `SDL_OpenGamepad(joystickID)`.
  Хранить `SDL_Gamepad*` по слоту + запоминать `SDL_JoystickID` для слота.
- События: вместо `SDL_EVENT_JOYSTICK_*` слушать `SDL_EVENT_GAMEPAD_*`:
  - `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP` → `joyButtonPressed[slot][event.gbutton.button]`,
    где `button` — `SDL_GamepadButton` (семантика);
  - `SDL_EVENT_GAMEPAD_AXIS_MOTION` → `joyAxis[slot][event.gaxis.axis]` с нормализацией.
- **Фикс instance ID:** `which` — это `SDL_JoystickID`, не индекс. Перевод
  `which → slot` по открытым геймпадам (`SDL_GetGamepadID`). Это устраняет
  корень поломки.
- **Нормализация осей при записи:**
  - `LEFTX/LEFTY/RIGHTX/RIGHTY` (стики): −1..1 из raw −32768..32767;
  - `LEFT_TRIGGER/RIGHT_TRIGGER`: 0..1 из raw 0..32767 — триггеры перестают быть
    «полуосями», deadzone/знак решаются здесь.
- **Калибровка удаляется:** `SetJoystickAxisCalibration` и чтение
  `input_gamepad_*_calibration_*_*` из конфига исчезают. `GetJoystickAxis`
  возвращает нормализованное значение напрямую.
- Интерфейс потребителей сохраняется: `GetJoyButtonState(j, id)` /
  `GetJoystickAxis(j, axisID)` — та же сигнатура, меняется только смысл `id`/`axisID`.
- Приятный факт: `SDL_GAMEPAD_AXIS_LEFTX=0`, `LEFTY=1` — стик в GUI (`axes[0]`,
  `axes[1]`) и в игре совпадает по индексам, GUI-навигация стиком чинится
  автоматически.

## Секция 2. `HIDGamepad` — функции и раскладка

- `controllerMapping[i]` хранит `SDL_GamepadButton` (положительное) или
  `SDL_GamepadAxis` со знаком (отрицательное). Соглашение «отрицательное = ось»
  сохраняется — `Gui2CaptureJoy` пишет в тот же формат.
- Дефолты PES6 на семантике:
  - движение — левый стик: Up = `SDL_GAMEPAD_AXIS_LEFTY` (−), Down = `LEFTY` (+),
    Left = `LEFTX` (−), Right = `LEFTX` (+);
  - Y/B/A/X, L1/R1, Select/Start — `SDL_GAMEPAD_BUTTON_*`;
  - L2 = `SDL_GAMEPAD_AXIS_LEFT_TRIGGER`, R2 = `RIGHT_TRIGGER` — отдельные оси 0..1.
- `Process()` схлопывается: кнопка → `GetJoyButtonState(slot, mapping)`, ось →
  `GetJoystickAxis(slot, axis)` с учётом знака. Математика `(axisID, sign)` и
  вызовы `SetJoystickAxisCalibration` удаляются.
- `GetControllerMapping` (его ест `main.cpp:185`) продолжает возвращать число;
  теперь это `SDL_GAMEPAD_BUTTON_A/B` (A=0, B=1 — те же числа, что сейчас).
- **Деззона:** `analogStickDeadzone` снижается 0.75 → 0.3. Дополнительно в
  `Process()` вводится малый порог срабатывания кнопки от стика ~0.2.

### Пресечты раскладки PES / FIFA

Отличаются только `functionMapping` (какая физическая кнопка какой функцией
занята). `controllerMapping` (физика) не трогается.

- **PES** (дефолт): A = короткий пас, B = верхний пас, X = удар, Y = длинный пас.
- **FIFA:** X = короткий пас, A = удар, B = верхний пас, Y = длинный пас.

Вводится `enum e_ControllerLayout { e_ControllerLayout_PES, e_ControllerLayout_FIFA }`
и переключатель в `ControllerSelectPage`: у каждого геймпада — **визуальный
переключатель «PES / FIFA»** рядом со строкой контроллера. Он навигируется и
подтверждается кнопкой активации (A), циклически меняет пресет этого геймпада,
значение пишется в конфиг (`input_gamepad_<id>_layout`). Управление по
Left/Right (смена стороны) и A/B (подтвердить/отменить) не конфликтует.

## Секция 3. GUI-навигация (стик + крестовина)

Во **всех** GUI-экранах (главное меню, меню матча, настройки и т.д.) направление
навигации = стик **плюс** крестовина одновременно.

В SDL3 крестовина — кнопки `SDL_GAMEPAD_BUTTON_DPAD_*`. В `guitask.cpp:172`
(формирование `stick1Direction` из `axes[0], axes[1]`) добавляется вклад от
dpad-кнопок из `joyButtonPressed`. Это единственная правка в GUI-вводе.

## Секция 4. Hot-plug

- **Стабильный ключ устройства — `SDL_JoystickID`** (`which` из событий).
  Вводится как первичный идентификатор вместо позиционных индексов везде, где
  сейчас `controllers.at(i)` / `side.controllerID`:
  - `controllerselect.cpp` — маппинг по `SDL_JoystickID`;
  - `match.cpp:598` (`AddHumanGamer`) — привязка по `SDL_JoystickID`;
  - `settings.cpp`, `replaymenu.cpp`, `gamepage.cpp` — чтение по ключу.
- Слоты в `UserEventManager` не фиксированные: назначаются при
  `SDL_EVENT_GAMEPAD_ADDED`, освобождаются при `REMOVED`.
- **Поток событий:** ADDED/REMOVED приходят в рендер-поток (`SDL_PollEvent`).
  Они не трогают `controllers` (читается из game-потока) напрямую — реакция
  планируется через очередь: рендер-поток ставит событие, game-поток применяет
  на границе кадра.
- `gamecontext.cpp:256-261` — создание/удаление `HIDGamepad` переезжает со
  статики на эти события.
- Меню: `controllerselect` перечитывает список геймпадов при показе страницы.

### Поведение при отключении геймпада во время матча

1. `SDL_EVENT_GAMEPAD_REMOVED` → пауза матча → открывается меню паузы → поверх
   него `ControllerSelectPage` (тот же, что в предматчевом потоке; уже умеет
   `inGame`-режим, `controllerselect.cpp:19`).
2. Пока геймпад не вернулся, он отсутствует в списке страницы. После возврата —
   появляется; любой игрок нажимает кнопку — окно закрывается, матч остаётся на
   паузе, возобновление — как обычно.
3. Окно можно закрыть без ожидания переподключения (Escape/кнопка) — применяется
   текущий расклад `sides` на момент нажатия (например P1 против CPU).
4. В меню паузы добавляется пункт **«Выбор сторон»** — открывает ту же страницу
   в любой момент.

## Секция 5. Конфиг

- `input_gamepad_<id>_<i>` — физическая привязка `e_ControllerButton`
  (`SDL_GAMEPAD_BUTTON_*` или отрицательное `SDL_GAMEPAD_AXIS_*`).
- `input_gamepad_<id>_mapping_<i>` — `functionMapping` (без изменений формата).
- **Новое:** `input_gamepad_<id>_layout` = `0` (PES) / `1` (FIFA) — читается через
  `GetInt`, единый стиль с остальными ключами.
- **Новое (задел):** `input_gamepad_<id>_simple` = `false` — флаг читается,
  но не влияет (задел под simple-режим).
- **Удаляется:** `input_gamepad_<id>_calibration_<i>_{min,max,rest}`.

## Секция 6. Константы (`gamedefines.hpp`)

- `analogStickDeadzone`: 0.75 → 0.3.
- `enum e_ControllerLayout { e_ControllerLayout_PES, e_ControllerLayout_FIFA }`.
- `defaultControllerLayout = e_ControllerLayout_PES`.
- `defaultControllerSimpleMode = false` (задел).
- В комментарии к `e_ControllerButton` отметить соответствие `SDL_GAMEPAD_BUTTON_*`.
- Синхронизировать `docs/wiki/константы.md`.

## Не входит в эту задачу

- Simple-режим для геймпадов без стиков / с малым числом кнопок (только задел).
- Поддержка геймпадов без левого стика в матче (dpad-движение) — отложено
  вместе с simple.
- Сетевой слой (ворота №4 из дорожной карты) — не входит.

## Тестирование

1. `cmake --build` — сборка x86 (`CMAKE_GENERATOR_PLATFORM=Win32`, конфиг Release).
2. `tools/determinism` headless: `determinism check 372c4bbd...` — hash **не
   должен сдвинуться** (правки ввода не трогают симуляцию; это доказывает, что
   геймплей не сломан).
3. Ручная проверка на Xbox Series:
   - навигация стиком и крестовиной во всех GUI;
   - PES/FIFA-переключатель на экране выбора сторон;
   - «Выбор сторон» в меню паузы;
   - hot-plug: подключение/отключение геймпада на лету в меню и в матче
     (пауза + выбор сторон поверх, закрытие без ожидания возврата);
   - движение в матче только со стика.

## Затронутые файлы (предварительный список)

- `src/managers/usereventmanager.cpp` / `.hpp`
- `src/hid/gamepad.cpp` / `.hpp`
- `src/gamecontext.cpp`
- `src/main.cpp`
- `src/gamedefines.hpp`
- `src/utils/gui2/guitask.cpp`
- `src/menu/controllerselect.cpp`
- `src/menu/ingame/gamepage.cpp` (добавление пункта «Выбор сторон» в меню паузы)
- `src/menu/settings.cpp`, `src/menu/ingame/replaymenu.cpp` (переход на `SDL_JoystickID`)
- `src/onthepitch/match.cpp` (привязка контроллеров, реакция на отключение)
- `docs/wiki/константы.md`, `docs/wiki/ввод-геймпад` (обновление вики)

Точный список и порядок правок — в плане `docs/plans/2026-08-12-gamepad-input.md`.
