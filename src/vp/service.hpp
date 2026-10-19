// The service menu: save/settings.ini, changed on the machine. Opened and
// cancelled with F2. Laid out and worked as the Tempest 2000 port's: the
// spinner (or up / down) goes through the rows, FOCUS (or START 1) takes a
// row, the spinner (or left / right) sets it, FOCUS lets it go again;
// left / right set a row without taking it. Nothing is taken over until
// SAVE. The controls and the picture have pages of their own.
#pragma once

#include "settings.hpp"

typedef struct vp_input vp_input;
typedef struct vp_dvg vp_dvg;

namespace hg {
class Audio;
}

namespace vp {

enum
{
    SERVICE_OPEN = 0,
    SERVICE_SAVED,
    SERVICE_CANCELLED
};

// Once every step, open or not: true when F2 has just been pressed.
bool serviceHotkey();

void serviceOpen(const Settings& now, hg::Audio* audio);

// One step; `hotkey` is serviceHotkey's answer (F2 again cancels).
int serviceUpdate(vp_input* in, bool hotkey);

// The settings as they stand in the menu (take them on SERVICE_SAVED).
const Settings& serviceGet();

void serviceDraw(vp_dvg* d);

} // namespace vp
