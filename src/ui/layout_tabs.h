#pragma once

// File tab bar: drawing the tabs, dragging them to reorder, and switching/closing
// them with scroll restore.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"

#include <algorithm>
#include <cmath>

namespace layout_internal {
inline void BeginTabDrag(const FileBase& file,
                         Vector2 mouse) {
    tabDrag = {&file, mouse, false};
}

// Run after rendering: moving the vector during layout would invalidate tab indices.
// Explicit input values also let layout tests exercise gestures without opening a window.
inline void UpdateTabDrag(Editor& editor,
                          Vector2 mouse,
                          bool mouseDown,
                          bool cancelled,
                          float deltaTime) {
    if (cancelled || unsavedChangesOpen || IsAnyMenuOpen() || closeRequested) {
        tabDrag = {};
        return;
    }
    if (!tabDrag.file) {
        return;
    }
    const auto& files = editor.Files();
    const auto source = std::find_if(
        files.begin(), files.end(), [](const auto& file) { return file.get() == tabDrag.file; });
    if (source == files.end()) {
        tabDrag = {};
        return;
    }
    constexpr float DRAG_THRESHOLD = 6.0f;
    if (!tabDrag.dragging && std::abs(mouse.x - tabDrag.start.x) < DRAG_THRESHOLD) {
        if (!mouseDown) {
            tabDrag = {};
        }
        return;
    }
    if (!tabDrag.dragging) {
        const auto index = static_cast<uint32_t>(source - files.begin());
        const auto tab = Clay_GetElementData(CLAY_IDI("FileTab", index));
        tabDragGrabOffset = tab.found ? tabDrag.start.x - tab.boundingBox.x : 0;
    }
    tabDrag.dragging = true;
    const auto bar = Clay_GetElementData(CLAY_ID("FileBar"));
    if (!bar.found || mouse.y < bar.boundingBox.y ||
        mouse.y > bar.boundingBox.y + bar.boundingBox.height) {
        if (!mouseDown) {
            tabDrag = {};
        }
        return;
    }

    const auto sourceIndex = static_cast<std::size_t>(source - files.begin());
    std::size_t destination = sourceIndex;
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto tab = Clay_GetElementData(CLAY_IDI("FileTab", static_cast<uint32_t>(index)));
        if (!tab.found) {
            continue;
        }
        const float midpoint = tab.boundingBox.x + tab.boundingBox.width / 2;
        if (index < sourceIndex && mouse.x < midpoint) {
            destination = index;
            break;
        }
        if (index > sourceIndex && mouse.x > midpoint) {
            destination = index;
        }
    }
    editor.MoveTab(sourceIndex, destination);
    // Include the release position so a quick drag between frames still lands correctly.
    if (!mouseDown) {
        tabDrag = {};
        return;
    }

    // Reveal more tabs while holding a dragged tab near either edge of an overflowing bar.
    auto scroll = Clay_GetScrollContainerData(CLAY_ID("FileBar"));
    if (scroll.found) {
        constexpr float EDGE_WIDTH = 24.0f;
        constexpr float SCROLL_SPEED = 450.0f;
        float direction = 0;
        if (mouse.x < bar.boundingBox.x + EDGE_WIDTH) {
            direction = 1;
        } else if (mouse.x > bar.boundingBox.x + bar.boundingBox.width - EDGE_WIDTH) {
            direction = -1;
        }
        const float minimumOffset =
            std::min(0.0f, scroll.scrollContainerDimensions.width - scroll.contentDimensions.width);
        scroll.scrollPosition->x = std::clamp(
            scroll.scrollPosition->x + direction * SCROLL_SPEED * deltaTime, minimumOffset, 0.0f);
    }
}

