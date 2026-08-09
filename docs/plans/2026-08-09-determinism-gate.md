# Ворота №1: детерминизм симуляции + регрессионный эталон — план реализации

> **Для агентных исполнителей:** РЕКОМЕНДУЕТСЯ РЕАЛИЗОВАТЬ ЧЕРЕЗ `executing-plans`.
> Шаги используют чекбоксы (`- [ ]`). Каждая задача заканчивается компиляцией и
> независимо проверяемым результатом.

**Goal:** Сделать симуляцию матча детерминированной (фиксированный шаг + сид) и
получить headless-раннер с регрессионным эталоном, проверяющим «геймплей не сломан».

**Architecture:** Отдельный бинарник `determinism_runner` в `tools/determinism/`,
использующий общий с игрой код инициализации. Из ветки `google-brain` (GRF v2.10.1)
переносим только: `EnvState` (сериализатор), отвязку времени в `Match::Process`,
`randomize(seed)` и `ProcessState` для ядра классов (Match/Ball/Player/PlayerBase)
**только с совпадающими полями**. Геймплейная логика и константы не трогаются.

**Tech Stack:** C++14, CMake, Boost (thread/system/filesystem, uuids::detail::sha1),
SQLite3, SDL2/OpenGL (не используются раннером), старый форк Blunted2.

## Global Constraints

- **НЕ менять геймплей**: ни одна константа `src/gamedefines.hpp` не меняется;
  логика `Match::Process` не переписывается — только отвязка времени (мёртвые
  переменные) + добавленные методы.
- **Источник переноса** — ветка `origin/google-brain` (GRF v2.10.1), команды вида
  `git show origin/google-brain:src/onthepitch/ball.cpp`.
- Переносим **только совпадающие поля**; поля `manMarking`, `valid_predictions`,
  `first_team`/`second_team` НЕ переносим.
- Не переносим `ProcessState` для `Team`/`Referee`/`Officials`/`MentalImage`,
  сериализацию `humanoid`/`controller`.
- Язык кода — английский; namespace `blunted`.
- Запуск и проверка — из каталога сборки с копией `data/` (относительные пути:
  `databases/default/database.sqlite`, `media/...`, `football.config`).
- Проект без тестов — проверка компиляцией (`cmake --build`) + запуском раннера.

---

## Файловая структура

| Файл | Роль |
|---|---|
| `src/gamecontext.hpp`, `src/gamecontext.cpp` | **создать**: глобальные переменные игрового контекста (scene2D, scene3D, db, config, menuTask, gameTask, pilons, controllers) + геттеры + `InitGameContext()`/`ShutdownGameContext()`. Выносится из `main.cpp`. |
| `src/main.cpp` | **изменить**: убрать вынесенное в gamecontext, оставить `main()`, `Run()`, тест-обвязку. |
| `src/defines.hpp` | **изменить**: добавить `EnvState` (raw-memcpy сериализатор) + `randomize(unsigned int)`. **`src/defines.cpp` в master НЕ существует** — randomize добавляем как inline в `defines.hpp` (или в `src/base/math/bluntmath.cpp` рядом с `randomseed()`). |
| `src/onthepitch/match.cpp`, `match.hpp` | **изменить**: отвязка времени в `Process()`; добавить `BumpActualTime_ms`, `ProcessState(EnvState*)`. |
| `src/onthepitch/ball.cpp`, `ball.hpp` | **изменить**: добавить `ProcessState(EnvState*)`. |
| `src/onthepitch/player/player.cpp`, `player.hpp` | **изменить**: добавить `ProcessState(EnvState*)` (Player). |
| `src/onthepitch/player/playerbase.cpp`, `playerbase.hpp` | **изменить**: добавить `ProcessState(EnvState*)` (PlayerBase). |
| `tools/determinism/main.cpp` | **создать**: раннер с режимами `run`/`check`. |
| `tools/determinism/reference.txt` | **создать**: эталонный SHA-1, сгенерированный `run`. |
| `CMakeLists.txt` | **изменить**: добавить таргет `determinism_runner`. |
| `docs/specs/2026-08-09-determinism-gate-design.md` | существующий (не менять). |

---

### Task 1: Вынесение игрового контекста из `main.cpp` в `src/gamecontext.*`

**Files:**
- Create: `src/gamecontext.hpp`, `src/gamecontext.cpp`
- Modify: `src/main.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `blunted::Initialize` (`src/blunted.cpp`), `Database` (`src/utils/database.hpp`),
  `GraphicsSystem`/`AudioSystem` (`src/systems/...`), `Scene2D`/`Scene3D`,
  `HIDKeyboard`/`HIDGamepad`, `MenuTask`, `GameTask`, `GetConfiguration`.
- Produces: `InitGameContext(const Properties&)`, `ShutdownGameContext()`, геттеры
  `GetScene2D/GetScene3D/GetGraphicsSystem/GetGameTask/GetMenuTask/GetDB/GetConfiguration/GetControllers`
  и pilon-геттеры — те же сигнатуры, что в текущем `src/main.hpp`.

**Почему:** раннер (отдельный бинарник) не может линковаться с `main.cpp` — там
`main()`. Все глобальные функции, которые требуют `Match` (ctor: `GetScene3D()`,
`GetMenuTask()`, `GetScheduler()`, pilon-геттеры), живут сейчас в `main.cpp`.
Их выносим в общий модуль, который линкуют и игра, и раннер.

- [ ] **Step 1: Создать `src/gamecontext.hpp`**

Переносим из `src/main.hpp` объявления глобальных контекст-функций и добавляем
`InitGameContext`/`ShutdownGameContext`:

```cpp
#ifndef _HPP_GAMECONTEXT
#define _HPP_GAMECONTEXT

