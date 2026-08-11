// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "gamepad.hpp"

#include "managers/usereventmanager.hpp"
#include "base/utils.hpp"

#include "../main.hpp"

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

HIDGamepad::~HIDGamepad() {
}

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

void HIDGamepad::LoadConfig() {
  boost::mutex::scoped_lock blah(mutex);

  for (int i = 0; i < e_ControllerButton_Size; i++) {
    controllerButtonState[i] = false;
    previousControllerButtonState[i] = false;
  }

  for (int i = 0; i < _JOYSTICK_MAXAXES; i++) {
    float min = GetConfiguration()->GetReal(("input_gamepad_" + GetIdentifier() + "_calibration_" + int_to_str(i) + "_min").c_str(), -32768);
    float max = GetConfiguration()->GetReal(("input_gamepad_" + GetIdentifier() + "_calibration_" + int_to_str(i) + "_max").c_str(), 32767);
    float rest = GetConfiguration()->GetReal(("input_gamepad_" + GetIdentifier() + "_calibration_" + int_to_str(i) + "_rest").c_str(), 0);
    UserEventManager::GetInstance().SetJoystickAxisCalibration(GetGamepadID(), i, min, max, rest);
  }

  std::string gpbuttonIDs_string[14];
  for (int i = 0; i < e_ControllerButton_Size; i++) {

    // xbox controller defaults (semantic SDL3 layout)
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

    controllerMapping[i] = GetConfiguration()->GetInt(("input_gamepad_" + GetIdentifier() + "_" + int_to_str(i)).c_str(), defaultButton);
  }

  layout = (e_ControllerLayout)GetConfiguration()->GetInt(("input_gamepad_" + GetIdentifier() + "_layout").c_str(), defaultControllerLayout);
  // reserved simple-mode flag (future feature, currently unused)
  GetConfiguration()->GetBool(("input_gamepad_" + GetIdentifier() + "_simple").c_str(), defaultControllerSimpleMode);

  const std::vector<e_ControllerButton> &preset = GetPresetFunctionMapping(layout);
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    functionMapping[i] = (e_ControllerButton)GetConfiguration()->GetInt(("input_gamepad_" + GetIdentifier() + "_mapping_" + int_to_str(i)).c_str(), preset.at(i));
  }
}

void HIDGamepad::SaveConfig() {
  boost::mutex::scoped_lock blah(mutex);
  for (int i = 0; i < e_ControllerButton_Size; i++) {
    GetConfiguration()->Set(("input_gamepad_" + GetIdentifier() + "_" + int_to_str(i)).c_str(), controllerMapping[i]);
  }
  for (int i = 0; i < e_ButtonFunction_Size; i++) {
    GetConfiguration()->Set(("input_gamepad_" + GetIdentifier() + "_mapping_" + int_to_str(i)).c_str(), functionMapping[i]);
  }
  GetConfiguration()->SaveFile(GetConfigFilename());
}

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

void HIDGamepad::Process() {
  boost::mutex::scoped_lock blah(mutex);
  //printf("gamepad ID #%i\n", gamepadID);
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

bool HIDGamepad::GetButton(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock blah(mutex);
  return controllerButtonState[functionMapping[buttonFunction]] > 0.0f;
}

float HIDGamepad::GetButtonValue(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock blah(mutex);
  return controllerButtonState[functionMapping[buttonFunction]];
}

void HIDGamepad::SetButton(e_ButtonFunction buttonFunction, bool state) {
  boost::mutex::scoped_lock blah(mutex);
  controllerButtonState[functionMapping[buttonFunction]] = state;
}

bool HIDGamepad::GetPreviousButtonState(e_ButtonFunction buttonFunction) {
  boost::mutex::scoped_lock blah(mutex);
  return previousControllerButtonState[functionMapping[buttonFunction]];
}

Vector3 HIDGamepad::GetDirection() {
  Vector3 inputDirection;
  inputDirection.coords[0] -= GetButtonValue(e_ButtonFunction_Left);
  inputDirection.coords[0] += GetButtonValue(e_ButtonFunction_Right);
  inputDirection.coords[1] += GetButtonValue(e_ButtonFunction_Up);
  inputDirection.coords[1] -= GetButtonValue(e_ButtonFunction_Down);
  if (inputDirection.GetLength() < analogStickDeadzone) {
    inputDirection = Vector3(0);
  } else {
    inputDirection.Normalize(0);
  }
  return inputDirection;
}
