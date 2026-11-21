#pragma once

#include "app/CommandLine.h"

// --screenshot <png> [--screenshot-state <name>] [--screenshot-scale <s>] (SPEC 6).
//
// Renders one view headless and exits. Everything is forced to be hermetic: the
// offscreen Qt platform (unless QT_QPA_PLATFORM is already set), --fake-midi (no real
// port is ever opened), a canned library in a temporary config directory (the user's
// library is never read or written), and a fixed window size of 1920x1080 logical px.
// The PNG is 1920*s x 1080*s device pixels for scale s.
//
// The seven state names are all accepted from T3.1 on. `browse` and `browse-search` (T3.2)
// render the real BrowseView, driven through X50Link against the simulated X50 (connect,
// then send COMBI C096). The other states render the MainWindow with the DisplayStrip in
// the matching connection state until their views exist: T3.4 (settings, settings-errors)
// and T4.1 (live-8, live-6, live-disconnected) replace their entries in applyState().
namespace ScreenshotRunner {

// Sets QT_QPA_PLATFORM and QT_SCALE_FACTOR. Must run before the QApplication exists.
void prepareEnvironment(const CliOptions &options);
// Needs a QApplication. Returns a process exit code (ExitCode::Ok, or SendFailed
// when the PNG could not be written).
int run(const CliOptions &options);

} // namespace ScreenshotRunner