#include "blunted.hpp"
#include "gametask.hpp"
#include "menu/menutask.hpp"
#include "hid/ihidevice.hpp"
#include "systems/graphics/graphics_system.hpp"
#include "base/properties.hpp"
#include "utils/database.hpp"

class Match;

bool InitGameContext(const Properties &config);
void ShutdownGameContext();

void SetGreenDebugPilon(const Vector3 &pos);
void SetBlueDebugPilon(const Vector3 &pos);
void SetYellowDebugPilon(const Vector3 &pos);
void SetRedDebugPilon(const Vector3 &pos);
void SetSmallDebugCircle1(const Vector3 &pos);
void SetSmallDebugCircle2(const Vector3 &pos);
void SetLargeDebugCircle(const Vector3 &pos);
boost::intrusive_ptr<Geometry> GetGreenDebugPilon();
boost::intrusive_ptr<Geometry> GetBlueDebugPilon();
boost::intrusive_ptr<Geometry> GetYellowDebugPilon();
boost::intrusive_ptr<Geometry> GetRedDebugPilon();
boost::intrusive_ptr<Geometry> GetSmallDebugCircle1();
boost::intrusive_ptr<Geometry> GetSmallDebugCircle2();
boost::intrusive_ptr<Geometry> GetLargeDebugCircle();
boost::shared_ptr<Scene2D> GetScene2D();
boost::shared_ptr<Scene3D> GetScene3D();
GraphicsSystem *GetGraphicsSystem();
boost::shared_ptr<GameTask> GetGameTask();
boost::shared_ptr<MenuTask> GetMenuTask();
Database *GetDB();
Properties *GetConfiguration();

#endif
```

- [ ] **Step 2: Создать `src/gamecontext.cpp`**

Переносим глобальные переменные и геттеры из `main.cpp` (строки 46-142), а
инициализацию, которая была в `main()` (строки 298-403), оборачиваем в
`InitGameContext`. Контекст содержит: `scene2D`, `scene3D`, `graphicsSystem`,
`audioSystem`, `db`, `config`, `gameTask`, `menuTask`, pilons, `controllers`.

Ключевые куски (остальные геттеры — механический перенос из `main.cpp`):

```cpp
#include "gamecontext.hpp"
#include "managers/resourcemanagerpool.hpp"
#include "utils/objectloader.hpp"
#include "scene/objectfactory.hpp"
#include "managers/systemmanager.hpp"
#include "managers/scenemanager.hpp"
#include "systems/audio/audio_system.hpp"
#include "base/log.hpp"
#include "hid/keyboard.hpp"
#include "hid/gamepad.hpp"
#include "SDL2/SDL.h"

GraphicsSystem *graphicsSystem;
AudioSystem *audioSystem;
boost::shared_ptr<Scene2D> scene2D;
boost::shared_ptr<Scene3D> scene3D;
boost::shared_ptr<GameTask> gameTask;
boost::shared_ptr<MenuTask> menuTask;
Database *db;
Properties *config;
std::vector<IHIDevice*> controllers;

bool InitGameContext(const Properties &cfg) {
  config = new Properties(cfg);

  db = new Database();
  bool dbSuccess = db->Load("databases/default/database.sqlite");
  if (!dbSuccess) { Log(e_FatalError, "gamecontext", "InitGameContext", "Could not open database"); return false; }

  SystemManager *systemManager = SystemManager::GetInstancePtr();
  graphicsSystem = new GraphicsSystem();
  bool ok = systemManager->RegisterSystem("GraphicsSystem", graphicsSystem);
  if (!ok) { Log(e_FatalError, "gamecontext", "InitGameContext", "Could not register GraphicsSystem"); return false; }
  audioSystem = new AudioSystem();
  ok = systemManager->RegisterSystem("AudioSystem", audioSystem);
  if (!ok) { Log(e_FatalError, "gamecontext", "InitGameContext", "Could not register AudioSystem"); return false; }

  graphicsSystem->Initialize(*config);
  audioSystem->Initialize(*config);

  scene2D = boost::shared_ptr<Scene2D>(new Scene2D("scene2D", *config));
  SceneManager::GetInstance().RegisterScene(scene2D);
  scene3D = boost::shared_ptr<Scene3D>(new Scene3D("scene3D"));
  SceneManager::GetInstance().RegisterScene(scene3D);

  // ... pilons: механический перенос main.cpp:335-393 ...

  HIDKeyboard *keyboard = new HIDKeyboard();
  controllers.push_back(keyboard);
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    HIDGamepad *gamepad = new HIDGamepad(i);
    controllers.push_back(gamepad);
  }
  return true;
}

