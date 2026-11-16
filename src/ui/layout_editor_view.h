#pragma once

// Describes the editor area to the active file's rendering/input methods.

#include "layout_common.h"
#include "layout_state.h"

namespace layout_internal {
inline Rectangle EditorViewport() {
    const auto workspace = Clay_GetElementData(CLAY_ID("Workspace"));
    const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
    if (!workspace.found || !editor.found) {
        return {};
    }
    const float left = std::max(workspace.boundingBox.x, editor.boundingBox.x);
    const float right = std::min(workspace.boundingBox.x + workspace.boundingBox.width,
                                 editor.boundingBox.x + editor.boundingBox.width);
    return {
        left, workspace.boundingBox.y, std::max(0.0f, right - left), workspace.boundingBox.height};
}

// Describe the editor area for the active file's rendering/input methods.
inline EditorView MakeEditorView(Font* fonts) {
    EditorView view;
    view.fonts = fonts;
    const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
    if (editor.found) {
        view.content = {editor.boundingBox.x,
                        editor.boundingBox.y,
                        editor.boundingBox.width,
                        editor.boundingBox.height};
    }
    view.viewport = EditorViewport();
    view.acceptsMouse = !IsAnyMenuOpen() && !tabDrag.file;
    constexpr double CURSOR_SOLID_SECONDS = 0.7;
    constexpr double CURSOR_BLINK_SECONDS = 0.5;
    const double elapsed = GetTime() - lastCursorActivity;
    view.cursorVisible =
        elapsed < CURSOR_SOLID_SECONDS ||
        static_cast<int>((elapsed - CURSOR_SOLID_SECONDS) / CURSOR_BLINK_SECONDS) % 2 == 0;
    return view;
}
}  // namespace layout_internal
