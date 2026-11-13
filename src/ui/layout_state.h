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

    // Insert dropdown in the menu bar.
    inline bool insertMenuOpen = false;
    // Insert -> Math was clicked this frame; opened in ProcessLayoutActions.
    inline bool insertMathRequested = false;

    // Math popup (see layout_math_menu.h / layout_math_menu_items.h).
    inline bool mathMenuOpen = false;
    // Window position of the popup's top-left corner, set by OpenMathMenu.
    inline Clay_Vector2 mathMenuPosition = {0, 0};
    // Index of the math item clicked this frame (if any); run after rendering.
    inline std::size_t mathMenuItemRequested = SIZE_MAX;
    // Text typed into the popup's search bar (after the backslash).
    inline std::string mathMenuQuery;
    // Highlighted entry in the filtered list, moved with Up/Down.
    inline std::size_t mathMenuSelected = 0;

    // True while any dropdown/popup menu is open (editor mouse input is paused).
    inline bool IsAnyMenuOpen() {
        return fileMenuOpen || insertMenuOpen || mathMenuOpen;
    }

    inline void CloseAllMenus() {
        fileMenuOpen = false;
        insertMenuOpen = false;
        mathMenuOpen = false;
    }

    inline double lastCursorActivity = 0;

    inline void ResetEditorCursorBlink() {
        lastCursorActivity = GetTime();
    }
}
