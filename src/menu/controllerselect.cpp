// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "controllerselect.hpp"

#include "../main.hpp"

#include "mainmenu.hpp"

#include "startmatch/teamselect.hpp"

#include "pagefactory.hpp"

#include "base/geometry/line.hpp"
#include "scene/objects/image2d.hpp"

using namespace blunted;

ControllerSelectPage::ControllerSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData) : Gui2Page(windowManager, pageData) {

  inGame = pageData.properties->GetBool("isInGame");
  resumeOnClose = pageData.properties->GetBool("resumeOnClose");

  Gui2Image *bg1 = new Gui2Image(windowManager, "image_gameover_bg", 10, 15, 80, 70);
  this->AddView(bg1);
  bg1->LoadImage("media/menu/backgrounds/black.png");
  bg1->Show();

  Gui2Caption *t1 = new Gui2Caption(windowManager, "caption_controllerselect_t1", 0, 0, 28, 3, "Team 1");
  Gui2Caption *t2 = new Gui2Caption(windowManager, "caption_controllerselect_t2", 0, 0, 28, 3, "Team 2");

  t1->SetPosition(25 - t1->GetTextWidthPercent() * 0.5, 10);
  t2->SetPosition(75 - t2->GetTextWidthPercent() * 0.5, 10);

  this->AddView(t1);
  t1->Show();
  this->AddView(t2);
  t2->Show();

  this->SetFocus();

  BuildDeviceViews(GetMenuTask()->GetControllerSetup());

  SetImagePositions();

  this->Show();
}

ControllerSelectPage::~ControllerSelectPage() {
}

void ControllerSelectPage::BuildDeviceViews(const std::vector<SideSelection> &savedSides) {
  // copy before sides.clear() below: savedSides may reference the same vector
  // (Process() passes this->sides), and clearing it would drop all chosen sides
  std::vector<SideSelection> savedSidesCopy = savedSides;

  // remove previously created device rows (if any)
  for (unsigned int i = 0; i < deviceViews.size(); i++) {
    deviceViews.at(i)->Exit();
    delete deviceViews.at(i);
  }
  deviceViews.clear();
  sides.clear();
  delay.clear();
  for (int i = 0; i < _JOYSTICK_MAX; i++) {
    layoutCaption[i] = 0;
    confirmIcon[i] = 0;
  }

  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 0; i < controllers.size(); i++) {
    SideSelection side;
    side.controllerID = i;
    side.joystickID = (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) ?
                      static_cast<HIDGamepad*>(controllers.at(i))->GetJoystickID() : 0;
    side.side = 0;
    // restore side for devices still connected (match by joystickID; keyboard == 0)
    for (unsigned int s = 0; s < savedSidesCopy.size(); s++) {
      if (savedSidesCopy.at(s).joystickID == side.joystickID) { side.side = savedSidesCopy.at(s).side; break; }
    }
    if (!inGame && savedSidesCopy.size() == 0) {
      if (i == 0 && controllers.size() < 2) side.side = -1; // autoselect 1st player == team 0 (side -1)
      else if (i == 1) side.side = -1; // if more than 1 controller, we're likely to have a gamepad on id > 0, so pick this one as auto p1 instead
    }
    side.confirmed = false;
    side.controllerImage = new Gui2Image(windowManager, "image_controller" + int_to_str(i), 0, 0, 14, 10);
    this->AddView(side.controllerImage);
    deviceViews.push_back(side.controllerImage);
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
      side.controllerImage->LoadImage("media/menu/controller/controller_small.png");
    } else {
      side.controllerImage->LoadImage("media/menu/controller/keyboard_small.png");
    }
    side.controllerImage->Show();
    sides.push_back(side);
    delay.push_back(0);

    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
      HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(i));
      std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
      layoutCaption[i] = new Gui2Caption(windowManager, "caption_controllerselect_layout" + int_to_str(i), 0, 0, 12, 3, "LAYOUT: " + layoutStr);
      layoutCaption[i]->SetPosition(43 + side.side * 25 + 7 - layoutCaption[i]->GetTextWidthPercent() * 0.5, 31 + i * 15); // centered under the controller image
      this->AddView(layoutCaption[i]);
      deviceViews.push_back(layoutCaption[i]);
      layoutCaption[i]->Show();

      confirmIcon[i] = new Gui2Image(windowManager, "image_controllerselect_confirm" + int_to_str(i), 0, 0, 3, 3);
      confirmIcon[i]->SetPosition(43 + side.side * 25 + 5, 34 + i * 15); // under the layout label
      this->AddView(confirmIcon[i]);
      deviceViews.push_back(confirmIcon[i]);
      confirmIcon[i]->Hide();
    } else {
      // keyboard: confirm indicator centered under the keyboard image
      confirmIcon[i] = new Gui2Image(windowManager, "image_controllerselect_confirm" + int_to_str(i), 0, 0, 3, 3);
      confirmIcon[i]->SetPosition(43 + side.side * 25 + 5, 31 + i * 15);
      this->AddView(confirmIcon[i]);
      deviceViews.push_back(confirmIcon[i]);
      confirmIcon[i]->Hide();
    }
  }
}