void ShutdownGameContext() {
  for (unsigned int i = 0; i < controllers.size(); i++) delete controllers[i];
  controllers.clear();
  if (db) delete db; db = NULL;
  if (config) delete config; config = NULL;
}
```

**Примечание:** `MenuTask`/`GameTask` создаются в `main()` после context
(см. Task 4). В `InitGameContext` их НЕ создаём — они нужны игре, но не
раннеру. Пока `menuTask`/`gameTask` = NULL; `GetMenuTask()` возвращает NULL до
их создания (смотри Task 4 для полной последовательности).

- [ ] **Step 3: Обновить `src/main.cpp`**

- Убрать из `main.cpp` перенесённые глобальные переменные и геттеры
  (строки 46-142), заменить их на `#include "gamecontext.hpp"`.
- В `main()`: вызов `blunted::Initialize(*config)` остаётся; вместо ручного
  создания менеджеров/сцен (строки 298-403) вызвать `InitGameContext(*config)`
  (возвращает false → `Log(e_FatalError)` и выход).
- Сохранить создание `menuTask`/`gameTask` и последовательностей (строки 406+).
- `ShutdownGameContext()` вызывать на выходе из `main()` (перед/вместо старых
  delete-блоков).

- [ ] **Step 4: Обновить `CMakeLists.txt`** — добавить `gamecontext.cpp` в состав
  `CORE_SOURCES` (он линкуется в `gameplayfootball`; раннер добавим в Task 9).

- [ ] **Step 5: Собрать и проверить, что игра работает**

Run: `cmake --build . --parallel --config Release` (из каталога сборки с копией `data/`)
Expected: сборка успешна; запуск `gameplayfootball.exe` открывает окно, меню и
матч работают (визуальная проверка без изменений поведения).

- [ ] **Step 6: Commit**

```bash
git add src/gamecontext.hpp src/gamecontext.cpp src/main.cpp src/main.hpp CMakeLists.txt
git commit -m "refactor: extract game context globals from main.cpp into gamecontext"
```

---

### Task 2: `EnvState` — механизм сериализации

**Files:**
- Modify: `src/defines.hpp`, `src/defines.cpp`

**Interfaces:**
- Consumes: ничего (raw-memcpy над `std::string`).
- Produces: `class EnvState` с `process(T&)`, `process(std::vector<T>&)`,
  `process(std::list<T>&)`, `process(Player*&)`, `process(Team*&)`,
  `bool enabled()`, `bool Load()`, `std::string GetState()`.

**Почему:** переносим из google-brain `src/defines.hpp` (строки 78-170) —
автономный raw-memcpy сериализатор, НЕ зависящий от GameEnv/GameContext.

- [ ] **Step 1: Получить исходник из google-brain**

Run: `git show origin/google-brain:src/defines.hpp > C:\Users\User\AppData\Local\Temp\opencode\grf_defines.hpp`
Expected: файл содержит `class EnvState` (raw-memcpy сериализатор).

- [ ] **Step 2: Добавить `EnvState` в `src/defines.hpp`**

Переносим `class EnvState` из GRF, но **упрощаем**: убираем зависимости от
`GameEnv`/`GameContext`/`AIControlledKeyboard`/`Animation` — остаётся только
сериализация примитивов/контейнеров/`Player*`/`Team*`:

```cpp
class Player;
class Team;

class EnvState {
 public:
  EnvState(const std::string& state, bool load);
  void process(std::string &value);
  void process(Player* &value);
  void process(Team* &value);
  template<typename T> void process(std::vector<T>& collection) {
    int size = collection.size();
    process(size);
    collection.resize(size);
    for (auto& el : collection) process(el);
  }
  template<typename T> void process(std::list<T>& collection) {
    int size = collection.size();
    process(size);
    collection.resize(size);
    for (auto& el : collection) process(el);
  }
  template<typename T> void process(T& obj) {
    if (load) {
      if (pos + sizeof(T) > state.size()) { Log(blunted::e_FatalError, "EnvState", "process", "state is invalid"); }
      memcpy(&obj, &state[pos], sizeof(T));
      pos += sizeof(T);
    } else {
      state.resize(pos + sizeof(T));
      memcpy(&state[pos], &obj, sizeof(T));
      pos += sizeof(T);
    }
  }
  std::string GetState() { return state; }
  bool Load() { return load; }
 private:
  std::string state;
  size_t pos = 0;
  bool load;
};
```

**ВНИМАНИЕ:** GRF-оригинал содержит `SetControllers`/`SetAnimations`/
`setValidate`/`isFailure`/валидацию против `reference` — НЕ переносим. Для
нашего эталона достаточно односторонней записи. `Log` доступен через
`#include "base/log.hpp"` (уже есть в `defines.hpp` окружении).

- [ ] **Step 3: Добавить `randomize(unsigned int)`**

Из google-brain (`src/main.cpp`). **`src/defines.cpp` в master нет** — добавляем
inline в конец `src/defines.hpp` (в namespace blunted):

```cpp
inline void randomize(unsigned int seed) {
  srand(seed);
  rand(); // mingw32? buggy compiler? first value seems bogus
  randomseed(seed); // for the boost random
}
```

(`randomseed(unsigned int)` объявлен в `src/base/math/bluntmath.hpp` — проверить,
что принимает аргумент; в master объявлен `void randomseed();` без аргумента —
при необходимости добавить перегрузку `randomseed(unsigned int)` в
`bluntmath.hpp`/`.cpp`.)

- [ ] **Step 4: Собрать**

Run: `cmake --build . --parallel --config Release`
Expected: компилируется (EnvState не используется никем — просто компилируется).

- [ ] **Step 5: Commit**

