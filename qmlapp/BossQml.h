// BossQml.h - the QML cover's serve-thread API, the counterpart of main.cpp's
// mapping. The window lifecycle (creation, screens, showing) is BossQml.cpp,
// all on the GUI thread; these calls marshal over from the serve() thread.
#ifndef BOSSKEY_BOSSQML_H
#define BOSSKEY_BOSSQML_H

#include <string>

#include "CoverController.h"

namespace bosskey {

// Everything the cover needs, read from the plugin settings at action time.
struct CoverSpec {
    std::string scene;        // logic.h's kScene* ids
    std::string exitSequence; // normalized (lowercase [a-z0-9]), non-empty
    int autoHideSec = 0;      // 0 = until dismissed
    std::string gccProject;
    bool allScreens = false;  // false: the screen with the mouse only
    bool zh = true;
};

// Shows the cover (creates the windows on first use). False with an error.
bool showCover(const CoverSpec& spec, std::string& error);

// The plugin protocol loop (main.cpp's mapping), run on the worker thread.
int runServeLoop();

// Hides the cover and stops its timers. Safe when nothing is shown.
void hideCover();

// Builds the engine, the controller and one hidden window per screen ahead of
// time (qmlmain calls it once the GUI is up), so the first cover is instant.
void precreateCover();

// True while any cover window is visible.
bool coverVisible();

// The process's one controller (created by precreateCover), for wiring its
// exitRequested signal. Null before precreateCover.
CoverController* coverController();

} // namespace bosskey
#endif // BOSSKEY_BOSSQML_H
