#pragma once

// Menu bar (File / Edit / Insert / Run / Docs) and the File dropdown.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"

namespace layout_internal {
    // One row of a menu-bar dropdown. Clicking it sets `requested` and closes
    // the dropdown whose open flag is `menuOpen`.
    inline void DropdownMenuItem(Clay_ElementId id, Clay_String label, bool enabled,
                                 bool& requested, bool& menuOpen) {
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)},
                         .padding = {10, 16, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = enabled && Clay_PointerOver(id)
                  ? menuHover : Clay_Color{0, 0, 0, 0}}) {
            CLAY_TEXT(label, {.textColor = enabled ? menuText : Clay_Color{150, 150, 150, 255},
                              .fontId = FONT_BODY, .fontSize = 18});
            if (enabled && !unsavedChangesOpen && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                requested = true;
                menuOpen = false;
            }
        }
    }

    inline void FileMenuItem(Clay_ElementId id, Clay_String label, bool enabled,
                             bool& requested) {
        DropdownMenuItem(id, label, enabled, requested, fileMenuOpen);
    }

    inline void InsertMenuItem(Clay_ElementId id, Clay_String label, bool enabled,
                               bool& requested) {
        DropdownMenuItem(id, label, enabled, requested, insertMenuOpen);
    }

    inline void LayoutMenuBar() {
        CLAY(CLAY_ID("MenuBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {4, 4, 0, 0}, .childGap = 0,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = menuBarColor}) {
            const Clay_String labels[] = {CLAY_STRING("File"), CLAY_STRING("Edit"),
                                          CLAY_STRING("Insert"), CLAY_STRING("Run"),
                                          CLAY_STRING("Docs")};
            const Clay_ElementId ids[] = {CLAY_ID("FileMenu"), CLAY_ID("EditMenu"),
                                          CLAY_ID("InsertMenu"), CLAY_ID("RunMenu"),
                                          CLAY_ID("DocsMenu")};
            for (int i = 0; i < 5; ++i) {
                CLAY(ids[i],
                     {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                                 .padding = {8, 8, 0, 0}, .childGap = 6,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .backgroundColor = Clay_PointerOver(ids[i]) || (i == 0 && fileMenuOpen) ||
                                         (i == 2 && insertMenuOpen)
                          ? menuHover : Clay_Color{0, 0, 0, 0}}) {
                    CLAY_TEXT(labels[i], {.textColor = menuText,
                                          .fontId = FONT_BODY, .fontSize = 20,
                                          .wrapMode = CLAY_TEXT_WRAP_NONE});
                    if (false && i < 4) {  // mockup has no chevrons; change back to (i < 4) to show them
                        CLAY(CLAY_IDI("MenuChevron", i),
                             {.layout = {.sizing = {CLAY_SIZING_FIXED(12), CLAY_SIZING_FIXED(12)}},
                              .custom = {.customData = &chevron}}) {}
                    }
                    if (i == 0 && !unsavedChangesOpen && Clay_Hovered() &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        fileMenuOpen = !fileMenuOpen;
                    }
                    if (i == 2 && !unsavedChangesOpen && Clay_Hovered() &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        insertMenuOpen = !insertMenuOpen;
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
              .backgroundColor = dropdownColor,
              .floating = {.offset = {0, 0}, .parentId = CLAY_ID("FileMenu").id,
                           .zIndex = 1,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                            .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                           .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
            FileMenuItem(CLAY_ID("NewButton"), CLAY_STRING("New"), true, newRequested);
            FileMenuItem(CLAY_ID("OpenButton"), CLAY_STRING("Open File"), true, openRequested);
            FileMenuItem(CLAY_ID("SaveButton"), CLAY_STRING("Save File"), canSave, saveRequested);
        }
    }

    inline void LayoutInsertDropdown() {
        CLAY(CLAY_ID("InsertDropdown"),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(200), CLAY_SIZING_FIT(0)},
                         .padding = {0, 0, 4, 4},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = dropdownColor,
              .floating = {.offset = {0, 0}, .parentId = CLAY_ID("InsertMenu").id,
                           .zIndex = 1,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                            .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                           .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID}}) {
            // ===== INSERT MENU ITEMS: add new Insert entries here =====
            // Each needs its own request flag in layout_state.h, handled in
            // ProcessLayoutActions (Math is handled in ProcessMathMenuTriggers).
            InsertMenuItem(CLAY_ID("InsertMathButton"), CLAY_STRING("Math"), true,
                           insertMathRequested);
            // ===== END INSERT MENU ITEMS =====
        }
    }
}
