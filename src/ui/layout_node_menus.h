#pragma once

// UI for objects in the document (graphs, tables), in one place:
//   - settings popups: graph -> layout_graph_menu.h, table -> layout_table_menu.h
//   - buttons drawn inside nodes (a table's "+" buttons): run as undo steps
// layout.cpp talks to these functions only, so a new kind of object with a
// popup needs its menu header plus one line in each function below.

#include "layout_common.h"
#include "layout_graph_menu.h"
#include "layout_state.h"
#include "layout_table_menu.h"
#include "math/GraphNode.h"
#include "math/NodeStyle.h"
#include "math/TableNode.h"

namespace layout_internal {

// Open the settings popup that fits node `id` (no-op for other nodes).
inline void OpenNodeSettingsMenu(Editor& editor,
                                 std::uint32_t id) {
    if (FindActiveNode<GraphNode>(editor, id)) {
        OpenGraphMenu(editor, id);
    } else if (FindActiveNode<TableNode>(editor, id)) {
        OpenTableMenu(editor, id);
    }
}

inline bool IsNodeMenuOpen() {
    return graphMenuOpen || tableMenuOpen;
}

// Keyboard and mouse while a popup is open (the editor gets none then).
inline void HandleNodeMenuKeys(Font* fonts) {
    HandleGraphMenuKeys(fonts);
    HandleTableMenuKeys();
}

// At the start of Layout(): clicks outside a popup, and where it goes this
// frame (which must be worked out before the Root element is opened).
inline void UpdateNodeMenus() {
    HandleGraphMenuDismiss();
    HandleTableMenuDismiss();
    UpdateGraphMenuPlacement();
    UpdateTableMenuPlacement();
}

inline void LayoutNodeMenus() {
    if (graphMenuOpen) {
        LayoutGraphMenu();
    }
    if (tableMenuOpen) {
        LayoutTableMenu();
    }
}

// After rendering: apply or close a popup; run a button clicked inside a
// node (as one undo step); open a popup for a Settings button clicked.
inline void ProcessNodeMenus(Editor& editor) {
    ProcessGraphMenu(editor);
    ProcessTableMenu();
    const auto action = node_style::TakeActionRequest();
    const auto settings = node_style::TakeSettingsRequest();
    if (IsAnyMenuOpen() || unsavedChangesOpen) {
        return;
    }
    if (action) {
        if (auto* file = dynamic_cast<FileMpp*>(editor.ActiveTab());
            file && file->EditNode(action->nodeId, [&](EditorNode& node) {
                return node.RunAction(action->action);
            })) {
            ResetEditorCursorBlink();
        }
    }
    if (settings) {
        OpenNodeSettingsMenu(editor, *settings);
    }
}
}  // namespace layout_internal