```bash
git add src/defines.hpp src/base/math/bluntmath.hpp src/base/math/bluntmath.cpp
git commit -m "feat: add EnvState serializer and randomize(seed) from GRF"
```

---

### Task 3: Отвязка времени в `Match::Process`

**Files:**
- Modify: `src/onthepitch/match.cpp`, `src/onthepitch/match.hpp`

**Interfaces:**
- Consumes: `Match::GetActualTime_ms()`, `actualTime_ms` (member).
- Produces: `Match::BumpActualTime_ms(unsigned long)`; `Process()` больше не
  читает `EnvironmentManager::GetInstance().GetTime_ms()`.

**Почему:** см. spec секция 1. `timeSincePreviousProcess_ms` нигде не
используется в расчётах — отвязка не меняет геймплей.

- [ ] **Step 1: Убрать чтение реального времени из `Match::Process()`**

В `src/onthepitch/match.cpp` в начале `void Match::Process()` (строка ~851)
заменить:

```cpp
  unsigned long time_ms = EnvironmentManager::GetInstance().GetTime_ms() - gameSequenceInfo.startTime_ms;
  timeSincePreviousProcess_ms = time_ms - GetPreviousProcessTime_ms();
  previousProcessTime_ms = time_ms;
```

на (время продвигается самим матчем, шаг уже есть ниже: `actualTime_ms += 10`):

```cpp
  // time is advanced internally (10ms per Process call); not tied to real-time clock.
```

(Оставить `UserEventManager`/F1-блок и прочий код нетронутым.)

- [ ] **Step 2: Добавить `BumpActualTime_ms`**

В `src/onthepitch/match.hpp` объявить:

```cpp
void BumpActualTime_ms(unsigned long time);
```

В `src/onthepitch/match.cpp` (рядом с `GetMatchTime_ms`/`GetActualTime_ms`
реализациями):

```cpp
void Match::BumpActualTime_ms(unsigned long time) {
  if (IsInPlay() && !IsInSetPiece()) matchTime_ms += time * (1.0f / matchDurationFactor);
  actualTime_ms += time;
}
```

**ВНИМАНИЕ:** метод добавляем, но в `Process()` его НЕ вызываем (шаг `+= 10`
уже есть). Метод — для симметрии с GRF и будущего внешнего управления временем.

- [ ] **Step 3: Проверить, что `time_ms` в `Process` не нужен остальному коду**

Run: grep по `timeSincePreviousProcess_ms` в `src/onthepitch/match.cpp`
Expected: только объявление и присваивание (строка 289, 854) — без чтения.
Если где-то читается — разобрать перед удалением (в spec подтверждено: не читается).

- [ ] **Step 4: Собрать и проверить игру**

Run: `cmake --build . --parallel --config Release`; запустить `gameplayfootball.exe`
Expected: сборка успешна; матч играется как раньше (визуальная проверка).

- [ ] **Step 5: Commit**

```bash
git add src/onthepitch/match.cpp src/onthepitch/match.hpp
git commit -m "refactor: decouple match simulation time from real-time clock"
```

---

### Task 4: `ProcessState` для `Ball`

**Files:**
- Modify: `src/onthepitch/ball.cpp`, `src/onthepitch/ball.hpp`

**Interfaces:**
- Consumes: `EnvState` (Task 2).
- Produces: `void Ball::ProcessState(EnvState* state)` — сериализует совпадающие
  поля: `momentum`, `rotation_ms`, `predictions[]`, `orientPrediction`,
  `ballPosHistory`, `positionBuffer`, `orientationBuffer`, `ballTouchesNet`.

- [ ] **Step 1: Получить GRF-исходник**

Run: `git show origin/google-brain:src/onthepitch/ball.cpp`
Expected: содержит `void Ball::ProcessState(EnvState *state)`.

- [ ] **Step 2: Добавить `ProcessState` в `src/onthepitch/ball.cpp`**

Переносим метод из GRF, **без** поля `valid_predictions` (в master его нет):

```cpp
void Ball::ProcessState(EnvState *state) {
  state->process(momentum);
  state->process(rotation_ms);
  for (int x = 0; x < sizeof(predictions) / sizeof(predictions[0]); x++) {
    state->process(predictions[x]);
  }
  state->process(orientPrediction);
  int size = ballPosHistory.size();
  state->process(size);
  ballPosHistory.resize(size);
  for (auto &i : ballPosHistory) state->process(i);
  state->process(positionBuffer);
  state->process(orientationBuffer);
  state->process(ballTouchesNet);
}
```

В `src/onthepitch/ball.hpp` объявить: `void ProcessState(EnvState *state);`
(в protected-секцию рядом с `Process()`).

**ВНИМАНИЕ:** `BallSpatialInfo`/`Vector3`/`Quaternion` — POD-подобные структуры;
`state->process(T&)` через `memcpy` работает, если они тривиально копируемы.
Проверить компиляцией (Vector3/Quaternion в Blunted — тривиальные).

- [ ] **Step 3: Собрать**

Run: `cmake --build . --parallel --config Release`
Expected: компилируется (метод не вызывается — просто компилируется).

- [ ] **Step 4: Commit**

```bash
git add src/onthepitch/ball.cpp src/onthepitch/ball.hpp
git commit -m "feat: add Ball::ProcessState from GRF (matching fields only)"
```

