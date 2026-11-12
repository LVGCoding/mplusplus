#pragma once

// File tab bar: drawing the tabs, and switching/closing them with scroll restore.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"

namespace layout_internal {
    // Report tab clicks; switching/closing is deferred until after rendering.
    inline void FileTab(const FileBase& file, std::size_t index, bool active) {
        const std::string& name = file.TabName();
        const auto i = static_cast<uint32_t>(index);
        const Clay_ElementId tabId = CLAY_IDI("FileTab", i);
        const Clay_ElementId closeId = CLAY_IDI("FileTabCloseButton", i);
        const bool clickable = !unsavedChangesOpen && !fileMenuOpen;

        CLAY(tabId,
             {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                         .padding = {12, 4, 0, 0}, .childGap = 12,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = !active && Clay_PointerOver(tabId)
                  ? Clay_Color{240, 240, 240, 255} : Clay_Color{0, 0, 0, 0},
              .border = {.color = active ? menuText : dividerColor,
                         .width = {.right = 1, .bottom = static_cast<uint16_t>(active ? 2 : 0)}}}) {
            Clay_String label = {
                .isStaticallyAllocated = false,
                .length = static_cast<int32_t>(name.size()),
                .chars = name.c_str()
            };
            CLAY_TEXT(label, {.textColor = active ? Clay_Color{20, 20, 20, 255}
                                                  : Clay_Color{100, 100, 100, 255},
                              .fontId = active ? FONT_BOLD : FONT_BODY, .fontSize = 18,
                              .wrapMode = CLAY_TEXT_WRAP_NONE});

            if (file.modified) {
                CLAY(CLAY_IDI("FileTabUnsavedDot", i),
                     {.layout = {.sizing = {CLAY_SIZING_FIXED(6), CLAY_SIZING_FIXED(6)}},
                      .backgroundColor = {20, 20, 20, 255},
                      .cornerRadius = CLAY_CORNER_RADIUS(3)}) {}
            }

            CLAY(closeId,
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
                  .backgroundColor = Clay_PointerOver(closeId)
                      ? Clay_Color{190, 190, 190, 255} : Clay_Color{0, 0, 0, 0},
                  .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
                if (clickable && Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    closeTabRequested = index;
                }
                CLAY(CLAY_IDI("FileTabCloseIcon", i),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
                      .custom = {.customData = &closeIcon}}) {}
            }

            if (clickable && closeTabRequested != index && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selectTabRequested = index;
            }
        }
    }

    // The row of open-file tabs under the menu bar.
    inline void LayoutFileBar(Editor& editor) {
        CLAY(CLAY_ID("FileBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {static_cast<uint16_t>(gutterWidth), 24, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = editorColor,
              // Scrolls horizontally (trackpad / shift-wheel) when tabs overflow.
              .clip = {.horizontal = true, .childOffset = Clay_GetScrollOffset()},
              .border = {.color = dividerColor, .width = {.bottom = 2}}}) {
            const auto& files = editor.Files();
            for (std::size_t i = 0; i < files.size(); ++i) {
                FileTab(*files[i], i, i == editor.ActiveTabIndex());
            }
        }
    }

    // Save the current file's scroll offset and restore the newly active file's.
    inline void SwitchTab(Editor& editor, std::size_t index) {
        if (index == editor.ActiveTabIndex()) return;
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
        if (scroll.found) {
            if (const FileBase* current = editor.ActiveFile()) {
                tabScroll[current] = *scroll.scrollPosition;
            }
        }
        editor.SetActiveTab(index);
        if (scroll.found) {
            const auto saved = tabScroll.find(editor.ActiveFile());
            *scroll.scrollPosition = saved != tabScroll.end() ? saved->second : Clay_Vector2{0, 0};
        }
    }

    inline void CloseActiveTab(Editor& editor) {
        tabScroll.erase(editor.ActiveFile());
        editor.CloseActiveFile();
        // The next active file comes in with its own scroll position.
        auto scroll = Clay_GetScrollContainerData(CLAY_ID("Workspace"));
        if (scroll.found) {
            const auto saved = tabScroll.find(editor.ActiveFile());
            *scroll.scrollPosition = saved != tabScroll.end() ? saved->second : Clay_Vector2{0, 0};
        }
    }
}
