#pragma once

// Table settings popup. A placeholder for now: it opens from the Settings
// button on a table (see layout_node_menus.h) and closes with its close
// button, Escape, or a click outside. Settings go in LayoutTableMenu; when
// there are some, give it Cancel/Apply like the graph menu
// (layout_graph_menu.h), which edits a copy and applies it with
// FileMpp::EditNode as one undo step.

#include "layout_common.h"
#include "layout_form.h"
#include "layout_state.h"
#include "layout_style.h"
#include "math/TableNode.h"

#include <optional>

namespace layout_internal {

struct TableMenuState {
    std::uint32_t nodeId = 0;
    int framesMissing = 0;
    std::optional<PopupPlacement> placement;
    bool closeRequested = false;
};
inline TableMenuState tableMenu;

inline void OpenTableMenu(Editor& editor,
                          std::uint32_t id) {
    if (!FindActiveNode<TableNode>(editor, id) || unsavedChangesOpen) {
        return;
    }
    CloseAllMenus();
    tableMenu = TableMenuState{};
    tableMenu.nodeId = id;
    tableMenuOpen = true;
}

// Keys while the popup is open; the editor gets none then.
inline void HandleTableMenuKeys() {
    if (tableMenuOpen && IsKeyPressed(KEY_ESCAPE)) {
        tableMenu.closeRequested = true;
    }
    while (GetCharPressed() > 0) {
    }
}

// Called at the start of Layout(), using last frame's hover data.
inline void HandleTableMenuDismiss() {
    if (tableMenuOpen && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !Clay_PointerOver(CLAY_ID("TableMenuPopup"))) {
        tableMenu.closeRequested = true;
    }
}

// Call before the Root element is opened (see PlacePopupNear).
inline void UpdateTableMenuPlacement() {
    if (tableMenuOpen) {
        tableMenu.placement = PlacePopupNear(
            tableMenu.nodeId, CLAY_ID("TableMenuPopup"), TABLE_MENU_WIDTH, tableMenu.framesMissing);
    }
}

inline void LayoutTableMenu() {
    if (!tableMenu.placement) {
        return;
    }
    FormPopup(CLAY_ID("TableMenuPopup"),
              "Table settings",
              *tableMenu.placement,
              TABLE_MENU_WIDTH,
              5,
              tableMenu.closeRequested,
              [] { FormHint("Add settings here"); });
}

inline void ProcessTableMenu() {
    if (tableMenuOpen && tableMenu.closeRequested) {
        tableMenuOpen = false;
        tableMenu.closeRequested = false;
    }
}
}  // namespace layout_internal