---

### Task 5: `ProcessState` для `PlayerBase` и `Player`

**Files:**
- Modify: `src/onthepitch/player/playerbase.cpp`, `playerbase.hpp`,
  `src/onthepitch/player/player.cpp`, `player.hpp`

**Interfaces:**
- Consumes: `EnvState`, `Player::GetPosition()` (для замены сериализации humanoid).
- Produces: `void PlayerBase::ProcessState(EnvState*)`,
  `void Player::ProcessState(EnvState*)`.

**Почему:** GRF-версия вызывает `humanoid->ProcessState` и `controller->ProcessState`
— НЕ переносим (переписанный AI). Вместо них сериализуем позицию/движение через
уже существующие геттеры `PlayerBase` (`GetPosition()`, `GetMovement()`).

- [ ] **Step 1: Добавить `PlayerBase::ProcessState`**

В `src/onthepitch/player/playerbase.cpp`:

```cpp
void PlayerBase::ProcessState(EnvState *state) {
  state->process(isActive);
  state->process(lastTouchTime_ms);
  state->process(lastTouchType);
  state->process(fatigueFactorInv);
  int size = positionHistoryPerSecond.size();
  state->process(size);
  positionHistoryPerSecond.resize(size);
  for (auto &v : positionHistoryPerSecond) state->process(v);
  // вместо humanoid->ProcessState / controller->ProcessState:
  Vector3 pos = humanoid ? humanoid->GetPosition() : Vector3(0, 0, 0);
  Vector3 mov = humanoid ? humanoid->GetMovement() : Vector3(0, 0, 0);
  state->process(pos);
  state->process(mov);
}
```

В `playerbase.hpp` объявить: `void ProcessState(EnvState *state);`

**ВНИМАНИЕ:** `humanoid` — member `HumanoidBase *`; `GetPosition()`/`GetMovement()`
доступны через `humanoid` (см. `playerbase.hpp:40,44`). Сериализация позиции
покрывает результаты решений AI без сериализации внутренностей AI.

- [ ] **Step 2: Добавить `Player::ProcessState`**

В `src/onthepitch/player/player.cpp` (из GRF, без `manMarking`):

```cpp
void Player::ProcessState(EnvState *state) {
  ProcessStateBase(state);
  dynamicFormationEntry.ProcessState(state);
  state->process(hasPossession);
  state->process(hasBestPossession);
  state->process(hasUniquePossession);
  state->process(possessionDuration_ms);
  state->process(timeNeededToGetToBall_ms);
  state->process(timeNeededToGetToBall_optimistic_ms);
  state->process(timeNeededToGetToBall_previous_ms);
  state->process(triggerControlledBallCollision);
  state->process(desiredTimeToBall_ms);
  state->process(cards);
  state->process(cardEffectiveTime_ms);
}
```

**ВНИМАНИЕ:** имя базового метода — `ProcessStateBase` (в GRF так назван метод
PlayerBase). Соответственно в Step 1 назвать метод именно `ProcessStateBase`,
а `Player::ProcessState` вызывает его. `FormationEntry` — тривиальная POD
(enum + 3 Vector3, `gamedefines.hpp:249`), поэтому `dynamicFormationEntry`
сериализуем через `state->process(dynamicFormationEntry)` (memcpy), а не
через `.ProcessState()`. `TacticalPlayerSituation` — тоже POD (4 float),
можно сериализовать полями.

- [ ] **Step 3: Собрать**

Run: `cmake --build . --parallel --config Release`
Expected: компилируется.

- [ ] **Step 4: Commit**

```bash
git add src/onthepitch/player/playerbase.cpp src/onthepitch/player/playerbase.hpp src/onthepitch/player/player.cpp src/onthepitch/player/player.hpp
git commit -m "feat: add Player/PlayerBase ProcessState from GRF (matching fields, no AI internals)"
```

---

### Task 6: `ProcessState` для `Match`

**Files:**
- Modify: `src/onthepitch/match.cpp`, `src/onthepitch/match.hpp`

**Interfaces:**
- Consumes: `EnvState`, `Team::GetAllPlayers()`, `Player::ProcessState`,
  `Ball::ProcessState`, `MatchData` (целиком не сериализуем — только поля Match).
- Produces: `void Match::ProcessState(EnvState*)`.

**Почему:** ядро эталона. Без `Team::ProcessState`/`Referee::ProcessState`
(не переносим) — сериализуем только поля самого `Match` + игроков + мяча.

- [ ] **Step 1: Добавить `Match::ProcessState`**

В `src/onthepitch/match.cpp` (из GRF, без команд/рефери/mentalimages/officials):

```cpp
void Match::ProcessState(EnvState *state) {
  std::vector<Player*> players;
  for (int t = 0; t < 2; t++) {
    teams[t]->GetAllPlayers(players);
  }
  for (auto &player : players) {
    player->ProcessState(state);
  }
  ball->ProcessState(state);
  state->process(matchTime_ms);
  state->process(actualTime_ms);
  state->process(goalScoredTimer);
  state->process(matchPhase);
  state->process(inPlay);
  state->process(inSetPiece);
  state->process(goalScored);
  state->process(ballIsInGoal);
  state->process(lastGoalTeamID);
  state->process(lastGoalScorer);
  for (int &v : lastTouchTeamIDs) state->process(v);
  state->process(lastTouchTeamID);
  state->process(bestPossessionTeamID);
  state->process(designatedPossessionPlayer);
  state->process(ballRetainer);
  state->process(autoUpdateIngameCamera);
  state->process(cameraOrientation);
  state->process(cameraNodeOrientation);
}
```

