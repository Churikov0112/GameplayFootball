// Copyright 2019 Google LLC & Bastiaan Konings
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "usereventmanager.hpp"

#include "environmentmanager.hpp"

namespace blunted {

  template<> UserEventManager* Singleton<UserEventManager>::singleton = 0;

  UserEventManager::UserEventManager() {
    lastKeyTime_ms = 0;

    //SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);

    // yes, SDL starts mousebuttons at 1...
    for (int i = 1; i < 8; i++) {
      mousePressed[i] = false;
    }

    for (int j = 0; j < _JOYSTICK_MAX; j++) {
      for (int i = 0; i < _JOYSTICK_MAXBUTTONS; i++) {
        joyButtonPressed[j][i] = false;
      }
      for (int i = 0; i < _JOYSTICK_MAXAXES; i++) {
        joyAxis[j][i] = 0.0;
        joyAxisCalibration[j][i][0] = -32768.0;
        joyAxisCalibration[j][i][1] = 32767.0;
        joyAxisCalibration[j][i][2] = 0.0;
      }
      gamepad[j] = 0;
      joystickID[j] = 0;
    }
    gamepadCount = 0;

    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    RescanGamepads();
  }

  UserEventManager::~UserEventManager() {
    for (int i = 0; i < _JOYSTICK_MAX; i++) {
      if (gamepad[i]) SDL_CloseGamepad(gamepad[i]);
      gamepad[i] = 0;
    }
  }

  void UserEventManager::Exit() {
  }

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

  void UserEventManager::InputSDLEvent(const SDL_Event &event) {
    switch (event.type) {
      case SDL_EVENT_KEY_DOWN:
        keyPressedMutex.lock();
        keyPressed[event.key.key].pressTime_ms = EnvironmentManager::GetInstance().GetTime_ms();
        lastKeyTime_ms = keyPressed[event.key.key].pressTime_ms;
        keyPressedMutex.unlock();
        break;
      case SDL_EVENT_KEY_UP:
        keyPressedMutex.lock();
        keyPressed.erase(event.key.key);
        keyPressedMutex.unlock();
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        mousePressedMutex.lock();
        mousePressed[event.button.button] = true;
        mousePressedMutex.unlock();
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        mousePressedMutex.lock();
        mousePressed[event.button.button] = false;
        mousePressedMutex.unlock();
        break;
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
    }
  }

  bool UserEventManager::GetKeyboardState(SDL_Keycode code) const {
    boost::mutex::scoped_lock lock(keyPressedMutex);
    return keyPressed.count(code) > 0;
  }

  std::map<SDL_Keycode, TimedKeyPress> UserEventManager::GetKeyboardState() const {
    boost::mutex::scoped_lock lock(keyPressedMutex);
    return keyPressed;
  }

  void UserEventManager::SetKeyboardState(SDL_Keycode key, bool newState) {
      boost::mutex::scoped_lock lock(keyPressedMutex);
      if (!newState) {
          keyPressed.erase(key);
      } else {
          keyPressed[key].pressTime_ms = EnvironmentManager::GetInstance().GetTime_ms();
      }
  }

  unsigned long UserEventManager::GetLastKeyPressDiff_ms() {
    boost::mutex::scoped_lock lock(keyPressedMutex);
    return EnvironmentManager::GetInstance().GetTime_ms() - lastKeyTime_ms;
  }

  unsigned long UserEventManager::GetLastKeyPressDiff_ms(SDL_Keycode key) {
    boost::mutex::scoped_lock lock(keyPressedMutex);
    return EnvironmentManager::GetInstance().GetTime_ms() - keyPressed[key].pressTime_ms;
  }

  bool UserEventManager::GetMouseButtonState(int sdlButtonID) const {
    boost::mutex::scoped_lock lock(mousePressedMutex);
    return mousePressed[sdlButtonID];
  }

  Vector3 UserEventManager::GetMouseRelativePos() const {
    Vector3 mousePos;
    mousePos.coords[2] = 0;
    float x, y;
    SDL_GetRelativeMouseState(&x, &y);
    mousePos.coords[0] = x;
    mousePos.coords[1] = y;
    return mousePos;
  }


  bool UserEventManager::GetJoyButtonState(int joyID, int sdlJoyButtonID) const {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    return joyButtonPressed[joyID][sdlJoyButtonID];
  }

  void UserEventManager::SetJoyButtonState(int joyID, int sdlJoyButtonID, bool newState) {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    joyButtonPressed[joyID][sdlJoyButtonID] = newState;
  }

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

  float UserEventManager::GetJoystickAxis(int joyID, int axisID, bool deadzone) const {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    float value = joyAxis[joyID][axisID];
    if (deadzone && fabs(value) < 0.05f) value = 0.0f;
    return value;
  }

  float UserEventManager::GetJoystickAxisRaw(int joyID, int axisID) const {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    return joyAxis[joyID][axisID];
  }

  float UserEventManager::GetJoystickAxisCalibrationMin(int joyID, int axisID) {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    return joyAxisCalibration[joyID][axisID][0];
  }

  float UserEventManager::GetJoystickAxisCalibrationMax(int joyID, int axisID) {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    return joyAxisCalibration[joyID][axisID][1];
  }

  float UserEventManager::GetJoystickAxisCalibrationRest(int joyID, int axisID) {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    return joyAxisCalibration[joyID][axisID][2];
  }

  void UserEventManager::SetJoystickAxisCalibration(int joyID, int axisID, float min, float max, float rest) {
    boost::mutex::scoped_lock lock(joyButtonPressedMutex);
    joyAxisCalibration[joyID][axisID][0] = min;
    joyAxisCalibration[joyID][axisID][1] = max;
    joyAxisCalibration[joyID][axisID][2] = rest;

    // rest has to be within min/max range
    if (joyAxisCalibration[joyID][axisID][2] < joyAxisCalibration[joyID][axisID][0]) joyAxisCalibration[joyID][axisID][2] = joyAxisCalibration[joyID][axisID][0];
    if (joyAxisCalibration[joyID][axisID][2] > joyAxisCalibration[joyID][axisID][1]) joyAxisCalibration[joyID][axisID][2] = joyAxisCalibration[joyID][axisID][1];
    joyAxis[joyID][axisID] = rest;
  }

}
