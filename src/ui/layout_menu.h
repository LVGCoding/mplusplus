#pragma once

// Menu bar (File / Edit / Insert / Run / Docs) and its dropdowns.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"

namespace layout_internal {
// One row of a menu-bar dropdown. Clicking it sets `requested` and closes
// the dropdown whose open flag is `menuOpen`.
inline void DropdownMenuItem(Clay_ElementId id,
                             Clay_String label,
                             bool enabled,
                             bool& requested,
                             bool& menuOpen) {
    CLAY(id,
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)},
                     .padding = {10, 16, 0, 0},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor =
              enabled && Clay_PointerOver(id) ? MENU_HOVER : Clay_Color{0, 0, 0, 0}}) {
        CLAY_TEXT(label,
                  {.textColor = enabled ? MENU_TEXT : Clay_Color{150, 150, 150, 255},
                   .fontId = FONT_BODY,
                   .fontSize = 18});
        if (enabled && !unsavedChangesOpen && Clay_Hovered() &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            requested = true;
            menuOpen = false;
        }
    }
}

inline void FileMenuItem(Clay_ElementId id,
                         Clay_String label,
                         bool enabled,
                         bool& requested) {
    DropdownMenuItem(id, label, enabled, requested, fileMenuOpen);
}

inline void InsertMenuItem(Clay_ElementId id,
                           Clay_String label,
                           bool enabled,
                           bool& requested) {
    DropdownMenuItem(id, label, enabled, requested, insertMenuOpen);
}

inline void LayoutMenuBar() {
    constexpr bool SHOW_MENU_CHEVRONS = false;
    CLAY(CLAY_ID("MenuBar"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                     .padding = {4, 4, 0, 0},
                     .childGap = 0,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = MENU_BAR_COLOR}) {
        const Clay_String labels[] = {CLAY_STRING("File"),
                                      CLAY_STRING("Edit"),
                                      CLAY_STRING("Insert"),
                                      CLAY_STRING("Run"),
                                      CLAY_STRING("Docs")};
        const Clay_ElementId ids[] = {CLAY_ID("FileMenu"),
                                      CLAY_ID("EditMenu"),
                                      CLAY_ID("InsertMenu"),
                                      CLAY_ID("RunMenu"),
                                      CLAY_ID("DocsMenu")};
        for (int i = 0; i < 5; ++i) {
            CLAY(ids[i],
                 {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                             .padding = {8, 8, 0, 0},
                             .childGap = 6,
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = Clay_PointerOver(ids[i]) || (i == 0 && fileMenuOpen) ||
                                             (i == 1 && editMenuOpen) ||
                                             (i == 2 && insertMenuOpen) || (i == 3 && runMenuOpen)
                                         ? MENU_HOVER
                                         : Clay_Color{0, 0, 0, 0}}) {
                CLAY_TEXT(labels[i],
                          {.textColor = MENU_TEXT,
                           .fontId = FONT_BODY,
                           .fontSize = 20,
                           .wrapMode = CLAY_TEXT_WRAP_NONE});
                if (SHOW_MENU_CHEVRONS && i < 4) {
                    CLAY(CLAY_IDI("MenuChevron", i),
                         {.layout = {.sizing = {CLAY_SIZING_FIXED(12), CLAY_SIZING_FIXED(12)}},
                          .custom = {.customData = &chevron}}) {}
                }
                if (i == 0 && !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    fileMenuOpen = !fileMenuOpen;
                }
                if (i == 1 && !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    editMenuOpen = !editMenuOpen;
                }
                if (i == 2 && !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    insertMenuOpen = !insertMenuOpen;
                }
                if (i == 3 && !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    runMenuOpen = !runMenuOpen;
                }
                if (i == 4 && !unsavedChangesOpen && Clay_Hovered() &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    docsRequested = true;
                    fileMenuOpen = false;
                }
            }
        }
    }
}

inline void LayoutFileDropdown(bool canSave) {
    CLAY(CLAY_ID("FileDropdown"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                     .padding = {0, 0, 4, 4},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = DROPDOWN_COLOR,
          .floating = {.offset = {0, 0},
                       .parentId = CLAY_ID("FileMenu").id,
                       .zIndex = 1,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
        FileMenuItem(CLAY_ID("NewButton"), CLAY_STRING("New"), true, newRequested);
        FileMenuItem(CLAY_ID("OpenButton"), CLAY_STRING("Open File"), true, openRequested);
        FileMenuItem(CLAY_ID("SaveButton"), CLAY_STRING("Save File"), canSave, saveRequested);
    }
}

inline void LayoutEditDropdown(const FileBase* file) {
    CLAY(CLAY_ID("EditDropdown"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                     .padding = {0, 0, 4, 4},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = DROPDOWN_COLOR,
          .floating = {.offset = {0, 0},
                       .parentId = CLAY_ID("EditMenu").id,
                       .zIndex = 1,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
        DropdownMenuItem(CLAY_ID("UndoButton"),
                         CLAY_STRING("Undo"),
                         file && file->CanUndo(),
                         undoRequested,
                         editMenuOpen);
        DropdownMenuItem(CLAY_ID("RedoButton"),
                         CLAY_STRING("Redo"),
                         file && file->CanRedo(),
                         redoRequested,
                         editMenuOpen);
        const bool hasSelection = file && file->HasSelection();
        const char* clipboard = GetClipboardText();
        const bool canPaste = file && clipboard && clipboard[0] != 0;
        DropdownMenuItem(
            CLAY_ID("CutButton"), CLAY_STRING("Cut"), hasSelection, cutRequested, editMenuOpen);
        DropdownMenuItem(
            CLAY_ID("CopyButton"), CLAY_STRING("Copy"), hasSelection, copyRequested, editMenuOpen);
        DropdownMenuItem(
            CLAY_ID("PasteButton"), CLAY_STRING("Paste"), canPaste, pasteRequested, editMenuOpen);
    }
}

inline void LayoutInsertDropdown() {
    CLAY(CLAY_ID("InsertDropdown"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                     .padding = {0, 0, 4, 4},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = DROPDOWN_COLOR,
          .floating = {.offset = {0, 0},
                       .parentId = CLAY_ID("InsertMenu").id,
                       .zIndex = 1,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
        // Each needs its own request flag in layout_state.h, handled in
        // ProcessLayoutActions (Math is handled in ProcessMathMenuTriggers).
        InsertMenuItem(CLAY_ID("InsertMathButton"), CLAY_STRING("Math"), true, insertMathRequested);
    }
}

// Run dropdown. Run -> Run sets runRequested and Run -> Stop sets
// stopRequested; ProcessRunRequest (layout_actions.h) handles both.
inline void LayoutRunDropdown(const FileBase* file) {
    CLAY(CLAY_ID("RunDropdown"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                     .padding = {0, 0, 4, 4},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = DROPDOWN_COLOR,
          .floating = {.offset = {0, 0},
                       .parentId = CLAY_ID("RunMenu").id,
                       .zIndex = 1,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
        DropdownMenuItem(
            CLAY_ID("RunButton"), CLAY_STRING("Run"), file != nullptr, runRequested, runMenuOpen);
        DropdownMenuItem(
            CLAY_ID("StopButton"), CLAY_STRING("Stop"), runJob.Running(), stopRequested, runMenuOpen);
    }
}
}  // namespace layout_internal