В `match.hpp` объявить: `void ProcessState(EnvState *state);`

**ВНИМАНИЕ:** `lastGoalScorer`/`designatedPossessionPlayer`/`ballRetainer` —
указатели на Player. `EnvState::process(Player*&)` должен обрабатывать их как
индексы/не тривиально — проще сериализовать их **ID** (`GetID()`) вместо
сырых указателей: сериализовать `int` (playerID), при Load — не восстанавливать
(эталон только записывает). Проверить `Player::GetID()`.

- [ ] **Step 2: Собрать**

Run: `cmake --build . --parallel --config Release`
Expected: компилируется.

- [ ] **Step 3: Commit**

```bash
git add src/onthepitch/match.cpp src/onthepitch/match.hpp
git commit -m "feat: add Match::ProcessState core snapshot (match fields + players + ball)"
```

---

### Task 7: `CaptureMatchState` + SHA-1

**Files:**
- Create: `src/utils/capturestate.hpp`, `src/utils/capturestate.cpp`
- Modify: `src/sources.cmake` (добавить файлы в UTILS_SOURCES/UTILS_HEADERS)

**Interfaces:**
- Consumes: `Match::ProcessState`, `EnvState`.
- Produces: `std::string CaptureMatchState(Match* match)` — возвращает SHA-1 хэш
  состояния матча.

- [ ] **Step 1: Создать `src/utils/capturestate.hpp`**

```cpp
#ifndef _HPP_CAPTURESTATE
#define _HPP_CAPTURESTATE

#include <string>
class Match;

std::string CaptureMatchState(Match *match);

#endif
```

- [ ] **Step 2: Создать `src/utils/capturestate.cpp`**

```cpp
#include "capturestate.hpp"

#include "../defines.hpp"
#include "../onthepitch/match.hpp"
#include <boost/uuid/detail/sha1.hpp>
#include <sstream>
#include <iomanip>

std::string CaptureMatchState(Match *match) {
  EnvState state("", false);
  match->ProcessState(&state);
  std::string raw = state.GetState();

  boost::uuids::detail::sha1 sha;
  sha.process_bytes(raw.data(), raw.size());
  unsigned int digest[5];
  sha.get_digest(digest);

  std::ostringstream oss;
  for (int i = 0; i < 5; i++) {
    oss << std::hex << std::setw(8) << std::setfill('0') << digest[i];
  }
  return oss.str();
}
```

- [ ] **Step 3: Добавить в `sources.cmake`** — `capturestate.cpp` в `UTILS_SOURCES`,
  `capturestate.hpp` в `UTILS_HEADERS` (проверить текущие имена списков в файле).

- [ ] **Step 4: Собрать**

Run: `cmake --build . --parallel --config Release`
Expected: компилируется.

- [ ] **Step 5: Commit**

```bash
git add src/utils/capturestate.hpp src/utils/capturestate.cpp sources.cmake
git commit -m "feat: add CaptureMatchState SHA-1 helper"
```

---

### Task 8: Headless-раннер `determinism_runner`

**Files:**
- Create: `tools/determinism/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blunted::Initialize`, `InitGameContext`, `GetScene2D/GetScene3D`,
  `MenuTask`, `GameTask`/`Match`, `MatchData(3,8)`, `randomize`, `CaptureMatchState`.
- Produces: бинарник `determinism_runner` с `run` / `check <hash>`.

**Почему:** отдельный бинарник (решение секции 4). Раннеру нужен `Match` ctor,
который требует `GetScene3D()`, `GetMenuTask()`, pilons, Scheduler с
последовательностью "game", шрифты (MenuTask). 

- [ ] **Step 1: Выяснить минимальную инициализацию, требуемую `Match` ctor**

`Match::Match` (ctor) использует: `GetMenuTask()` (для `messageCaption`/
`scoreboard`/`radar`/`tacticsDebug` через `windowManager`), `GetScene3D()`
(AddNode), pilon-геттеры (`GetGreenDebugPilon` и др.), `GetScheduler()`
(`GetTaskSequenceInfo("game")`), грузит анимации `media/animations`,
звуковые буферы (`media/sounds/*.wav`).

Вывод: раннер должен:
1. `blunted::Initialize(*config)` — менеджеры, TTF, ResourceManagerPool;
2. `InitGameContext(*config)` — БД, GraphicsSystem, AudioSystem, сцены, pilons,
   controllers (Graphics/Audio рендер-потоки создают контексты — см. Task 9
   про mock);
3. создать `MenuTask` (для `GetMenuTask()`), зарегистрировать `GameTask`;
4. зарегистрировать Scheduler-последовательность `"game"` (имитация, см. ниже).

**Имитация game-последовательности:** `Match` читает
`GetScheduler()->GetTaskSequenceInfo("game")` (`match.cpp:285`). В `main.cpp`
последовательность регистрируется через `gameSequence` с `TaskSequence`.
Раннер регистрирует пустую последовательность с тем же именем, чтобы
`GetTaskSequenceInfo` вернул корректную структуру:
```cpp
boost::shared_ptr<TaskSequence> seq(new TaskSequence("game", 10, false));
GetScheduler()->RegisterTaskSequence(seq);
```
(проверить, что RegisterTaskSequence не требует entry count > 0 — в
`scheduler.cpp:42` требует! См. Task 9 — либо добавить заглушечный entry.)

