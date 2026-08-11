# Ввод с геймпада — SDL3 SDL_Gamepad, настраиваемая раскладка, hot-plug (ворота №4)

> **Номенклатура SDL3:** в SDL3.4 тип называется `SDL_Gamepad`, события
> `SDL_EVENT_GAMEPAD_*`, функции `SDL_OpenGamepad`/`SDL_CloseGamepad`/
> `SDL_GetGamepadName`/`SDL_GamepadHasAxis`. Старые имена `SDL_GameController*`
> доступны как алиасы через `SDL_oldnames.h` (включается автоматически из
> `SDL3/SDL.h`). В плане используются актуальные имена `SDL_Gamepad*`.

> **Для агентных исполнителей:** РЕКОМЕНДУЕТСЯ РЕАЛИЗОВАТЬ ЧЕРЕЗ `executing-plans`.
> Каждая задача заканчивается сборкой и `determinism check` (эталон Windows x86 =
> `372c4bbdf78e294a05dbbc9176f86741054f46fb`).

**Goal:** Починить ввод с геймпада (Xbox Series и любой современный геймпад):
перейти на SDL3 `SDL_Gamepad`, завести семантику `SDL_GAMEPAD_BUTTON_*` /
`SDL_GAMEPAD_AXIS_*` в индексах `UserEventManager`, добавить настраиваемые
пресеты раскладки PES/FIFA, навигацию GUI со стика и крестовины и hot-plug
(подключение/отключение на лету, при отключении в матче — пауза + выбор сторон).

**Architecture:** Слой семантического ввода `SDL_Gamepad →
UserEventManager → HIDGamepad → игра`. Ключевая идея — семантика живёт в
**индексах массивов** `UserEventManager`, а не в прослойке над сырыми числами:
тогда GUI (`guitask.cpp`), `main.cpp:185` и матч чинятся тем же движением.
Корень поломки — `event.jbutton.which`/`event.jaxis.which` (instance ID) раньше
использовался как индекс массива. Проектная спека —
`docs/specs/2026-08-12-gamepad-input-design.md`.

**Tech Stack:** SDL3 (SDL_Gamepad API), C++17, Boost, CMake, OpenGL (не
трогаем).

## Global Constraints

- **Геймплей не трогаем**: `Match::Process`, AI, мяч, физика — без изменений.
  Ввод не влияет на симуляцию. Это проверяется эталоном детерминизма.
- **Эталон детерминизма обязателен** после каждой задачи:
  `determinism check 372c4bbdf78e294a05dbbc9176f86741054f46fb` → код 0
  (Windows x86). Эталон не должен сдвинуться.
- **Сборка — из каталога сборки** с копией `data/`; проверка —
  `cmake --build <build> --config Release`.
- **Движение в матче — только со стика.** Simple-режим и безстиковые геймпады —
  только задел (флаг читается, не влияет).
- **GUI-навигация — стик + крестовина** одновременно на всех экранах.
- **Дефолт раскладки — PES**, переключатель PES/FIFA на экране выбора сторон
  (per-gampad, визуальный переключатель, активация кнопкой A).
- **Hot-plug обязателен**: подключение/отключение на лету в меню и в матче.
  Отключение в матче → пауза + выбор сторон поверх; закрытие без ожидания
  возврата применяет текущий расклад. Пункт «выбор сторон» уже есть в меню паузы
  (`ingame.cpp:27`), его сохраняем.
- **Boost не сокращаем. Ассеты не меняем.**
- Ветка работы: новая (например `gamepad-input`) от `master`.
- Вики обновляется после каждой содержательной задачи (не «UPDATE:», а
  переписывание); константы сверяются с `docs/wiki/константы.md`.

---

## Файловая структура

| Файл | Роль |
|---|---|
| `src/gamedefines.hpp` | константы ввода: `analogStickDeadzone`, `e_ControllerLayout`, задел `defaultControllerSimpleMode`. |
| `src/managers/usereventmanager.hpp/.cpp` | ядро: SDL_Gamepad, семантика в индексах, нормализация осей, фикс instance ID, горячее добавление/удаление устройств. |
| `src/hid/gamepad.hpp/.cpp` | семантические дефолты, пресеты PES/FIFA, упрощённый `Process()`, `GetJoystickID()`. |
| `src/utils/gui2/guitask.cpp` | GUI-навигация: стик + крестовина. |
| `src/menu/controllerselect.cpp/.hpp` | переключатель PES/FIFA per-геймпад, привязка по `SDL_JoystickID`. |
| `src/menu/ingame/gamepage.cpp` | чтение принадлежности контроллера по joystickID. |
| `src/menu/ingame/replaymenu.cpp` | оси по семантическим индексам. |
| `src/menu/settings.cpp` | удалить страницу калибровки; чтение/запись маппинга — семантика; пресет в настройках. |
| `src/menu/pagefactory.cpp/.hpp` | удалить `e_PageID_GamepadCalibration`. |
| `src/gamecontext.cpp` | hot-plug: пересборка списка геймпадов из актуальных устройств. |
| `src/onthepitch/match.cpp/.hpp` | `UpdateControllerSetup` по joystickID; реакция на отключение. |
| `src/main.cpp` | без изменений (проверить, что `main.cpp:185` работает с новыми числами). |
| `docs/wiki/константы.md`, `docs/wiki/архитектура.md`, `log.md` | документация. |
| `docs/plans/2026-08-12-gamepad-input.md` | этот план. |

---

### Task 1: Константы ввода в `gamedefines.hpp`

**Files:**
- Modify: `src/gamedefines.hpp:31`

**Интерфейс:** производит `e_ControllerLayout`, `defaultControllerLayout`,
`defaultControllerSimpleMode`; меняет `analogStickDeadzone`. Потребляется в
Task 2-3 (`HIDGamepad::LoadConfig`).

- [ ] **Step 1: Изменить `analogStickDeadzone` и добавить enum/задел**

В `src/gamedefines.hpp` заменить строку 31 и добавить после неё:

```cpp
const float analogStickDeadzone = 0.3f;

// gamepad layout presets (which face button does what). PES6 style is the default.
enum e_ControllerLayout {
  e_ControllerLayout_PES,
  e_ControllerLayout_FIFA
};
const e_ControllerLayout defaultControllerLayout = e_ControllerLayout_PES;
const bool defaultControllerSimpleMode = false; // reserved: simple mode for sticks-less gamepads is a future feature (see docs/wiki/открытые-вопросы.md)
```

