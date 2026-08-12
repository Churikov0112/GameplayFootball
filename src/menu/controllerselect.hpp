// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#ifndef _HPP_MENU_CONTROLLERSELECT
#define _HPP_MENU_CONTROLLERSELECT

#include "utils/gui2/windowmanager.hpp"

#include "utils/gui2/widgets/menu.hpp"
#include "utils/gui2/widgets/root.hpp"
#include "utils/gui2/widgets/grid.hpp"
#include "utils/gui2/widgets/button.hpp"
#include "utils/gui2/widgets/caption.hpp"
#include "utils/gui2/widgets/image.hpp"

#include "hid/gamepad.hpp"

#include "../onthepitch/match.hpp"

using namespace blunted;

class ControllerSelectPage : public Gui2Page {

  public:
    ControllerSelectPage(Gui2WindowManager *windowManager, const Gui2PageData &pageData);
    virtual ~ControllerSelectPage();

    void SetImagePositions();
    void ToggleLayout(int controllerID);
    void SetConfirmed(int controllerID, bool confirmed);
    void CheckAllConfirmed();
    void ExitControllerSelectPage();
    void BuildDeviceViews(const std::vector<SideSelection> &savedSides);

    virtual void Process();
    virtual void ProcessKeyboardEvent(KeyboardEvent *event);
    virtual void ProcessJoystickEvent(JoystickEvent *event);
    virtual void ProcessWindowingEvent(WindowingEvent *event);

  protected:
    std::vector<SideSelection> sides;
    std::vector<unsigned long> delay;
    bool inGame;
    bool resumeOnClose = false; // resume the paused match when this window closes (opened from the match, not from the pause menu)

    Gui2Caption *layoutCaption[_JOYSTICK_MAX];
    Gui2Image *confirmIcon[_JOYSTICK_MAX];
    std::vector<Gui2View*> deviceViews; // device rows created by BuildDeviceViews (controller images, layout labels, confirm icons)

};

#endif
