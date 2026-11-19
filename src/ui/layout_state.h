#pragma once

#include "ui/TextInput.h"

// UI state shared between building the layout and processing its actions.
// Clicks only set request flags here; ProcessLayoutActions acts on them later.

#include "layout_common.h"

#include <string>
#include <vector>

namespace layout_internal {
inline bool newRequested = false;
inline bool openRequested = false;
inline bool docsRequested = false;
inline bool saveRequested = false;
inline bool undoRequested = false;
inline bool redoRequested = false;
// Edit -> Cut / Copy / Paste clicked this frame.
inline bool cutRequested = false;
inline bool copyRequested = false;
inline bool pasteRequested = false;
// Edit-menu actions (undo/redo, cut/paste) need a fresh layout before revealing the cursor.
inline bool historyActivity = false;
inline bool closeRequested = false;
// Tab index the user clicked to activate or close this frame (if any).
inline std::size_t selectTabRequested = SIZE_MAX;
inline std::size_t closeTabRequested = SIZE_MAX;
// Remember each open file's editor scroll position so switching tabs restores it.
inline std::unordered_map<const FileBase*, Clay_Vector2> tabScroll;

// Order of the tabs as shown in the tab bar. The Editor keeps its own
// order; this one only changes when the user drags a tab.
inline std::vector<const FileBase*> tabOrder;
// Tab being dragged (pressed and still held), and whether it has moved far
// enough to count as a drag rather than a click.
inline const FileBase* draggedTab = nullptr;
inline float tabDragStartX = 0;
inline bool tabDragMoved = false;
// Where along the tab the mouse grabbed it, so the drag preview keeps that spot under the mouse.
inline float tabDragGrabOffset = 0;

// Terminal panel height, changed by dragging the bar along its top edge.
inline float terminalHeight = 140;
inline bool terminalResizing = false;
inline float terminalResizeStartY = 0;
inline float terminalResizeStartHeight = 0;
inline bool fileMenuOpen = false;
inline bool editMenuOpen = false;
inline bool unsavedChangesOpen = false;
enum class CloseAction { NONE, SAVE, DISCARD, CANCEL };
inline CloseAction closeAction = CloseAction::NONE;
inline std::string closeError;

// Insert dropdown in the menu bar.
inline bool insertMenuOpen = false;
// Insert -> Math was clicked this frame; opened in ProcessLayoutActions.
inline bool insertMathRequested = false;

// Run dropdown in the menu bar.
inline bool runMenuOpen = false;
// Run -> Run was clicked this frame; handled in ProcessRunRequest.
inline bool runRequested = false;
// Lines shown in the terminal panel, e.g. the output of the last run.
inline std::vector<std::string> terminalOutput;

// Math popup (see layout_math_menu.h / layout_math_menu_items.h).
inline bool mathMenuOpen = false;
// Window position of the popup's top-left corner, set by OpenMathMenu.
inline Clay_Vector2 mathMenuPosition = {0, 0};
// Index of the math item clicked this frame (if any); run after rendering.
inline std::size_t mathMenuItemRequested = SIZE_MAX;
// The popup's search bar: what has been typed after the backslash.
inline TextInput mathMenuSearch{"MathMenuSearchInput"};
// Highlighted entry in the filtered list, moved with Up/Down.
inline std::size_t mathMenuSelected = 0;

// Settings popups for graphs and tables; the rest of their state is in
// layout_graph_menu.h and layout_table_menu.h.
inline bool graphMenuOpen = false;
inline bool tableMenuOpen = false;

// True while any dropdown/popup menu is open (editor mouse input is paused).
inline bool IsAnyMenuOpen() {
    return fileMenuOpen || editMenuOpen || insertMenuOpen || runMenuOpen || mathMenuOpen ||
           graphMenuOpen || tableMenuOpen;
}

inline void CloseAllMenus() {
    fileMenuOpen = false;
    editMenuOpen = false;
    insertMenuOpen = false;
    runMenuOpen = false;
    mathMenuOpen = false;
    graphMenuOpen = false;
    tableMenuOpen = false;
}

inline double lastCursorActivity = 0;

inline void ResetEditorCursorBlink() {
    lastCursorActivity = GetTime();
}
}  // namespace layout_internal
