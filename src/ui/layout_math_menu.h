#pragma once

// Math popup: a scrollable list of math items (frac, sqrt, mat, ...).
//   - Items and what they do:  layout_math_menu_items.h
//   - Ways to open the popup:  the TRIGGERS sections below
//   - Size:                    mathMenu* constants in layout_style.h

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_math_menu_items.h"
#include "layout_state.h"
#include "layout_style.h"

namespace layout_internal {
    // Open the popup with its top-left corner at `position` (window pixels).
    // Every trigger goes through this, so new triggers only need to pick a position.
    inline void OpenMathMenu(Clay_Vector2 position) {
        if (unsavedChangesOpen) return;
        CloseAllMenus();
        mathMenuOpen = true;
        mathMenuPosition = position;
        mathMenuItemRequested = SIZE_MAX;
        // Start at the top of the list if it was scrolled before.
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("MathMenuList"));
        if (scroll.found) {
            *scroll.scrollPosition = {0, 0};
        }
    }

    inline void CloseMathMenu() {
        mathMenuOpen = false;
        mathMenuItemRequested = SIZE_MAX;
    }

    // Position just below an element from the last layout (e.g. a menu-bar button).
    inline Clay_Vector2 PositionBelow(Clay_ElementId id) {
        const auto element = Clay_GetElementData(id);
        if (!element.found) return {0, 0};
        return {element.boundingBox.x, element.boundingBox.y + element.boundingBox.height};
    }

    // ===== TRIGGERS (menu / button): ways to open the popup from the UI =====
    // Called from ProcessLayoutActions after rendering. For a new trigger, set a
    // request flag (layout_state.h) where the click happens and handle it here.
    inline void ProcessMathMenuTriggers() {
        // Trigger: Insert -> Math. Opens just below the Insert menu button.
        if (insertMathRequested) {
            insertMathRequested = false;
            OpenMathMenu(PositionBelow(CLAY_ID("InsertMenu")));
        }

        // Add more UI triggers here, e.g.:
        // if (toolbarMathRequested) {
        //     toolbarMathRequested = false;
        //     OpenMathMenu(PositionBelow(CLAY_ID("ToolbarMathButton")));
        // }
    }
    // ===== END TRIGGERS (menu / button) =====

    // ===== TRIGGERS (keyboard): ways to open the popup while typing =====
    // Called every frame from HandleActiveFileInput, before the file gets input.
    // `view` lets you place the popup at the text cursor.
    inline void CheckMathMenuKeyTriggers(const FileBase& /*file*/, const EditorView& /*view*/) {
        // Example (disabled): open at the cursor when backslash is pressed.
        // The '\' still reaches the editor afterwards; consume it if you don't want that.
        //
        // if (!mathMenuOpen && IsKeyPressed(KEY_BACKSLASH)) {
        //     if (const auto cursor = file.CursorBounds(view)) {
        //         OpenMathMenu({cursor->x, cursor->y + cursor->height});
        //     }
        // }
    }
    // ===== END TRIGGERS (keyboard) =====

    // Close on Escape or a click anywhere outside the popup.
    // Called at the start of Layout(), using last frame's hover data.
    inline void HandleMathMenuDismiss() {
        if (!mathMenuOpen) return;
        if (IsKeyPressed(KEY_ESCAPE) ||
            (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
             !Clay_PointerOver(CLAY_ID("MathMenuPopup")))) {
            CloseMathMenu();
        }
    }

    // Run the clicked item's action (after rendering), then close the popup.
    inline void ProcessMathMenuSelection(Editor& editor) {
        if (mathMenuItemRequested == SIZE_MAX) return;
        const std::size_t index = mathMenuItemRequested;
        mathMenuItemRequested = SIZE_MAX;
        const auto& items = MathMenuItems();
        if (index < items.size() && items[index].onSelect) {
            items[index].onSelect(editor);
        }
        // Remove this line to keep the popup open after a click.
        CloseMathMenu();
    }

    inline void MathMenuButton(const MathMenuItem& item, std::size_t index) {
        const Clay_ElementId id = CLAY_IDI("MathMenuItem", static_cast<uint32_t>(index));
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(mathMenuItemHeight)},
                         .padding = {12, 12, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = Clay_PointerOver(id) ? menuHover : Clay_Color{0, 0, 0, 0},
              .cornerRadius = CLAY_CORNER_RADIUS(6)}) {
            Clay_String label = {.isStaticallyAllocated = false,
                                 .length = static_cast<int32_t>(item.label.size()),
                                 .chars = item.label.data()};
            CLAY_TEXT(label, {.textColor = menuText, .fontId = FONT_BODY,
                              .fontSize = mathMenuFontSize, .wrapMode = CLAY_TEXT_WRAP_NONE});
            if (!unsavedChangesOpen && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                mathMenuItemRequested = index;
            }
        }
    }

    // ===== SEARCH BAR (not functional yet) =====
    // Only draws a placeholder. To make it work later: keep the typed text in
    // a std::string in layout_state.h, show it here instead of the placeholder,
    // and skip non-matching items in LayoutMathMenu's loop.
    inline void MathMenuSearchBar() {
        CLAY(CLAY_ID("MathMenuSearch"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(mathMenuSearchHeight)},
                         .padding = {10, 10, 0, 0}, .childGap = 8,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {255, 255, 255, 255},
              .cornerRadius = CLAY_CORNER_RADIUS(8),
              .border = {.color = {190, 190, 190, 255}, .width = CLAY_BORDER_ALL(1)}}) {
            CLAY(CLAY_ID("MathMenuSearchIcon"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(16), CLAY_SIZING_FIXED(16)}},
                  .custom = {.customData = &searchIcon}}) {}
            CLAY_TEXT(CLAY_STRING("Search..."),
                      {.textColor = {150, 150, 150, 255}, .fontId = FONT_BODY,
                       .fontSize = 18, .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
    }
    // ===== END SEARCH BAR =====

    inline void LayoutMathMenu() {
        // Outer window: rounded, holds the fixed search bar and the scrolling list.
        CLAY(CLAY_ID("MathMenuPopup"),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(mathMenuWidth), CLAY_SIZING_FIT(0)},
                         .padding = CLAY_PADDING_ALL(10), .childGap = 8,
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = {250, 250, 250, 255},
              .cornerRadius = CLAY_CORNER_RADIUS(mathMenuCornerRadius),
              .floating = {.offset = mathMenuPosition, .zIndex = 5,
                           .attachTo = CLAY_ATTACH_TO_ROOT},
              .border = {.color = {160, 160, 160, 255}, .width = CLAY_BORDER_ALL(1)}}) {
            MathMenuSearchBar();

            // Item list: scrolls vertically (mouse wheel / trackpad) when items overflow.
            CLAY(CLAY_ID("MathMenuList"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                        CLAY_SIZING_FIT(0, mathMenuListMaxHeight)},
                             .childGap = 2,
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
                const auto& items = MathMenuItems();
                for (std::size_t i = 0; i < items.size(); ++i) {
                    MathMenuButton(items[i], i);
                }
            }
        }
    }
}
