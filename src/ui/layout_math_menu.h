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

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace layout_internal {
    // Scroll the item list back to the top (used when opening and when the search changes).
    inline void ResetMathMenuScroll() {
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("MathMenuList"));
        if (scroll.found) {
            *scroll.scrollPosition = {0, 0};
        }
    }

    // Scroll the item list just enough that `itemId` is fully visible.
    // Uses last frame's layout, the same way ScrollEditorCursorIntoView does.
    inline void ScrollMathMenuToItem(Clay_ElementId itemId) {
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("MathMenuList"));
        const auto list = Clay_GetElementData(CLAY_ID("MathMenuList"));
        const auto item = Clay_GetElementData(itemId);
        if (!scroll.found || !list.found || !item.found) return;

        const float listTop = list.boundingBox.y;
        const float listBottom = listTop + list.boundingBox.height;
        const float itemTop = item.boundingBox.y;
        const float itemBottom = itemTop + item.boundingBox.height;

        float offset = scroll.scrollPosition->y;  // 0 at the top, negative when scrolled down
        if (itemTop < listTop) {
            offset += listTop - itemTop;           // row is above the view: scroll up
        } else if (itemBottom > listBottom) {
            offset -= itemBottom - listBottom;     // row is below the view: scroll down
        }
        const float minimumOffset = std::min(0.0f,
            scroll.scrollContainerDimensions.height - scroll.contentDimensions.height);
        scroll.scrollPosition->y = std::clamp(offset, minimumOffset, 0.0f);
    }

    // Open the popup with its top-left corner at `position` (window pixels).
    // Every trigger goes through this, so new triggers only need to pick a position.
    inline void OpenMathMenu(Clay_Vector2 position) {
        if (unsavedChangesOpen) return;
        CloseAllMenus();
        mathMenuOpen = true;
        mathMenuPosition = position;
        mathMenuItemRequested = SIZE_MAX;
        mathMenuQuery.clear();
        mathMenuSelected = 0;
        ResetEditorCursorBlink();  // search-bar cursor starts solid
        // Start at the top of the list if it was scrolled before.
        ResetMathMenuScroll();
    }

    inline void CloseMathMenu() {
        mathMenuOpen = false;
        mathMenuItemRequested = SIZE_MAX;
        mathMenuQuery.clear();
    }

    // Indices into MathMenuItems() matching the search text (case-insensitive,
    // ignoring the leading backslash). Names starting with it come first.
    inline std::vector<std::size_t> FilteredMathItems() {
        auto lower = [](std::string_view text) {
            std::string out(text);
            for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        };
        const std::string query = lower(mathMenuQuery);
        std::vector<std::size_t> starts, contains;
        const auto& items = MathMenuItems();
        for (std::size_t i = 0; i < items.size(); ++i) {
            std::string name = lower(items[i].label);
            if (!name.empty() && name[0] == '\\') name.erase(0, 1);
            const auto at = name.find(query);
            if (at == 0) starts.push_back(i);
            else if (at != std::string::npos) contains.push_back(i);
        }
        starts.insert(starts.end(), contains.begin(), contains.end());
        return starts;
    }

    // Position just below an element from the last layout (e.g. a menu-bar button).
    inline Clay_Vector2 PositionBelow(Clay_ElementId id) {
        const auto element = Clay_GetElementData(id);
        if (!element.found) return {0, 0};
        return {element.boundingBox.x, element.boundingBox.y + element.boundingBox.height};
    }

    // UI triggers
    inline void ProcessMathMenuTriggers() {
        // Trigger: Insert -> Math. Opens just below the Insert menu button.
        if (insertMathRequested) {
            insertMathRequested = false;
            OpenMathMenu(PositionBelow(CLAY_ID("InsertMenu")));
        }

        // Add more UI triggers here

    }

    // Keyboard triggers. Called after the file has handled this frame's input.
    inline void CheckMathMenuKeyTriggers(FileBase& file, const EditorView& view) {
        // Trigger: typing a backslash in an M++ file. Opens just below the cursor.
        auto* mpp = dynamic_cast<FileMpp*>(&file);
        if (mpp && mpp->TakeMathMenuRequest()) {
            const auto cursor = file.CursorBounds(view);
            OpenMathMenu(cursor ? Clay_Vector2{cursor->x, cursor->y + cursor->height + 4}
                                : PositionBelow(CLAY_ID("InsertMenu")));
        }
    }

    // Keyboard control while the popup is open; the editor gets no input then.
    // Typing filters the list, Up/Down choose, Enter inserts, and Backspace on
    // an empty search closes the popup (like deleting the backslash).
    inline void HandleMathMenuKeys() {
        if (!mathMenuOpen) return;
        bool queryChanged = false;
        while (const int codepoint = GetCharPressed()) {
            if (codepoint > 32 && codepoint < 127 && codepoint != '\\') {
                mathMenuQuery += static_cast<char>(codepoint);
                mathMenuSelected = 0;
                queryChanged = true;
            }
        }
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (mathMenuQuery.empty()) {
                CloseMathMenu();
                return;
            }
            mathMenuQuery.pop_back();
            mathMenuSelected = 0;
            queryChanged = true;
        }
        // New search results start at the top, where the selection is.
        if (queryChanged) {
            ResetMathMenuScroll();
            ResetEditorCursorBlink();
        }
        const auto filtered = FilteredMathItems();
        if (filtered.empty()) return;
        bool selectionMoved = false;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
            mathMenuSelected = (mathMenuSelected + 1) % filtered.size();
            selectionMoved = true;
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
            mathMenuSelected = (mathMenuSelected + filtered.size() - 1) % filtered.size();
            selectionMoved = true;
        }
        mathMenuSelected = std::min(mathMenuSelected, filtered.size() - 1);
        // Keep the selected row in view. Only on arrow keys, so the mouse wheel
        // can still scroll freely. Row IDs use the index into MathMenuItems().
        if (selectionMoved) {
            ScrollMathMenuToItem(CLAY_IDI("MathMenuItem",
                                          static_cast<uint32_t>(filtered[mathMenuSelected])));
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_TAB)) {
            mathMenuItemRequested = filtered[mathMenuSelected];
        }
    }

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
        SelectMathMenuItem(editor, index);
        // Remove this line to keep the popup open after a click.
        CloseMathMenu();
    }

    inline void MathMenuButton(const MathMenuItem& item, std::size_t index, bool selected) {
        const Clay_ElementId id = CLAY_IDI("MathMenuItem", static_cast<uint32_t>(index));
        CLAY(id,
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(mathMenuItemHeight)},
                         .padding = {12, 12, 4, 4}, .childGap = 12,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = selected || Clay_PointerOver(id) ? menuHover
                                                                  : Clay_Color{0, 0, 0, 0},
              .cornerRadius = CLAY_CORNER_RADIUS(6)}) {
            Clay_String label = {.isStaticallyAllocated = false,
                                 .length = static_cast<int32_t>(item.label.size()),
                                 .chars = item.label.data()};
            CLAY_TEXT(label, {.textColor = menuText, .fontId = FONT_BODY,
                              .fontSize = mathMenuFontSize, .wrapMode = CLAY_TEXT_WRAP_NONE});
            // Spacer pushes the preview to the right edge of the row.
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
            // Rendered preview of the node this item inserts.
            if (EditorNode* preview = MathMenuPreview(index)) {
                RenderContext context;
                context.fontSize = mathMenuPreviewFontSize;
                preview->Render(context);
            }
            if (!unsavedChangesOpen && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                mathMenuItemRequested = index;
            }
        }
    }

    // Blinking text cursor at the end of the search text. Same timing as the
    // editor's cursor: solid for a moment after typing, then blinking.
    inline void MathMenuSearchCursor() {
        const double elapsed = GetTime() - lastCursorActivity;
        const bool visible = elapsed < 0.7 || static_cast<int>((elapsed - 0.7) / 0.5) % 2 == 0;
        CLAY(CLAY_ID("MathMenuSearchCursor"),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(20)}},
              .backgroundColor = visible ? menuText : Clay_Color{0, 0, 0, 0}}) {}
    }

    // Search bar: shows what has been typed since the backslash.
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
            if (mathMenuQuery.empty()) {
                // Cursor flush against the placeholder, so it doesn't read as a divider.
                CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                         .childGap = 1,
                                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
                    MathMenuSearchCursor();
                    CLAY_TEXT(CLAY_STRING("Search..."),
                              {.textColor = {150, 150, 150, 255}, .fontId = FONT_BODY,
                               .fontSize = 18, .wrapMode = CLAY_TEXT_WRAP_NONE});
                }
            } else {
                Clay_String query = {.isStaticallyAllocated = false,
                                     .length = static_cast<int32_t>(mathMenuQuery.size()),
                                     .chars = mathMenuQuery.c_str()};
                // No gap between the text and the cursor right after it.
                CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                         .childGap = 1,
                                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
                    CLAY_TEXT(query, {.textColor = menuText, .fontId = FONT_BODY,
                                      .fontSize = 18, .wrapMode = CLAY_TEXT_WRAP_NONE});
                    MathMenuSearchCursor();
                }
            }
        }
    }

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
                const auto filtered = FilteredMathItems();
                for (std::size_t i = 0; i < filtered.size(); ++i) {
                    MathMenuButton(items[filtered[i]], filtered[i], i == mathMenuSelected);
                }
                if (filtered.empty()) {
                    CLAY_TEXT(CLAY_STRING("No matches"),
                              {.textColor = {150, 150, 150, 255}, .fontId = FONT_BODY,
                               .fontSize = mathMenuFontSize, .wrapMode = CLAY_TEXT_WRAP_NONE});
                }
            }
        }
    }
}
