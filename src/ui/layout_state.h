#pragma once

// UI state shared between building the layout and processing its actions.
// Clicks only set request flags here; ProcessLayoutActions acts on them later.

#include "layout_common.h"

namespace layout_internal {
    inline bool newRequested = false;
    inline bool openRequested = false;
    inline bool docsRequested = false;
    inline bool saveRequested = false;
    inline bool closeRequested = false;
    // Tab index the user clicked to activate or close this frame (if any).
    inline std::size_t selectTabRequested = SIZE_MAX;
    inline std::size_t closeTabRequested = SIZE_MAX;
    // Remember each open file's editor scroll position so switching tabs restores it.
    inline std::unordered_map<const FileBase*, Clay_Vector2> tabScroll;
    inline bool fileMenuOpen = false;
    inline bool unsavedChangesOpen = false;
    enum class CloseAction { None, Save, Discard, Cancel };
    inline CloseAction closeAction = CloseAction::None;
    inline std::string closeError;

    inline double lastCursorActivity = 0;

    inline void ResetEditorCursorBlink() {
        lastCursorActivity = GetTime();
    }
}
