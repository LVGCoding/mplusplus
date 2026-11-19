#pragma once

// Graph settings popup: the x and y axis ranges of one graph. Everything else
// (title, axis labels, functions) is edited in the graph itself.
//   - Opens from the Settings button on a graph (see layout_node_menus.h).
//   - Edits a copy; Apply (or Enter, or clicking outside) writes it to the
//     graph as one undo step, Cancel (or Escape) drops it.
// The graph is found again by its node id (see FileMpp::FindNode), so the
// popup never holds a pointer into the document.

#include "layout_common.h"
#include "layout_form.h"
#include "layout_state.h"
#include "layout_style.h"
#include "math/GraphNode.h"
#include "math/GraphSettings.h"
#include "math/Options.h"

#include <optional>
#include <string>
#include <vector>

namespace layout_internal {

struct GraphMenuState {
    std::uint32_t nodeId = 0;
    // The settings when the popup opened; unknown options are carried over from here.
    GraphSettings original;

    FormField xMin{"GraphMenuXMin", FormField::Kind::NUMBER};
    FormField xMax{"GraphMenuXMax", FormField::Kind::NUMBER};
    FormField yMin{"GraphMenuYMin", FormField::Kind::NUMBER};
    FormField yMax{"GraphMenuYMax", FormField::Kind::NUMBER};

    FormField* focused = &xMin;
    // Why the last Apply was refused; shown above the buttons.
    std::string error;
    int framesMissing = 0;
    std::optional<PopupPlacement> placement;

    // Set by clicks and keys; acted on in ProcessGraphMenu, after rendering.
    bool applyRequested = false;
    bool cancelRequested = false;
};
inline GraphMenuState graphMenu;

// The fields in Tab order.
inline std::vector<FormField*> GraphMenuFields() {
    GraphMenuState& m = graphMenu;
    return {&m.xMin, &m.xMax, &m.yMin, &m.yMax};
}

inline void OpenGraphMenu(Editor& editor,
                          std::uint32_t id) {
    const GraphNode* graph = FindActiveNode<GraphNode>(editor, id);
    if (!graph || unsavedChangesOpen) {
        return;
    }
    CloseAllMenus();
    GraphMenuState& m = graphMenu;
    const GraphSettings& s = graph->Settings();
    m.nodeId = id;
    m.original = s;
    m.xMin.SetText(options::FormatNumber(s.xMin));
    m.xMax.SetText(options::FormatNumber(s.xMax));
    m.yMin.SetText(options::FormatNumber(s.yMin));
    m.yMax.SetText(options::FormatNumber(s.yMax));
    for (FormField* field : GraphMenuFields()) {
        field->invalid = false;
    }
    m.focused = &m.xMin;
    m.error.clear();
    m.framesMissing = 0;
    m.placement.reset();
    m.applyRequested = m.cancelRequested = false;
    graphMenuOpen = true;
}

inline void CloseGraphMenu() {
    graphMenuOpen = false;
    graphMenu.applyRequested = graphMenu.cancelRequested = false;
}

// The settings in the form, or nothing (with fields marked and `error` set)
// if something can't be used.
inline std::optional<GraphSettings> ReadGraphMenu() {
    GraphMenuState& m = graphMenu;
    m.error.clear();
    GraphSettings s = m.original;
    auto fail = [&](FormField& field, const char* message) {
        field.invalid = true;
        if (m.error.empty()) {
            m.error = message;
        }
    };
    auto range = [&](FormField& minField,
                     FormField& maxField,
                     double& low,
                     double& high,
                     const char* message) {
        if (!options::ParseNumber(minField.Text(), low)) {
            fail(minField, "Enter a number in the highlighted field.");
        }
        if (!options::ParseNumber(maxField.Text(), high)) {
            fail(maxField, "Enter a number in the highlighted field.");
        }
        if (!minField.invalid && !maxField.invalid && low >= high) {
            fail(minField, message);
            fail(maxField, "");
        }
    };
    range(m.xMin, m.xMax, s.xMin, s.xMax, "The x axis must go from a smaller to a larger number.");
    range(m.yMin, m.yMax, s.yMin, s.yMax, "The y axis must go from a smaller to a larger number.");
    if (!m.error.empty()) {
        return std::nullopt;
    }
    return s;
}

// --- Input (before the layout) ----------------------------------------------------------

// Keys and clicks while the popup is open; the editor gets no input then.
inline void HandleGraphMenuKeys(Font* fonts) {
    if (!graphMenuOpen) {
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        graphMenu.cancelRequested = true;
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        graphMenu.applyRequested = true;
        return;
    }
    if (FormHandleKeys(GraphMenuFields(), graphMenu.focused, fonts)) {
        graphMenu.error.clear();
    }
}

// A click outside the popup applies and closes it, like clicking away from
// a popover. Called at the start of Layout(), using last frame's hover data.
inline void HandleGraphMenuDismiss() {
    if (graphMenuOpen && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !Clay_PointerOver(CLAY_ID("GraphMenuPopup"))) {
        graphMenu.applyRequested = true;
    }
}

// --- Layout -----------------------------------------------------------------------------

// Call before the Root element is opened (see PlacePopupNear).
inline void UpdateGraphMenuPlacement() {
    if (graphMenuOpen) {
        graphMenu.placement = PlacePopupNear(
            graphMenu.nodeId, CLAY_ID("GraphMenuPopup"), GRAPH_MENU_WIDTH, graphMenu.framesMissing);
    }
}

inline void LayoutGraphMenu() {
    GraphMenuState& m = graphMenu;
    if (!m.placement) {
        return;
    }
    constexpr int16_t Z = 5;
    auto input = [&](FormField& field) { FormInput(field, m.focused == &field, Z + 1); };

    FormPopup(CLAY_ID("GraphMenuPopup"),
              "Graph settings",
              *m.placement,
              GRAPH_MENU_WIDTH,
              Z,
              m.cancelRequested,
              [&] {
                  FormRow("X axis", [&] {
                      input(m.xMin);
                      FormJoiner("to");
                      input(m.xMax);
                  });
                  FormRow("Y axis", [&] {
                      input(m.yMin);
                      FormJoiner("to");
                      input(m.yMax);
                  });
                  FormHint("Add settings here");
                  if (!m.error.empty()) {
                      FormError(m.error);
                  }
                  FormDialogButtons(
                      CLAY_ID("GraphMenuPopup").id, m.cancelRequested, m.applyRequested);
              });
}

// --- Actions (after rendering) ------------------------------------------------------------

inline void ProcessGraphMenu(Editor& editor) {
    if (!graphMenuOpen) {
        return;
    }
    if (graphMenu.cancelRequested) {
        CloseGraphMenu();
        return;
    }
    if (!graphMenu.applyRequested) {
        return;
    }
    graphMenu.applyRequested = false;
    // Stays open, with the problem shown, if the form can't be used.
    const auto settings = ReadGraphMenu();
    if (!settings) {
        return;
    }
    if (auto* file = dynamic_cast<FileMpp*>(editor.ActiveFile())) {
        file->EditNode(graphMenu.nodeId, [&](EditorNode& node) {
            auto* graph = dynamic_cast<GraphNode*>(&node);
            if (!graph || graph->Settings() == *settings) {
                return false;
            }
            graph->SetSettings(*settings);
            return true;
        });
    }
    CloseGraphMenu();
    ResetEditorCursorBlink();
}
}  // namespace layout_internal