- [ ] **Step 2: Написать `tools/determinism/main.cpp`**

```cpp
#include "blunted.hpp"
#include "main.hpp"
#include "gamecontext.hpp"
#include "data/matchdata.hpp"
#include "onthepitch/match.hpp"
#include "menu/menutask.hpp"
#include "gametask.hpp"
#include "utils/capturestate.hpp"
#include "SDL2/SDL_ttf.h"

#include <string>
#include <iostream>

using namespace blunted;

int main(int argc, char** argv) {
  std::string mode = (argc > 1) ? argv[1] : "run";
  std::string expected = (argc > 2) ? argv[2] : "";

  Properties config;
  blunted::Initialize(config);
  if (!InitGameContext(config)) return 1;

  // шрифт + MenuTask (аналогично main.cpp:411-420)
  std::string fontFile = config.Get("font_filename", "media/fonts/alegreya/AlegreyaSansSC-ExtraBold.ttf");
  TTF_Font *font = TTF_OpenFont(fontFile.c_str(), 32);
  TTF_Font *outlineFont = TTF_OpenFont(fontFile.c_str(), 32);
  if (!font) { Log(e_FatalError, "determinism", "main", "Could not load font"); return 1; }
  TTF_SetFontOutline(outlineFont, 2);

  menuTask = boost::shared_ptr<MenuTask>(new MenuTask(5.0f / 4.0f, 0, font, outlineFont));
  gameTask = boost::shared_ptr<GameTask>(new GameTask());

  // регистрация "game" последовательности (см. Task 9 про заглушку)
  // GetScheduler()->RegisterTaskSequence(...);

  // команды из дефолтного матча (menutask.cpp:118-119)
  MatchData *matchData = new MatchData(3, 8);

  randomize(42);
  Match *match = new Match(matchData, GetControllers());

  int steps = 5000;
  for (int i = 0; i < steps; i++) {
    match->Process();
  }

  std::string hash = CaptureMatchState(match);

  if (mode == "run") {
    std::cout << hash << std::endl;
  } else if (mode == "check") {
    if (hash == expected) { std::cout << "OK " << hash << std::endl; return 0; }
    else { std::cout << "FAIL got=" << hash << " expected=" << expected << std::endl; return 1; }
  }

  delete match;
  delete matchData;
  return 0;
}
```

**ВНИМАНИЕ:** `menuTask`/`gameTask` — глобальные переменные в `gamecontext.cpp`,
объявлены там как `boost::shared_ptr<MenuTask> menuTask;` и т.д. (Task 1). Если
не экспортируются — добавить сеттеры/сделать доступ через геттеры.

- [ ] **Step 3: Добавить CMake-таргет**

В `CMakeLists.txt`:

```cmake
add_executable(determinism_runner tools/determinism/main.cpp)
target_link_libraries(determinism_runner ${LIBRARIES})
```

(переиспользуем `${LIBRARIES}` из основного таргета; `gamecontext.cpp` должен
быть в составе общей библиотеки `gamelib`/`blunted2` или добавлен к
`determinism_runner` напрямую — проверить структуру `sources.cmake`.)

- [ ] **Step 4: Собрать и запустить `run`**

Run (из каталога сборки с копией `data/`):
`.\Release\determinism_runner.exe run`
Expected: печатает 40-символьный SHA-1 хэш без ошибок.

- [ ] **Step 5: Проверить детерминизм (два прогона → одинаковый хэш)**

Run: дважды `.\Release\determinism_runner.exe run`
Expected: оба вывода одинаковы.

- [ ] **Step 6: Commit**

```bash
git add tools/determinism/main.cpp CMakeLists.txt
git commit -m "feat: add determinism_runner tool (run/check modes)"
```

---

### Task 9: Mock-рендерер без окна + заглушка game-последовательности

**Files:**
- Modify: `src/systems/graphics/rendering/interface_renderer3d.hpp` (или создать
  `src/systems/graphics/rendering/mock_renderer3d.hpp/.cpp`), `graphics_system.cpp`,
  `tools/determinism/main.cpp`

**Interfaces:**
- Consumes: `Renderer3D` интерфейс (`interface_renderer3d.hpp`).
- Produces: `MockRenderer3D` — заглушка без окна/контекста, выбирается в
  `GraphicsSystem::Initialize` при флаге.

**Почему:** `InitGameContext` (Task 1) создаёт `GraphicsSystem`, который открывает
окно и GL-контекст (`graphics_system.cpp` — всегда `OpenGLRenderer3D`). Раннеру
окно не нужно. GRF решает это `MockRenderer3D` (`render=false`).

- [ ] **Step 1: Изучить интерфейс `Renderer3D`**

Run: `Get-Content src\systems\graphics\rendering\interface_renderer3d.hpp`
Expected: абстрактный класс `Renderer3D` с виртуальными методами (CreateContext,
Render, SetContext...).

- [ ] **Step 2: Добавить `MockRenderer3D`**