- [ ] **Step 2: Сборка**

Run (из каталога сборки, напр. `build/`): `cmake --build build --config Release --target gameplayfootball`
Expected: сборка успешна. Эта константа нигде не используется напрямую, компиляция
не должна сломаться.

- [ ] **Step 3: Commit**

```bash
git add src/gamedefines.hpp
git commit -m "feat(input): add gamepad layout enum and lower analog deadzone"
```

---

### Task 2: `UserEventManager` — SDL_Gamepad, семантика, нормализация, фикс instance ID

**Files:**
- Modify: `src/managers/usereventmanager.hpp`
- Modify: `src/managers/usereventmanager.cpp`

**Интерфейс:**
- Consumes: Task 1 (`e_ControllerLayout` не нужен здесь; нужен `_JOYSTICK_MAX` — уже есть).
- Produces:
  - `SDL_Gamepad *GetGamepad(int slot)` — для `HIDGamepad` (имя устройства, has-axis).
  - `SDL_JoystickID GetJoystickID(int slot)` — стабильный ключ устройства.
  - `int GetSlotForJoystickID(SDL_JoystickID)` — перевод `which → slot`.
  - `bool HasAxis(int slot, SDL_GamepadAxis axis)` — есть ли ось (для задела simple).
  - События: `SDL_EVENT_GAMEPAD_*` пишут в `joyButtonPressed[slot][SDL_GAMEPAD_BUTTON_*]`,
    `joyAxis[slot][SDL_GAMEPAD_AXIS_*]` (нормализовано).
  - `InputSDLEvent` обрабатывает `SDL_EVENT_GAMEPAD_ADDED/REMOVED` → пересканирование.
  - Сигнатуры `GetJoyButtonState/SetJoyButtonState/GetJoystickAxis` сохраняются.
  - Удаление калибровочных API (`GetJoystickAxisRaw`, `GetJoystickAxisCalibrationMin/Max/Rest`,
    `SetJoystickAxisCalibration`) — **не здесь**, а в Task 5, вместе с чисткой
    `settings.cpp`. Здесь они остаются (пока используются `gamepad.cpp:46,115,123`
    и `settings.cpp:571,595-596`), чтобы каждая задача собиралась и
    `determinism check` проходил после каждой.
  - Добавляется `gamepadCount` (риск «GetJoystickCount нестабилен между потоками»):
    актуальный счёт, обновляемый в `RescanGamepads`, возвращаемый `GetJoystickCount()`.

- [ ] **Step 1: Заголовок — новые поля и методы**

В `src/managers/usereventmanager.hpp`:

- Заменить поле `SDL_Joystick *joystick[_JOYSTICK_MAX];` (строка 85) на:

```cpp
SDL_Gamepad *gamepad[_JOYSTICK_MAX];
SDL_JoystickID joystickID[_JOYSTICK_MAX]; // stable device key per slot
int gamepadCount = 0; // actual count, updated in RescanGamepads (thread-safe read for GetJoystickCount)
```

- Добавить публичные методы после `GetJoystickCount()`:

```cpp
SDL_Gamepad *GetGamepad(int slot);
SDL_JoystickID GetJoystickID(int slot);
int GetSlotForJoystickID(SDL_JoystickID id);
bool HasAxis(int slot, SDL_GamepadAxis axis);
```

- Удалить объявления: **нет** — калибровочные API (`GetJoystickAxisRaw`,
  `GetJoystickAxisCalibrationMin/Max/Rest`, `SetJoystickAxisCalibration`)
  остаются до Task 5 (см. интерфейс выше).

- [ ] **Step 2: Конструктор — открытие геймпадов через SDL_Gamepad**

Заменить конструктор целиком:

```cpp
UserEventManager::UserEventManager() {
  lastKeyTime_ms = 0;

  for (int i = 1; i < 8; i++) {
    mousePressed[i] = false;
  }

  for (int j = 0; j < _JOYSTICK_MAX; j++) {
    for (int i = 0; i < _JOYSTICK_MAXBUTTONS; i++) {
      joyButtonPressed[j][i] = false;
    }
    for (int i = 0; i < _JOYSTICK_MAXAXES; i++) {
      joyAxis[j][i] = 0.0;
    }
    gamepad[j] = 0;
    joystickID[j] = 0;
  }
  gamepadCount = 0;

  SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  RescanGamepads();
}
```

- [ ] **Step 3: Деструктор — закрытие геймпадов**

Заменить деструктор:

```cpp
UserEventManager::~UserEventManager() {
  for (int i = 0; i < _JOYSTICK_MAX; i++) {
    if (gamepad[i]) SDL_CloseGamepad(gamepad[i]);
    gamepad[i] = 0;
  }
}
```

- [ ] **Step 4: RescanGamepads — первичный и горячий список**

Добавить приватный метод (и его объявление в hpp):

```cpp
void UserEventManager::RescanGamepads() {
  boost::mutex::scoped_lock lock(joyButtonPressedMutex);
  for (int i = 0; i < _JOYSTICK_MAX; i++) {
    if (gamepad[i]) SDL_CloseGamepad(gamepad[i]);
    gamepad[i] = 0;
    joystickID[i] = 0;
  }
  int joystickCount = 0;
  SDL_JoystickID *joystickIDs = SDL_GetJoysticks(&joystickCount);
  gamepadCount = joystickCount;
  if (gamepadCount > _JOYSTICK_MAX) gamepadCount = _JOYSTICK_MAX;
  for (int i = 0; i < joystickCount && i < _JOYSTICK_MAX; i++) {
    gamepad[i] = SDL_OpenGamepad(joystickIDs[i]);
    joystickID[i] = gamepad[i] ? joystickIDs[i] : 0;
  }
  if (joystickIDs) SDL_free(joystickIDs);
}
```

В hpp: `void RescanGamepads();` — приватный.

- [ ] **Step 5: InputSDLEvent — семантические события геймпада + hot-add/remove**

Заменить блоки joystick-событий (строки 98-112) на:

```cpp
case SDL_EVENT_GAMEPAD_ADDED:
  RescanGamepads();
  break;
case SDL_EVENT_GAMEPAD_REMOVED:
  RescanGamepads();
  break;
case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
  joyButtonPressedMutex.lock();
  int slot = GetSlotForJoystickID(event.gaxis.which);
  if (slot != -1) {
    float value = event.gaxis.value;
    bool trigger = (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
                    event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    if (trigger) {
      value = (value < 0.0f) ? 0.0f : value / 32767.0f;   // 0..1
    } else {
      value /= 32767.0f;                                   // -1..1
      if (value < -1.0f) value = -1.0f;
      if (value > 1.0f) value = 1.0f;
    }
    joyAxis[slot][event.gaxis.axis] = value;
  }
  joyButtonPressedMutex.unlock();
  break;
}
case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
case SDL_EVENT_GAMEPAD_BUTTON_UP: {
  joyButtonPressedMutex.lock();
  int slot = GetSlotForJoystickID(event.gbutton.which);
  if (slot != -1) joyButtonPressed[slot][event.gbutton.button] = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
  joyButtonPressedMutex.unlock();
  break;
}
```

Замечание: `GetSlotForJoystickID` вызывается из-под `joyButtonPressedMutex`
(RescanGamepads тоже берёт этот мьютекс). Опасность взаимоблокировки нет —
`GetSlotForJoystickID` не берёт мьютекс.

- [ ] **Step 6: Новые методы (публичные)**

```cpp
SDL_Gamepad *UserEventManager::GetGamepad(int slot) {
  if (slot < 0 || slot >= _JOYSTICK_MAX) return 0;
  return gamepad[slot];
}

SDL_JoystickID UserEventManager::GetJoystickID(int slot) {
  if (slot < 0 || slot >= _JOYSTICK_MAX) return 0;
  return joystickID[slot];
}

int UserEventManager::GetSlotForJoystickID(SDL_JoystickID id) {
  for (int i = 0; i < _JOYSTICK_MAX; i++) {
    if (joystickID[i] == id) return i;
  }
  return -1;
}

bool UserEventManager::HasAxis(int slot, SDL_GamepadAxis axis) {
  SDL_Gamepad *pad = GetGamepad(slot);
  if (!pad) return false;
  return SDL_GamepadHasAxis(pad, axis);
}
```

И заменить `GetJoystickCount()` на чтение кэша (без `SDL_GetJoysticks` из
game-потока):

```cpp
int UserEventManager::GetJoystickCount() {
  return gamepadCount;
}
```

- [ ] **Step 7: GetJoystickAxis — без калибровки, нормализованное значение**

Заменить тело `GetJoystickAxis` на:

```cpp
float UserEventManager::GetJoystickAxis(int joyID, int axisID, bool deadzone) const {
  boost::mutex::scoped_lock lock(joyButtonPressedMutex);
  float value = joyAxis[joyID][axisID];
  if (deadzone && fabs(value) < 0.05f) value = 0.0f;
  return value;
}
```

- [ ] **Step 8: Устаревшие методы — НЕ удаляем в этой задаче**

`GetJoystickAxisRaw`, `GetJoystickAxisCalibrationMin/Max/Rest`,
`SetJoystickAxisCalibration` остаются в cpp и hpp нетронутыми: их до Task 5
используют `gamepad.cpp:46,115,123` и `settings.cpp:571,595-596`. Удаление —
в Task 5 Step 5.

- [ ] **Step 9: Сборка + детерминизм**

Run: `cmake --build build --config Release --target gameplayfootball determinism_runner`
Expected: **сборка успешна** (старые API сохранены, потребители целы).

```bash
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: exit 0 (симуляция не зависит от ввода).

- [ ] **Step 10: Commit**

```bash
git add src/managers/usereventmanager.hpp src/managers/usereventmanager.cpp
git commit -m "feat(input): read gamepads via SDL_Gamepad with semantic indices and normalized axes"
```

---

### Task 3: `HIDGamepad` — семантические дефолты, пресеты PES/FIFA, упрощённый `Process()`

**Files:**
- Modify: `src/hid/gamepad.hpp`
- Modify: `src/hid/gamepad.cpp`

**Интерфейс:**
- Consumes: Task 1 (`e_ControllerLayout`, `defaultControllerLayout`), Task 2
  (`GetJoystickID`, `GetGamepad`, `HasAxis`).
- Produces:
  - `SDL_JoystickID GetJoystickID() const`
  - `e_ControllerLayout GetLayout() const` / `void SetLayout(e_ControllerLayout)`
  - `const std::vector<e_ControllerButton>& GetPresetFunctionMapping(e_ControllerLayout) const` (статик-хелперы внутри cpp).
  - `controllerMapping[i]` хранит `SDL_GamepadButton` (>=0) или `-(2*axis + sign)`.
  - Оси: LEFTX=0, LEFTY=1, RIGHTX=2, RIGHTY=3, LEFT_TRIGGER=4, RIGHT_TRIGGER=5.

- [ ] **Step 1: Заголовок — новые методы**

В `src/hid/gamepad.hpp` добавить:

```cpp
SDL_JoystickID GetJoystickID() const { return joystickID; }
e_ControllerLayout GetLayout() { boost::mutex::scoped_lock blah(mutex); return layout; }
void SetLayout(e_ControllerLayout newLayout);
```

В `protected` добавить:

```cpp
SDL_JoystickID joystickID;
e_ControllerLayout layout;
```

- [ ] **Step 2: Конструктор — получить стабильный ID**

Заменить конструктор:

```cpp
HIDGamepad::HIDGamepad(int gamepadID) : gamepadID(gamepadID) {
  deviceType = e_HIDeviceType_Gamepad;
  std::string name = "unknown";
  joystickID = UserEventManager::GetInstance().GetJoystickID(gamepadID);
  SDL_Gamepad *pad = UserEventManager::GetInstance().GetGamepad(gamepadID);
  if (pad) {
    const char *n = SDL_GetGamepadName(pad);
    if (n) name = n;
  }
  identifier = name + " #" + int_to_str(gamepadID);
  LoadConfig();
}
```

(Добавить `#include "managers/usereventmanager.hpp"` в cpp — уже есть в `gamepad.cpp:7`.)

- [ ] **Step 3: Таблица семантических дефолтов `controllerMapping`**

В `LoadConfig` заменить блок дефолтов `defaultButton` (строки 52-67) таблицей.
Новые числа (SDL3): A=0, B=1, X=2, Y=3, Select=SDL_GAMEPAD_BUTTON_BACK=4,
Start=SDL_GAMEPAD_BUTTON_START=6, L1=SDL_GAMEPAD_BUTTON_LEFT_SHOULDER=9,
R1=SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER=10. Оси: LEFTX=0, LEFTY=1, LEFT_TRIGGER=4,
RIGHT_TRIGGER=5, кодирование оси n: отрицательная половина `-(2n+1)`,
положительная `-(2n+2)`.