// Report tab clicks; switching/closing is deferred until after rendering.
// `index` is the file's index in editor.Files(); `dragging` is true while this
// tab is being dragged to a new position.
inline void FileTab(const FileBase& file,
                    std::size_t index,
                    bool active,
                    bool dragging) {
    const std::string& name = file.TabName();
    const auto i = static_cast<uint32_t>(index);
    const Clay_ElementId tabId = CLAY_IDI("FileTab", i);
    const Clay_ElementId closeId = CLAY_IDI("FileTabCloseButton", i);
    const bool clickable = !unsavedChangesOpen && !IsAnyMenuOpen();

    CLAY(tabId,
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                     .padding = {12, 4, 0, 0},
                     .childGap = 12,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          // While dragged, the tab's own slot fades to show where it will land;
          // the floating TabDragPreview follows the mouse.
          .backgroundColor = dragging ? TAB_DRAG_SLOT_COLOR
                            : !active && Clay_PointerOver(tabId) ? Clay_Color{240, 240, 240, 255}
                                                                 : Clay_Color{0, 0, 0, 0},
          .border = {.color = active ? MENU_TEXT : DIVIDER_COLOR,
                     .width = {.right = 1, .bottom = static_cast<uint16_t>(active ? 2 : 0)}}}) {
        Clay_String label = {.isStaticallyAllocated = false,
                             .length = static_cast<int32_t>(name.size()),
                             .chars = name.c_str()};
        CLAY_TEXT(
            label,
            {.textColor = dragging ? Clay_Color{0, 0, 0, 0}  // empty slot; keeps its width
                          : active ? Clay_Color{20, 20, 20, 255}
                                   : Clay_Color{100, 100, 100, 255},
             .fontId = active ? FONT_BOLD : FONT_BODY,
             .fontSize = 18,
             .wrapMode = CLAY_TEXT_WRAP_NONE});

        if (file.modified) {
            CLAY(CLAY_IDI("FileTabUnsavedDot", i),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(6), CLAY_SIZING_FIXED(6)}},
                  .backgroundColor = dragging ? Clay_Color{0, 0, 0, 0}
                                              : Clay_Color{20, 20, 20, 255},
                  .cornerRadius = CLAY_CORNER_RADIUS(3)}) {}
        }

        CLAY(closeId,
             {.layout = {.sizing = {CLAY_SIZING_FIXED(28), CLAY_SIZING_FIXED(28)}},
              .backgroundColor = !dragging && Clay_PointerOver(closeId)
                                     ? Clay_Color{190, 190, 190, 255}
                                     : Clay_Color{0, 0, 0, 0},
              .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
            if (clickable && Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                closeTabRequested = index;
            }
            if (!dragging) {
                CLAY(CLAY_IDI("FileTabCloseIcon", i),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
                      .custom = {.customData = &closeIcon}}) {}
            }
        }

        if (clickable && closeTabRequested != index && Clay_Hovered() &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectTabRequested = index;
            BeginTabDrag(file, GetMousePosition());
        }
    }
}

// The row of open-file tabs under the menu bar.
// See-through copy of the dragged tab that follows the mouse along the tab
// bar. `tab` and `bar` are last frame's boxes of the tab and the tab bar.
inline void TabDragPreview(const FileBase& file, Clay_BoundingBox tab, Clay_BoundingBox bar) {
    const std::string& name = file.TabName();
    // Keep it within the tab bar's padding (where the tabs themselves sit).
    const float left = bar.x + GUTTER_WIDTH;
    const float right = std::max(left, bar.x + bar.width - 24 - tab.width);
    const float x = std::clamp(GetMousePosition().x - tabDragGrabOffset, left, right);
    CLAY(CLAY_ID("FileTabDragPreview"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(tab.width), CLAY_SIZING_FIXED(tab.height)},
                     .padding = {12, 4, 0, 0},
                     .childGap = 12,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = TAB_DRAG_PREVIEW_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          // Clicks and hover go to whatever is underneath.
          .floating = {.offset = {x, tab.y},
                       .zIndex = 10,
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                       .attachTo = CLAY_ATTACH_TO_ROOT},
          .border = {.color = TAB_DRAG_PREVIEW_BORDER, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        Clay_String label = {.isStaticallyAllocated = false,
                             .length = static_cast<int32_t>(name.size()),
                             .chars = name.c_str()};
        CLAY_TEXT(label, {.textColor = {20, 20, 20, 190},
                          .fontId = FONT_BODY,
                          .fontSize = 18,
                          .wrapMode = CLAY_TEXT_WRAP_NONE});
    }
}

inline void LayoutFileBar(Editor& editor) {
    const auto& files = editor.Files();
    const auto bar = Clay_GetElementData(CLAY_ID("FileBar"));
    Clay_ElementData draggedBox = {};
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (files[i].get() == tabDrag.file && tabDrag.dragging) {
            draggedBox = Clay_GetElementData(CLAY_IDI("FileTab", static_cast<uint32_t>(i)));
            break;
        }
    }
    CLAY(CLAY_ID("FileBar"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                     .padding = {static_cast<uint16_t>(GUTTER_WIDTH), 24, 0, 0},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = EDITOR_COLOR,
          // Scrolls horizontally (trackpad / shift-wheel) when tabs overflow.
          .clip = {.horizontal = true, .childOffset = Clay_GetScrollOffset()},
          .border = {.color = DIVIDER_COLOR, .width = {.bottom = 2}}}) {
        for (std::size_t i = 0; i < files.size(); ++i) {
            FileTab(*files[i], i, i == editor.ActiveTabIndex(),
                    files[i].get() == tabDrag.file && tabDrag.dragging);
        }
    }
    if (tabDrag.file && tabDrag.dragging && bar.found && draggedBox.found) {
        TabDragPreview(*tabDrag.file, draggedBox.boundingBox, bar.boundingBox);
    }
}

// Save the current file's scroll offset and restore the newly active file's.
inline void SwitchTab(Editor& editor,
                      std::size_t index) {
    if (index == editor.ActiveTabIndex()) {
        return;
    }
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
}  // namespace layout_internal