void ControllerSelectPage::SetImagePositions() {
  for (unsigned int i = 0; i < sides.size(); i++) {
    int x = 43 + sides.at(i).side * 25;
    sides.at(i).controllerImage->SetPosition(x, 20 + i * 15);
    bool isGamepad = (GetControllers().at(i)->GetDeviceType() == e_HIDeviceType_Gamepad);
    if (layoutCaption[i]) {
      // centered under the controller image
      layoutCaption[i]->SetPosition(x + 7 - layoutCaption[i]->GetTextWidthPercent() * 0.5, 31 + i * 15);
    }
    if (confirmIcon[i]) {
      if (isGamepad) confirmIcon[i]->SetPosition(x + 5, 34 + i * 15); // under the layout label
      else confirmIcon[i]->SetPosition(x + 5, 31 + i * 15);           // centered under the keyboard image
    }
  }
}

void ControllerSelectPage::ToggleLayout(int controllerID) {
  HIDGamepad *gamepad = static_cast<HIDGamepad*>(GetControllers().at(controllerID));
  e_ControllerLayout next = (gamepad->GetLayout() == e_ControllerLayout_PES) ? e_ControllerLayout_FIFA : e_ControllerLayout_PES;
  gamepad->SetLayout(next);
  std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
  layoutCaption[controllerID]->SetCaption("LAYOUT: " + layoutStr);
  SetImagePositions();
}

void ControllerSelectPage::SetConfirmed(int controllerID, bool confirmed) {
  sides.at(controllerID).confirmed = confirmed;
  Gui2Image *icon = confirmIcon[controllerID];
  if (!icon) return;
  if (confirmed) {
    // draw a green circle with a white check mark
    boost::intrusive_ptr<Image2D> img = icon->GetImage2D();
    int w = int(round(img->GetSize().coords[0]));
    int h = int(round(img->GetSize().coords[1]));
    // clear
    img->DrawRectangle(0, 0, w, h, Vector3(0, 0, 0), 0);
    // green circle (filled disc)
    Vector3 green(0.0f, 200.0f, 0.0f); // 0..255 color space (see Image2D::DrawRectangle)
    Vector3 white(255.0f, 255.0f, 255.0f);
    int cx = w / 2;
    int cy = h / 2;
    int r = (w < h ? w : h) / 2 - 1;
    for (int y = 0; y < h; y++) {
      int dy = y - cy;
      int d = r * r - dy * dy;
      if (d >= 0) {
        int halfW = (int)floor(sqrt((real)d));
        img->DrawRectangle(cx - halfW, y, halfW * 2, 1, green);
      }
    }
    // white check mark. sdl_line is a stub (SGE removed), so draw lines manually.
    int x0 = cx - int(r * 0.5);
    int y0 = cy;
    int x1 = cx - int(r * 0.1);
    int y1 = cy + int(r * 0.4);
    int x2 = cx + int(r * 0.6);
    int y2 = cy - int(r * 0.4);
    DrawPixelLine(img, x0, y0, x1, y1, white);
    DrawPixelLine(img, x1, y1, x2, y2, white);
    img->OnChange();
    icon->Show();
  } else {
    icon->Hide();
  }
}