```cpp
signed int defaultButton = 0;
if      (i == 0) defaultButton = -3;    // Up:    LEFTY negative
else if (i == 1) defaultButton = -2;    // Right: LEFTX positive
else if (i == 2) defaultButton = -4;    // Down:  LEFTY positive
else if (i == 3) defaultButton = -1;    // Left:  LEFTX negative
else if (i == 4) defaultButton = 3;     // Y
else if (i == 5) defaultButton = 1;     // B
else if (i == 6) defaultButton = 0;     // A
else if (i == 7) defaultButton = 2;     // X
else if (i == 8) defaultButton = 9;     // L1 == LEFT_SHOULDER
else if (i == 9) defaultButton = -10;   // L2 == LEFT_TRIGGER (positive half)
else if (i == 10) defaultButton = 10;   // R1 == RIGHT_SHOULDER
else if (i == 11) defaultButton = -12;  // R2 == RIGHT_TRIGGER (positive half)
else if (i == 12) defaultButton = 4;    // Select == BACK
else if (i == 13) defaultButton = 6;    // Start == START
```

`controllerMapping[i] = GetConfiguration()->GetInt(..., defaultButton);` — без изменений.

- [ ] **Step 4: Пресеты `functionMapping` (PES/FIFA)**

Убрать блок `defaultMapping` по `e_ButtonFunction_*` (строки 72-92) и заменить на
загрузку из пресета:

```cpp
layout = (e_ControllerLayout)GetConfiguration()->GetInt(("input_gamepad_" + GetIdentifier() + "_layout").c_str(), defaultControllerLayout);
// reserved simple-mode flag (future feature, currently unused)
GetConfiguration()->GetBool(("input_gamepad_" + GetIdentifier() + "_simple").c_str(), defaultControllerSimpleMode);

const std::vector<e_ControllerButton> &preset = GetPresetFunctionMapping(layout);
for (int i = 0; i < e_ButtonFunction_Size; i++) {
  functionMapping[i] = (e_ControllerButton)GetConfiguration()->GetInt(("input_gamepad_" + GetIdentifier() + "_mapping_" + int_to_str(i)).c_str(), preset.at(i));
}
```

Добавить статические хелперы (в cpp, namespace-static или статик-методы):

```cpp
static const std::vector<e_ControllerButton>& GetPresetFunctionMapping(e_ControllerLayout layout) {
  static const std::vector<e_ControllerButton> pesPreset = {
    e_ControllerButton_Up, e_ControllerButton_Right, e_ControllerButton_Down, e_ControllerButton_Left, // movement
    e_ControllerButton_Y,  // LongPass
    e_ControllerButton_B,  // HighPass
    e_ControllerButton_A,  // ShortPass
    e_ControllerButton_X,  // Shot
    e_ControllerButton_Y,  // KeeperRush
    e_ControllerButton_B,  // Sliding
    e_ControllerButton_A,  // Pressure
    e_ControllerButton_X,  // TeamPressure
    e_ControllerButton_L1, // Switch
    e_ControllerButton_L2, // Special
    e_ControllerButton_R1, // Sprint
    e_ControllerButton_R2, // Dribble
    e_ControllerButton_Start, // Start
    e_ControllerButton_Select // Select
  };
  static const std::vector<e_ControllerButton> fifaPreset = {
    e_ControllerButton_Up, e_ControllerButton_Right, e_ControllerButton_Down, e_ControllerButton_Left,
    e_ControllerButton_Y,  // LongPass
    e_ControllerButton_B,  // HighPass
    e_ControllerButton_X,  // ShortPass  (FIFA: X = pass)
    e_ControllerButton_A,  // Shot       (FIFA: A = shot)
    e_ControllerButton_Y,  // KeeperRush
    e_ControllerButton_B,  // Sliding
    e_ControllerButton_B,  // Pressure   (FIFA: B = standing tackle)
    e_ControllerButton_X,  // TeamPressure
    e_ControllerButton_L1, // Switch
    e_ControllerButton_L2, // Special
    e_ControllerButton_R1, // Sprint
    e_ControllerButton_R2, // Dribble
    e_ControllerButton_Start,
    e_ControllerButton_Select
  };
  return (layout == e_ControllerLayout_FIFA) ? fifaPreset : pesPreset;
}
```

Порядок элементов должен совпадать с `e_ButtonFunction` в `ihidevice.hpp:17-37`.
(Проверить количество: 18 функций.)

- [ ] **Step 5: SetLayout**

```cpp
void HIDGamepad::SetLayout(e_ControllerLayout newLayout) {
  boost::mutex::scoped_lock blah(mutex);
  layout = newLayout;
  const std::vector<e_ControllerButton> &preset = GetPresetFunctionMapping(layout);
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    functionMapping[i] = preset.at(i);
  }
  GetConfiguration()->Set(("input_gamepad_" + GetIdentifier() + "_layout").c_str(), (int)layout);
  GetConfiguration()->SaveFile(GetConfigFilename());
}
```

- [ ] **Step 6: Process() — упрощение**

Заменить `Process()` (строки 108-128):

```cpp
void HIDGamepad::Process() {
  boost::mutex::scoped_lock blah(mutex);
  for (int i = 0; i < e_ControllerButton_Size; i++) {
    previousControllerButtonState[i] = controllerButtonState[i];
    signed int buttonID = controllerMapping[i];
    if (buttonID >= 0) { // button
      controllerButtonState[i] = UserEventManager::GetInstance().GetJoyButtonState(gamepadID, buttonID) ? 1.0 : 0.0;
    } else { // axis (semantic, encoded as -(2*axis + 1) = negative half, -(2*axis + 2) = positive half)
      int axisID = -buttonID - 1;
      signed int sign = ((axisID % 2) * 2) - 1;
      axisID /= 2;
      float value = UserEventManager::GetInstance().GetJoystickAxis(gamepadID, axisID, true);
      if ((sign < 0 && value < 0) || (sign > 0 && value > 0)) controllerButtonState[i] = fabs(value); else
                                                              controllerButtonState[i] = 0;
    }
  }
}
```

Логика знака — та же, что была; изменились только числа в `controllerMapping`.
Триггеры: ось 4/5 хранится в `joyAxis` как 0..1 (всегда >=0), поэтому sign = +1
даёт срабатывание при любом нажатии.

- [ ] **Step 7: Сборка + детерминизм**

