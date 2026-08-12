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

  const std::vector<IHIDevice*> &controllers = GetControllers();
  std::vector<SideSelection> savedSides;
  if (inGame) {
    sides = GetMenuTask()->GetControllerSetup();
    assert(sides.size() == controllers.size());
  } else {
    // restore previously chosen sides when re-entering this page (e.g. going
    // back from team select); confirmations are never kept
    savedSides = GetMenuTask()->GetControllerSetup();
  }
  for (unsigned int i = 0; i < controllers.size(); i++) {
    SideSelection side;
    side.controllerID = i;
    side.joystickID = (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) ?
                      static_cast<HIDGamepad*>(controllers.at(i))->GetJoystickID() : 0;
    if (inGame) {
      side.side = sides.at(i).side;
    } else if (savedSides.size() == controllers.size()) {
      side.side = savedSides.at(i).side;
    } else {
      side.side = 0;
      if (i == 0 && controllers.size() < 2) side.side = -1; // autoselect 1st player == team 0 (side -1)
      else if (i == 1) side.side = -1; // if more than 1 controller, we're likely to have a gamepad on id > 0, so pick this one as auto p1 instead
    }
    side.confirmed = false;
    side.controllerImage = new Gui2Image(windowManager, "image_controller" + int_to_str(i), 0, 0, 14, 10);
    this->AddView(side.controllerImage);
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
      side.controllerImage->LoadImage("media/menu/controller/controller_small.png");
    } else {
      side.controllerImage->LoadImage("media/menu/controller/keyboard_small.png");
    }
    side.controllerImage->Show();
    if (!inGame) sides.push_back(side); else sides.at(i) = side;
    delay.push_back(0);

    layoutCaption[i] = 0;
    confirmIcon[i] = 0;
    if (controllers.at(i)->GetDeviceType() == e_HIDeviceType_Gamepad) {
      HIDGamepad *gamepad = static_cast<HIDGamepad*>(controllers.at(i));
      std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
      layoutCaption[i] = new Gui2Caption(windowManager, "caption_controllerselect_layout" + int_to_str(i), 0, 0, 12, 3, "layout: " + layoutStr);
      layoutCaption[i]->SetPosition(43 + side.side * 25, 30 + i * 15); // below the controller image row
      this->AddView(layoutCaption[i]);
      layoutCaption[i]->Show();

      confirmIcon[i] = new Gui2Image(windowManager, "image_controllerselect_confirm" + int_to_str(i), 0, 0, 3, 3);
      confirmIcon[i]->SetPosition(43 + side.side * 25 + 9, 20 + i * 15 - 3); // on top of the icon, next to the controller image
      this->AddView(confirmIcon[i]);
      confirmIcon[i]->Hide();
    } else {
      // keyboard also has a confirm indicator (drawn later on confirm)
      confirmIcon[i] = new Gui2Image(windowManager, "image_controllerselect_confirm" + int_to_str(i), 0, 0, 3, 3);
      confirmIcon[i]->SetPosition(50, 17 + i * 15);
      this->AddView(confirmIcon[i]);
      confirmIcon[i]->Hide();
    }
  }

  SetImagePositions();

  this->Show();
}

ControllerSelectPage::~ControllerSelectPage() {
}

void ControllerSelectPage::SetImagePositions() {
  for (unsigned int i = 0; i < sides.size(); i++) {
    int x = 43 + sides.at(i).side * 25;
    sides.at(i).controllerImage->SetPosition(x, 20 + i * 15);
    if (layoutCaption[i]) {
      layoutCaption[i]->SetPosition(x, 30 + i * 15); // move layout label together with the icon
    }
    if (confirmIcon[i]) {
      confirmIcon[i]->SetPosition(x + 7, 17 + i * 15); // move confirm icon together with the icon
    }
  }
}

void ControllerSelectPage::ToggleLayout(int controllerID) {
  HIDGamepad *gamepad = static_cast<HIDGamepad*>(GetControllers().at(controllerID));
  e_ControllerLayout next = (gamepad->GetLayout() == e_ControllerLayout_PES) ? e_ControllerLayout_FIFA : e_ControllerLayout_PES;
  gamepad->SetLayout(next);
  std::string layoutStr = (gamepad->GetLayout() == e_ControllerLayout_PES) ? "PES" : "FIFA";
  layoutCaption[controllerID]->SetCaption("layout: " + layoutStr);
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
    // green circle
    Vector3 green(0.0f, 200.0f, 0.0f); // 0..255 color space (see Image2D::DrawRectangle/DrawLine)
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
    // white check mark (two strokes)
    img->DrawLine(Line(Vector3(cx - r * 0.5, cy, 0), Vector3(cx - r * 0.15, cy + r * 0.4, 0)), white);
    img->DrawLine(Line(Vector3(cx - r * 0.15, cy + r * 0.4, 0), Vector3(cx + r * 0.6, cy - r * 0.4, 0)), white);
    img->OnChange();
    icon->Show();
  } else {
    icon->Hide();
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
    GoBack();
  }
}

void ControllerSelectPage::Process() {
  Gui2View::Process();
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
  // A unconfirms the keyboard (only), Esc is a two-step back: unconfirm first, then leave
  if (event->GetKeyOnce(SDLK_A)) {
    if (sides.at(0).confirmed) SetConfirmed(0, false);
    return;
  }
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