В `src/systems/graphics/rendering/interface_renderer3d.hpp` (или отдельный файл)
реализовать `class MockRenderer3D : public Renderer3D` — все виртуальные методы
возвращают `true`/no-op (по образцу GRF `interface_renderer3d.hpp`).

- [ ] **Step 3: В `GraphicsSystem::Initialize` выбирать рендерер по конфигу**

Добавить ключ конфига `graphics3d_renderer` = `"mock"` (дефолт остаётся
`"opengl"`):

```cpp
std::string renderer = config.Get("graphics3d_renderer", "opengl");
if (renderer == "opengl") renderer3DTask = new OpenGLRenderer3D();
else if (renderer == "mock") renderer3DTask = new MockRenderer3D();
```

**ВНИМАНИЕ:** дефолт не менять — игра по-прежнему использует OpenGL. Раннер
запускается с `football.config`, где `graphics3d_renderer=mock`, ИЛИ вызывает
`InitGameContext` с модифицированным конфигом (установить ключ перед вызовом).

- [ ] **Step 4: Заглушка game-последовательности**

`RegisterTaskSequence` падает при `GetEntryCount() == 0` (`scheduler.cpp:42`).
Добавить минимальный entry. Проверить `TaskSequence::AddUserTaskEntry` и
создать sequence с одной заглушкой (или найти, как это делает `main.cpp`
— там добавляются реальные tasks; раннеру достаточно последовательности с
любым одним entry, чтобы `GetTaskSequenceInfo` работал).

- [ ] **Step 5: Собрать и запустить `run` из чистого окружения**

Run: `.\Release\determinism_runner.exe run`
Expected: хэш печатается БЕЗ открытия окна и без GUI-зависаний.

- [ ] **Step 6: Commit**

```bash
git add src/systems/graphics/rendering/interface_renderer3d.hpp src/systems/graphics/rendering/mock_renderer3d.cpp src/systems/graphics/graphics_system.cpp tools/determinism/main.cpp
git commit -m "feat: mock renderer for headless determinism runner"
```

---

### Task 10: Эталон и финальная проверка

**Files:**
- Create: `tools/determinism/reference.txt`

**Interfaces:**
- Consumes: `determinism_runner run`.
- Produces: закоммиченный эталонный хэш.

- [ ] **Step 1: Сгенерировать эталон**

Run: `.\Release\determinism_runner.exe run > tools\determinism\reference.txt`
Expected: файл содержит одну строку — 40-символьный SHA-1.

- [ ] **Step 2: Проверить `check`**

Run: `.\Release\determinism_runner.exe check (Get-Content tools\determinism\reference.txt)`
Expected: `OK <hash>`, код выхода 0.

- [ ] **Step 3: Проверить отрицательный случай (другой сид → другой хэш)**

Временно поменять сид 42 → 43 в `tools/determinism/main.cpp`, пересобрать,
запустить `run` — хэш должен отличаться. Вернуть сид 42.

- [ ] **Step 4: Обновить вики**

- `docs/wiki/архитектура.md`: добавить раздел о `determinism_runner` и вынесенном
  `gamecontext`.
- `docs/wiki/открытые-вопросы.md`: пункт «тестов нет» — добавить, что появился
  детерминированный эталон (регрессионный, но не полноценные тесты).

- [ ] **Step 5: Запись в `log.md`**

Дописать в конец:
```
## [2026-08-09] feat | Детерминированный headless-раннер (ворота №1)
Перенесены из google-brain: EnvState, отвязка времени Match::Process,
randomize(seed), ProcessState (ядро Match/Ball/Player, только совпадающие поля).
Вынесен игровой контекст в src/gamecontext.*. Добавлен tools/determinism с
режимами run/check и эталоном reference.txt. Геймплей не изменён.
```

- [ ] **Step 6: Commit**

```bash
git add tools/determinism/reference.txt docs/wiki/архитектура.md docs/wiki/открытые-вопросы.md log.md
git commit -m "feat: freeze determinism reference and update wiki"
```

---

## Self-Review (проверено при написании)

**1. Spec coverage:**
- Секция 1 (отвязка времени) → Task 3.
- Секция 2a (EnvState) → Task 2.
- Секция 2b (ProcessState Match/Ball/Player/PlayerBase, совпадающие поля) →
  Tasks 4-6.
- Секция 2c (CaptureMatchState + хэш) → Task 7.
- Секция 3 (сид 42, randomize) → Task 2 (randomize) + Task 8 (вызов 42).
- Секция 4 (раннер, общая инициализация, run/check, эталон) → Tasks 1, 8, 9, 10.

**2. Placeholder scan:** все шаги содержат код или явные команды. Шаги Task 8/9,
помеченные «проверить/выяснить при реализации», содержат конкретную инструкцию,
что именно выяснить и как (это реальные неопределённости кода, не «сделать
позже»).

**3. Type consistency:** `EnvState::process`, `ProcessState(EnvState*)`,
`CaptureMatchState(Match*)`, `randomize(unsigned int)` — согласованы между
задачами. Геттеры контекста — те же сигнатуры, что в текущем `main.hpp`.

**Открытые риски (сознательно, из spec):**
- `GraphicsSystem::Initialize` в master не имеет mock-режима — добавление в
  Task 9.
- `RegisterTaskSequence` требует entry > 0 — заглушка в Task 9.
- Сериализация указателей (`lastGoalScorer` и т.п.) как ID вместо raw-указателей —
  отмечено в Task 6.