```bash
cmake --build build --config Release --target gameplayfootball determinism_runner
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: сборка ок; `check` → exit 0. Ошибки из Task 2 Step 9 в `gamepad.cpp`
исчезли. Оставшиеся ошибки — только `settings.cpp` (чиним Task 5).

- [ ] **Step 8: Commit**

```bash
git add src/hid/gamepad.hpp src/hid/gamepad.cpp
git commit -m "feat(input): semantic gamepad defaults, PES/FIFA presets, simplified Process"
```

---

### Task 4: GUI-навигация — стик + крестовина

**Files:**
- Modify: `src/utils/gui2/guitask.cpp:139-176`

**Интерфейс:**
- Consumes: Task 2 (семантические `joyButtonPressed`, нормализованные оси).
- Produces: GUI-направление учитывает стик и dpad.

- [ ] **Step 1: Учесть dpad в направлении**

Заменить в `guitask.cpp` блок формирования `stick1Direction` (строки 172-176):

```cpp
Vector3 stick1Direction = Vector3(axes[0], axes[1], 0);
// digital cross adds to stick for menu navigation (semantic dpad buttons)
if (UserEventManager::GetInstance().GetJoyButtonState(j, SDL_GAMEPAD_BUTTON_DPAD_UP))    stick1Direction.coords[1] += 1.0f;
if (UserEventManager::GetInstance().GetJoyButtonState(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  stick1Direction.coords[1] -= 1.0f;
if (UserEventManager::GetInstance().GetJoyButtonState(j, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  stick1Direction.coords[0] -= 1.0f;
if (UserEventManager::GetInstance().GetJoyButtonState(j, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) stick1Direction.coords[0] += 1.0f;
if (j == GetActiveJoystickID() && stick1Direction.GetLength() != 0) {
  wEvent->SetDirection(stick1Direction);
  needsWindowingEvent = true;
}
```

- [ ] **Step 2: Сборка**

```bash
cmake --build build --config Release --target gameplayfootball
```
Expected: сборка ок. (SDL_GAMEPAD_BUTTON_DPAD_* доступны из SDL3-заголовков,
`guitask.cpp` их видит через `usereventmanager.hpp` → `SDL3/SDL.h`.)

- [ ] **Step 3: Commit**

```bash
git add src/utils/gui2/guitask.cpp
git commit -m "feat(input): menu navigation from both stick and dpad"
```

---

### Task 5: `settings.cpp` — удалить калибровку, семантика в маппинге

**Files:**
- Modify: `src/menu/settings.cpp` (удалить `GamepadCalibrationPage`; обновить декодирование осей в `GamepadMappingPage`; в `GamepadSetupPage` убрать кнопку калибровки)
- Modify: `src/menu/pagefactory.cpp` (убрать `case e_PageID_GamepadCalibration`)
- Modify: `src/menu/pagefactory.hpp` (убрать `e_PageID_GamepadCalibration` из enum)
- Modify: `src/managers/usereventmanager.hpp/.cpp` (удалить калибровочные API — теперь без потребителей)

**Интерфейс:**
- Consumes: Task 2 (новые методы; калибровочные API ещё на месте), Task 3 (семантические `controllerMapping`).
- Produces: `settings.cpp` компилируется; маппинг-экран показывает семантические оси;
  калибровочные методы `UserEventManager` удалены (последний потребитель — `settings.cpp` — исчез).

- [ ] **Step 1: Убрать enum**

В `src/menu/pagefactory.hpp:36` удалить строку `e_PageID_GamepadCalibration,`.

- [ ] **Step 2: Убрать case из PageFactory**

В `src/menu/pagefactory.cpp:135-136` удалить:

```cpp
case e_PageID_GamepadCalibration:
  page = new GamepadCalibrationPage(windowManager, pageData);
```

- [ ] **Step 3: Убрать кнопку калибровки из GamepadSetupPage**

В `src/menu/settings.cpp` найти `buttonCalibration` (строка ~483) и связанные
`GoGamepadCalibrationPage` (строка ~507): удалить создание кнопки, её connect и
метод `GoGamepadCalibrationPage`. (Прочитать контекст 440-520 перед правкой —
структура «сетки» страницы должна остаться валидной.)

- [ ] **Step 4: Удалить GamepadCalibrationPage**

Удалить класс `GamepadCalibrationPage` целиком (конструктор ~528, деструктор ~563,
`Process` ~566, `ProcessKeyboardEvent` ~577, `SaveCalibration` ~589). Удалить
объявление класса из `settings.hpp` (найти и убрать).

- [ ] **Step 5: Удалить калибровочные API из UserEventManager**

Теперь, когда `settings.cpp` не ссылается на калибровку, удалить из
`src/managers/usereventmanager.cpp` тела методов `GetJoystickAxisRaw`,
`GetJoystickAxisCalibrationMin/Max/Rest`, `SetJoystickAxisCalibration` и их
объявления из `src/managers/usereventmanager.hpp`. Проверить grep, что
потребителей не осталось:

```bash
rg "GetJoystickAxisRaw|SetJoystickAxisCalibration|GetJoystickAxisCalibration" src
```
Expected: только `usereventmanager.cpp`/`.hpp` (до удаления).

- [ ] **Step 6: Декодирование осей в GamepadMappingPage — без изменений**

Проверить `settings.cpp:630-638`: декодирование `value = -gpbuttonIDs[i] - 1;
sign = ((value % 2) * 2) - 1; value /= 2;` уже совпадает с новой схемой осей
(см. Task 3 Step 6). **Изменений не требуется** — только убедиться, что строка
отображения `"A<n>+/-"` корректна. Если окажется иначе — поправить на
`"A<axis>+/-"`.

- [ ] **Step 7: Сборка + детерминизм**

```bash
cmake --build build --config Release --target gameplayfootball determinism_runner
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: сборка ок (все таргеты), `check` → exit 0.

- [ ] **Step 8: Commit**

```bash
git add src/menu/settings.cpp src/menu/settings.hpp src/menu/pagefactory.cpp src/menu/pagefactory.hpp src/managers/usereventmanager.cpp src/managers/usereventmanager.hpp
git commit -m "feat(input): drop dead joystick calibration UI, semantic axis display in mapping"
```

---

### Task 6: `ControllerSelectPage` — визуальный переключатель PES/FIFA + привязка по joystickID

**Files:**
- Modify: `src/menu/controllerselect.cpp`
- Modify: `src/menu/controllerselect.hpp`

**Интерфейс:**
- Consumes: Task 3 (`GetLayout()`, `SetLayout()`, `GetJoystickID()`).
- Produces: per-gampad визуальный переключатель; `SideSelection` матчится к
  контроллеру по `SDL_JoystickID`.

- [ ] **Step 1: Заголовок — поле для переключателя**

В `src/menu/controllerselect.hpp` добавить в приватные члены
`Gui2Button *layoutToggle[_JOYSTICK_MAX];` и метод
`void ToggleLayout(int controllerID);`. Включить `widgets/button.hpp` и
`hid/gamepad.hpp` (проверить, что есть в cpp; в hpp добавить forward-declaration
`class Gui2Button;` или include).

- [ ] **Step 2: Создать переключатель на странице**

Механизм: windowing-события (включая Activate от кнопки A) уходят **только в
фокусированный вид** (`guitask.cpp:194-200`), а `Gui2Button::ProcessWindowingEvent`
на `IsActivate()` шлёт `sig_OnClick` (`button.cpp:97-107`). Значит переключатель
делаем `Gui2Button`, а навигацию к нему и активацию даёт штатная фокус-система
стрелками.

В конструкторе `ControllerSelectPage` (в цикле `for (i...controllers.size())`,
после создания `side.controllerImage`) добавить для геймпадов:

```cpp
if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
  HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(i));
  std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
  layoutToggle[i] = new Gui2Button(windowManager, "button_controllerselect_layout" + int_to_str(i), 0, 0, 12, 3, "layout: " + layoutStr);
  layoutToggle[i]->SetPosition(43 + sides.at(i).side * 25, 27 + i * 15); // below the controller image row
  layoutToggle[i]->sig_OnClick.connect(boost::bind(&ControllerSelectPage::ToggleLayout, this, i));
  this->AddView(layoutToggle[i]);
  layoutToggle[i]->Show();
}
```

(Если `Gui2Button` создаётся с позицией в процентах — использовать координаты
согласно `windowManager->GetCoordinates`, по образцу кнопок в других страницах.
`SetImagePositions` кладёт изображения в `20 + i * 15` по y; переключатель —
в `y = 20 + i * 15 + 10`.)

- [ ] **Step 3: ToggleLayout**

```cpp
void ControllerSelectPage::ToggleLayout(int controllerID) {
  HIDGamepad *gamepad = static_cast<HIDGamepad*>(GetControllers().at(controllerID));
  gamepad->SetLayout(gamepad->GetLayout() == e_ControllerLayout_PES ? e_ControllerLayout_FIFA : e_ControllerLayout_PES);
  std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
  layoutToggle[controllerID]->SetCaption("layout: " + layoutStr);
}
```

- [ ] **Step 4: Фокус по умолчанию — на первый переключатель**

Windowing-события идут только в фокусированный вид. Чтобы переключатель был
доступен без ручной навигации, в конструкторе после создания всех виджетов
(рядом с `this->SetFocus()` в `controllerselect.cpp:37`) поставить фокус на
первый геймпадный переключатель:

```cpp
for (int i = 1; i < (signed int)controllers.size(); i++) {
  if (layoutToggle[i]) {
    layoutToggle[i]->SetFocus();
    break;
  }
}
```

При этом Left/Right продолжат менять сторону (обрабатывается в
`ProcessJoystickEvent` и `ProcessKeyboardEvent` независимо от фокуса), а
A — активирует фокусированный переключатель через windowing-событие. Если
потребуется, чтобы A открывала TeamSelect как раньше — активация переключателя
на первой же кнопке конфликта не даёт: `IsActivate()` уходит в
`layoutToggle`, а `ProcessWindowingEvent` страницы не вызывается (фокус на
кнопке). Это осознанное изменение поведения: сначала игрок выбирает раскладку,
затем активирует TeamSelect отдельной кнопкой (Start/Enter) или после смены
фокуса. Уточнить итоговый flow при ручном тесте (Task 8 Step 3).

- [ ] **Step 5: Привязка по joystickID в SideSelection**

`SideSelection` в `menutask.hpp:29` содержит `int controllerID` (индекс).
Hot-plug (Task 7) требует стабильный ключ. Добавить в `SideSelection`:

```cpp
SDL_JoystickID joystickID; // stable device key, 0 for keyboard
```

В конструкторе `ControllerSelectPage` проставлять `side.joystickID`:
`controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad ?
static_cast<HIDGamepad*>(controllers.at(i))->GetJoystickID() : 0;`

`Match::UpdateControllerSetup` (Task 7 Step 3) будет матчить по нему.

- [ ] **Step 6: Сборка + детерминизм**

```bash
cmake --build build --config Release --target gameplayfootball determinism_runner
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: сборка ок, `check` → exit 0.

- [ ] **Step 7: Commit**

```bash
git add src/menu/controllerselect.cpp src/menu/controllerselect.hpp src/menu/menutask.hpp
git commit -m "feat(input): per-gamepad PES/FIFA toggle on controller select page"
```

---

### Task 7: Hot-plug — пересборка контроллеров и реакция матча

**Files:**
- Modify: `src/gamecontext.cpp:252-264` (создание контроллеров → динамика)
- Modify: `src/gametask.cpp` (`ProcessPhase` — проверка изменения списка)
- Modify: `src/onthepitch/match.cpp/.hpp` (`UpdateControllerSetup` по joystickID; пауза при отключении)
- Modify: `src/menu/ingame/gamepage.cpp` (чтение по joystickID)
- Modify: `src/menu/ingame/replaymenu.cpp` (ось 0/1 — уже семантика, проверить)
- Modify: `src/menu/menutask.cpp` (`ReleaseAllButtons` — итерировать по актуальному слоту)

**Интерфейс:**
- Consumes: Task 2 (`RescanGamepads` на ADDED/REMOVED; `GetJoystickCount`),
  Task 6 (`SideSelection.joystickID`).
- Produces: при добавлении/удалении геймпада `controllers` пересобирается в
  game-потоке; отключение геймпада игрока в матче ставит паузу и открывает
  выбор сторон поверх.

- [ ] **Step 1: GameContext — пересборка списка геймпадов**

Заменить блок `// controllers` в `gamecontext.cpp:252-264` на:

```cpp
// controllers (keyboard always present, gamepads rescanned dynamically)
controllers.clear();
controllers.push_back(new HIDKeyboard());
RefreshGamepads();
```

Добавить в `gamecontext.cpp`:

```cpp
void RefreshGamepads() {
  // called from the game thread (GameTask::ProcessPhase)
  int count = UserEventManager::GetInstance().GetJoystickCount();
  // remove gamepads that are gone
  for (int i = (int)controllers.size() - 1; i >= 1; i--) {
    HIDGamepad *pad = static_cast<HIDGamepad*>(controllers.at(i));
    bool stillThere = false;
    for (int j = 0; j < count; j++) {
      if (UserEventManager::GetInstance().GetJoystickID(j) == pad->GetJoystickID()) { stillThere = true; break; }
    }
    if (!stillThere) {
      delete controllers.at(i);
      controllers.erase(controllers.begin() + i);
    }
  }
  // add newly connected gamepads (keep ordering by slot)
  int existing = (int)controllers.size() - 1;
  for (int j = existing; j < count; j++) {
    controllers.push_back(new HIDGamepad(j));
  }
  // if an existing gamepad changed slot, re-map it (recreate to keep gamepadID == slot)
  for (unsigned int i = 1; i < controllers.size(); i++) {
    HIDGamepad *pad = static_cast<HIDGamepad*>(controllers.at(i));
    int slot = UserEventManager::GetInstance().GetSlotForJoystickID(pad->GetJoystickID());
    if (slot != (signed int)i) {
      // slot changed: recreate so gamepadID matches slot
      delete controllers.at(i);
      controllers.at(i) = new HIDGamepad(slot);
    }
  }
}
```

Объявить `void RefreshGamepads();` в `gamecontext.hpp` (или в `main.hpp`, где
доступен `controllers`). Проверить, где удобнее — `gamecontext.hpp` уже включён
в `main.cpp`/`gametask.cpp`.

- [ ] **Step 2: GameTask::ProcessPhase — вызов RefreshGamepads**

В `gametask.cpp:126-130`, перед циклом `Process()` контроллеров:

```cpp
void GameTask::ProcessPhase() {
  RefreshGamepads();

  for (unsigned int i = 0; i < GetControllers().size(); i++) {
    GetControllers().at(i)->Process();
  }
  ...
```

- [ ] **Step 3: Match::UpdateControllerSetup — привязка по joystickID**

Заменить `match.cpp:587-602`:

```cpp
void Match::UpdateControllerSetup() {
  teams[0]->DeleteHumanGamers();
  teams[1]->DeleteHumanGamers();

  const std::vector<SideSelection> sides = menuTask->GetControllerSetup();
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 0; i < sides.size(); i++) {
    if (sides.at(i).side == 0) continue;
    int teamID = int(round(sides.at(i).side * 0.5 + 0.5));
    IHIDevice *device = 0;
    if (sides.at(i).joystickID != 0) {
      for (unsigned int c = 1; c < controllers.size(); c++) {
        HIDGamepad *pad = static_cast<HIDGamepad*>(controllers.at(c));
        if (pad->GetJoystickID() == sides.at(i).joystickID) { device = controllers.at(c); break; }
      }
    } else {
      for (unsigned int c = 0; c < controllers.size(); c++) {
        if (controllers.at(c)->GetDeviceType() == e_HIDeviceType_Keyboard) { device = controllers.at(c); break; }
      }
    }
    if (device) teams[teamID]->AddHumanGamer(device, (e_PlayerColor)i);
  }
}
```

(Остальные поля `SideSelection` — `controllerImage`, `controllerID` — сохраняются
для GUI-совместимости; `controllerID` продолжает использоваться на странице.)

- [ ] **Step 4: Отключение геймпада в матче — пауза + выбор сторон поверх**

В `GameTask` (или через `Match`): на каждую проверку в `ProcessPhase`, если
`match` существует и активен (`!IsInMatch?` нет такого метода — используем
`GetMatch() != 0` и `!match->GetControllerSetup()->...`), сравниваем
`UserEventManager::GetInstance().GetJoystickCount()` с числом геймпадов в
`controllers`. Если стало меньше — инициируем паузу.

Добавить в `GameTask::ProcessPhase` после `RefreshGamepads()`:

```cpp
// if a human gamepad was unplugged mid-match: pause and open controller select on top
if (match) {
  bool anyGamerDeviceMissing = false;
  const std::vector<SideSelection> sides = GetMenuTask()->GetControllerSetup();
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 0; i < sides.size(); i++) {
    if (sides.at(i).side == 0) continue;
    if (sides.at(i).joystickID == 0) continue; // keyboard
    bool found = false;
    for (unsigned int c = 1; c < controllers.size(); c++) {
      if (static_cast<HIDGamepad*>(controllers.at(c))->GetJoystickID() == sides.at(i).joystickID) { found = true; break; }
    }
    if (!found) { anyGamerDeviceMissing = true; break; }
  }
  if (anyGamerDeviceMissing) {
    // pause and open pause menu with controller select on top
    GetMenuTask()->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_Ingame, Properties(), 0);
    Properties csProps;
    csProps.SetBool("isInGame", true);
    GetMenuTask()->GetWindowManager()->GetPageFactory()->CreatePage((int)e_PageID_ControllerSelect, csProps, 0);
  }
}
```

> **Оговорка:** `CreatePage` открывает страницу поверх текущей. `IngamePage`
> ставит `match->Pause(true)`. Открытие IngamePage затем ControllerSelect поверх —
> пользователь выбирает стороны; закрытие ControllerSelect через Escape применяет
> `UpdateControllerSetup` (в `controllerselect.cpp:130-132`) и возвращает к паузе.
> Частоту проверки ограничить (например, раз в секунду по `time_ms`), чтобы не
> спамить созданием страниц.

- [ ] **Step 5: gamepage.cpp и replaymenu.cpp — чтение по joystickID**

`gamepage.cpp:161-188` (ProcessJoystickEvent): заменить `joyID = gamepad->GetGamepadID()`
на поиск слота по `gamepad->GetJoystickID()`:

```cpp
int joyID = UserEventManager::GetInstance().GetSlotForJoystickID(gamepad->GetJoystickID());
if (joyID == -1) continue;
```

`replaymenu.cpp:104`: заменить жёсткий `controllers.at(1)` и `event->GetButton(0,...)`
на поиск первого геймпада и его слота (по аналогии с gamepage). Оси `0/1` —
уже семантика (LEFTX/LEFTY), индексы совпадают.

`menutask.cpp:179-184` (ReleaseAllButtons): итерировать `joyID` по
`GetJoystickCount()` — уже так и есть; проверить, что `SetJoyButtonState` по
семантическим индексам не ломает GUI (кнопки активации A/B теперь 0/1 — ок).

- [ ] **Step 6: Сборка + детерминизм**

```bash
cmake --build build --config Release --target gameplayfootball determinism_runner
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: сборка ок; эталон не сдвинулся (детерминизм-раннер работает headless,
геймпадов нет, `RefreshGamepads` на пустом списке безопасен).

- [ ] **Step 7: Commit**

```bash
git add src/gamecontext.cpp src/gamecontext.hpp src/gametask.cpp src/onthepitch/match.cpp src/onthepitch/match.hpp src/menu/ingame/gamepage.cpp src/menu/ingame/replaymenu.cpp src/menu/menutask.cpp
git commit -m "feat(input): hot-plug gamepad detection and pause on mid-match unplug"
```

---

### Task 8: Полная сборка + регрессия + ручная проверка

**Files:** нет правок.

- [ ] **Step 1: Полная сборка**

```bash
cmake --build build --config Release
```
Expected: все таргеты собраны без ошибок.

- [ ] **Step 2: Детерминизм**

```bash
build\Release\determinism_runner.exe check 372c4bbdf78e294a05dbbc9176f86741054f46fb
```
Expected: exit 0.

- [ ] **Step 3: Ручная проверка на Xbox Series**

Запуск `build\Release\gameplayfootball.exe` из `build/`. Проверить:
1. Навигация в главном меню со стика **и** крестовины.
2. Экран выбора сторон: переключатель PES/FIFA у геймпада (A — смена), смена
   сторон Left/Right, подтверждение A, отмена B.
3. Матч: движение только со стика; кнопки по пресету PES (A=пас, B=верхний пас,
   X=удар) и после переключения FIFA (X=пас, A=удар).
4. Меню паузы: пункт «controller select» открывает выбор сторон; матч на паузе.
5. Hot-plug: отключить геймпад в матче → пауза + выбор сторон поверх; подключить
   обратно → появляется в списке, кнопка закрывает окно, матч на паузе;
   закрыть без возврата (Escape) → применяется текущий расклад.
6. Hot-plug в меню: подключить второй геймпад на выборе сторон → появился сразу.

Записать результат в `log.md` (Task 9).

---

### Task 9: Документация и хронология

**Files:**
- Modify: `docs/wiki/константы.md` — `analogStickDeadzone` 0.3, `e_ControllerLayout`,
  дефолты PES, задел `simple`. (Страница заявляет сверенность с кодом — переписать
  актуальные значения, не дописывать «UPDATE:».)
- Modify: `docs/wiki/архитектура.md` — слой ввода: SDL_Gamepad, семантика
  в индексах `UserEventManager`, hot-plug, пресеты PES/FIFA.
- Modify: `docs/wiki/открытые-вопросы.md` — закрыть пункт про геймпад (если был);
  добавить задел: simple-режим и безстиковые геймпады.
- Modify: `log.md` — запись `feat`/`session` по итогу.
- (Новая страница `docs/wiki/ввод-геймпад.md` + строка в `docs/wiki/index.md`,
  если есть что документировать за пределами архитектуры; иначе — только правки
  существующих страниц.)

- [ ] **Step 1: Обновить вики**

Переписать затронутые разделы в `константы.md` и `архитектура.md` под новое
устройство ввода. Сверить `gamedefines.hpp` с `константы.md`.

- [ ] **Step 2: Обновить открытые-вопросы**

Закрыть сделанное; перенести задел simple-режима и безстиковых в список.

- [ ] **Step 3: Запись в log.md**

Дописать в конец `log.md`:

```markdown
## [2026-08-12] feat | Ввод с геймпада: SDL_Gamepad, семантика в индексах UserEventManager, пресеты PES/FIFA, GUI-навигация стик+крестовина, hot-plug (пауза+выбор сторон при отключении в матче). Спека docs/specs/2026-08-12-gamepad-input-design.md.
```

- [ ] **Step 4: Verify**

```bash
grep "^## \[" log.md | tail -3
```
Expected: последняя строка — новая запись.

- [ ] **Step 5: Commit**

```bash
git add docs/wiki/ docs/log.md 2>$null; git add log.md docs/wiki/константы.md docs/wiki/архитектура.md docs/wiki/открытые-вопросы.md
git commit -m "docs: gamepad input via SDL_Gamepad, wiki and changelog"
```

---

## Риски

- **`GetSlotForJoystickID` vs мьютексы.** `RescanGamepads` и `InputSDLEvent`
  берут `joyButtonPressedMutex`; `GetSlotForJoystickID` вызывается из-под него —
  он мьютекс не берёт (см. Task 2 Step 5). Не добавлять в него lock.
- **Поведение кнопки A на выборе сторон.** Фокус на `layoutToggle` перехватывает
  `IsActivate()` (A) — TeamSelect открывается не с первой кнопки. Осознанное
  изменение: игрок сначала выбирает раскладку. Итоговый flow (какой кнопкой
  дальше открывать TeamSelect) уточняется при ручном тесте (Task 8 Step 3); не
  менять `ProcessWindowingEvent` страницы без необходимости.
- **Слоты vs HIDGamepad.** `HIDGamepad.gamepadID` обязан совпадать со слотом
  `UserEventManager` (массивы `joyAxis`/`joyButtonPressed` индексируются по
  слоту). `RefreshGamepads` пересоздаёт геймпад при смене слота. Изменение
  порядка при hot-plug — главный источник багов; покрыть ручной проверкой
  (Task 8 Step 3).
- **Спам `CreatePage` при отключении.** В `GameTask::ProcessPhase` ограничить
  проверку по времени (например, `lastGamepadCheckTime_ms`), иначе окно будет
  переоткрываться каждый кадр, пока устройство отсутствует.
- **`GetJoystickCount` нестабилен между потоками.** `SDL_GetJoysticks` в
  `GetJoystickCount()` зовётся из game-потока; Rescan из рендер-потока. Хранить
  актуальный счёт в `UserEventManager` (поле `gamepadCount`, обновляемое в
  `RescanGamepads`) и возвращать его, чтобы не было гонки на SDL-списке.
- **`main.cpp:185`** — `SetEventJoyButtons(controllerMapping[A], controllerMapping[B])`.
  Новые числа A=0, B=1 — совпадают со старыми дефолтами, GUI-активация не ломается.
  Проверить при ручном тесте.