void ControllerSelectPage::DrawPixelLine(boost::intrusive_ptr<Image2D> img, int x0, int y0, int x1, int y1, const Vector3 &color) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    img->PutPixel(x0, y0, color);
    img->PutPixel(x0 + 1, y0, color);
    img->PutPixel(x0, y0 + 1, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void ControllerSelectPage::CheckAllConfirmed() {
  // only devices that picked a side (side != 0) need to confirm; center ones are ignored
  for (unsigned int i = 0; i < sides.size(); i++) {
    if (sides.at(i).side != 0 && !sides.at(i).confirmed) return;
  }
  GetMenuTask()->SetControllerSetup(sides);
  if (!inGame) {
    CreatePage(e_PageID_TeamSelect);
  } else {
    GetGameTask()->GetMatch()->UpdateControllerSetup();
    if (resumeOnClose) GetGameTask()->GetMatch()->Pause(false); // resume the match we paused on unplug
    GoBack();
  }
}

void ControllerSelectPage::Process() {
  Gui2View::Process();

  // hot-plug: if the device set changed while this window is open, rebuild the
  // device rows in place. Recreating the page (CreatePage) would push stale
  // entries onto the page path and break GoBack, so we rebuild contents instead.
  if (sides.size() != GetControllers().size()) {
    BuildDeviceViews(sides);
    SetImagePositions();
  }
}

void ControllerSelectPage::ProcessKeyboardEvent(KeyboardEvent *event) {
  HIDKeyboard *keyboard = static_cast<HIDKeyboard*>(GetControllers().at(0));
  if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_Left))) {
    sides.at(0).side -= 1;
  }
  if (event->GetKeyOnce(keyboard->GetFunctionMapping(e_ButtonFunction_Right))) {
    sides.at(0).side += 1;
  }
  sides.at(0).side = clamp(sides.at(0).side, -1, 1);

  // confirm with Enter
  if (event->GetKeyOnce(SDLK_RETURN)) {
    SetConfirmed(0, true);
    CheckAllConfirmed();
    return;
  }
  // Esc is a two-step back: unconfirm first, then leave
  if (event->GetKeyOnce(SDLK_ESCAPE)) {
    if (sides.at(0).confirmed) {
      SetConfirmed(0, false);
    } else {
      ExitControllerSelectPage();
    }
    return;
  }

  SetImagePositions();
}

void ControllerSelectPage::ProcessJoystickEvent(JoystickEvent *event) {
  const std::vector<IHIDevice*> &controllers = GetControllers();
  for (unsigned int i = 1; i < controllers.size(); i++) {
    HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(i));
    int joyID = gamepad->GetGamepadID();

    // back button (B): two-step — unconfirm first, then leave
    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_B))) {
      if (sides.at(i).confirmed) {
        SetConfirmed(i, false);
      } else {
        ExitControllerSelectPage();
        return;
      }
      continue;
    }

    if (delay.at(i) < EnvironmentManager::GetInstance().GetTime_ms() - 250) {
      if (gamepad->GetButtonValue(e_ButtonFunction_Left) > 0.5) {
        sides.at(i).side -= 1;
        sides.at(i).confirmed = false; // side changed, needs re-confirmation
        SetConfirmed(i, false);
        delay.at(i) = EnvironmentManager::GetInstance().GetTime_ms();
      }
      if (gamepad->GetButtonValue(e_ButtonFunction_Right) > 0.5) {
        sides.at(i).side += 1;
        sides.at(i).confirmed = false; // side changed, needs re-confirmation
        SetConfirmed(i, false);
        delay.at(i) = EnvironmentManager::GetInstance().GetTime_ms();
      }
      sides.at(i).side = clamp(sides.at(i).side, -1, 1);
    }

    // layout preset toggle: LB or RB cycles PES <-> FIFA
    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_L1)) ||
        event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_R1))) {
      ToggleLayout(i);
    }

    // confirm with A
    if (event->GetButton(joyID, gamepad->GetControllerMapping(e_ControllerButton_A))) {
      SetConfirmed(i, true);
      CheckAllConfirmed();
    }
  }

  SetImagePositions();
}

void ControllerSelectPage::ExitControllerSelectPage() {
  // remember chosen sides (without confirmations) so a later re-entry keeps them
  for (unsigned int i = 0; i < sides.size(); i++) {
    sides.at(i).confirmed = false;
  }
  GetMenuTask()->SetControllerSetup(sides);
  if (inGame) {
    GetGameTask()->GetMatch()->UpdateControllerSetup();
    if (resumeOnClose) GetGameTask()->GetMatch()->Pause(false); // resume the match we paused on unplug
  }
  GoBack();
}

void ControllerSelectPage::ProcessWindowingEvent(WindowingEvent *event) {
  // Activate (button A / Enter) no longer opens TeamSelect directly;
  // confirmation happens per-device in ProcessJoystickEvent/ProcessKeyboardEvent,
  // and CheckAllConfirmed() transitions once every side-picked device confirmed.
  // Escape is handled per-device too (two-step: unconfirm, then leave), so the
  // generic windowing escape must not go back on its own.
  event->Ignore();
}
