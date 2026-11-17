#pragma once

// File tab bar: drawing the tabs, dragging them to reorder, and switching/closing
// them with scroll restore.

#include "layout_common.h"
#include "layout_icons.h"
#include "layout_state.h"
#include "layout_style.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace layout_internal {
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
            // Pressing a tab also picks it up; it only moves once dragged.
            draggedTab = &file;
            tabDragStartX = GetMousePosition().x;
            tabDragMoved = false;
        }
    }
}

// Index of `file` in editor.Files(), or SIZE_MAX if it is not open.
inline std::size_t FileIndex(const std::vector<std::unique_ptr<FileBase>>& files,
                             const FileBase* file) {
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (files[i].get() == file) {
            return i;
        }
    }
    return SIZE_MAX;
}

// Bring tabOrder in line with the open files: drop closed ones and add newly
// opened ones at the end, keeping the user's order for the rest.
inline void SyncTabOrder(const std::vector<std::unique_ptr<FileBase>>& files) {
    std::erase_if(tabOrder,
                  [&](const FileBase* file) { return FileIndex(files, file) == SIZE_MAX; });
    for (const auto& file : files) {
        if (std::find(tabOrder.begin(), tabOrder.end(), file.get()) == tabOrder.end()) {
            tabOrder.push_back(file.get());
        }
    }
    if (draggedTab && FileIndex(files, draggedTab) == SIZE_MAX) {
        draggedTab = nullptr;
    }
}

// Move the dragged tab past a neighbour once the mouse crosses that
// neighbour's middle. Uses last frame's tab positions.
inline void UpdateTabDrag(const std::vector<std::unique_ptr<FileBase>>& files) {
    if (!draggedTab) {
        return;
    }
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) || unsavedChangesOpen) {
        draggedTab = nullptr;
        tabDragMoved = false;
        return;
    }
    const float mouseX = GetMousePosition().x;
    if (!tabDragMoved) {
        // Remember where on the tab it was grabbed (last frame's box is still
        // where it was pressed).
        const auto index = static_cast<uint32_t>(FileIndex(files, draggedTab));
        const auto tab = Clay_GetElementData(CLAY_IDI("FileTab", index));
        if (tab.found) {
            tabDragGrabOffset = tabDragStartX - tab.boundingBox.x;
        }
        if (std::fabs(mouseX - tabDragStartX) < 4.0f) {
            return;  // still just a click
        }
    }
    tabDragMoved = true;

    const auto it = std::find(tabOrder.begin(), tabOrder.end(), draggedTab);
    if (it == tabOrder.end()) {
        return;
    }
    const auto position = static_cast<std::size_t>(it - tabOrder.begin());
    auto middle = [&](std::size_t display) -> std::optional<float> {
        const auto index = static_cast<uint32_t>(FileIndex(files, tabOrder[display]));
        const auto tab = Clay_GetElementData(CLAY_IDI("FileTab", index));
        if (!tab.found) {
            return std::nullopt;
        }
        return tab.boundingBox.x + tab.boundingBox.width / 2;
    };
    if (position + 1 < tabOrder.size()) {
        if (const auto right = middle(position + 1); right && mouseX > *right) {
            std::swap(tabOrder[position], tabOrder[position + 1]);
            return;
        }
    }
    if (position > 0) {
        if (const auto left = middle(position - 1); left && mouseX < *left) {
            std::swap(tabOrder[position], tabOrder[position - 1]);
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
    SyncTabOrder(files);
    UpdateTabDrag(files);
    // Read last frame's boxes now: once an element is opened this frame, its
    // box reads as zero until layout finishes.
    const auto bar = Clay_GetElementData(CLAY_ID("FileBar"));
    const auto draggedBox =
        draggedTab && tabDragMoved
            ? Clay_GetElementData(
                  CLAY_IDI("FileTab", static_cast<uint32_t>(FileIndex(files, draggedTab))))
            : Clay_ElementData{};
    CLAY(CLAY_ID("FileBar"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                     .padding = {static_cast<uint16_t>(GUTTER_WIDTH), 24, 0, 0},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = EDITOR_COLOR,
          // Scrolls horizontally (trackpad / shift-wheel) when tabs overflow.
          .clip = {.horizontal = true, .childOffset = Clay_GetScrollOffset()},
          .border = {.color = DIVIDER_COLOR, .width = {.bottom = 2}}}) {
        // Drawn in the user's order; clicks still report the Editor's index.
        for (const FileBase* file : tabOrder) {
            const std::size_t index = FileIndex(files, file);
            FileTab(*file, index, index == editor.ActiveTabIndex(),
                    file == draggedTab && tabDragMoved);
        }
    }
    if (draggedTab && tabDragMoved && bar.found && draggedBox.found) {
        TabDragPreview(*draggedTab, draggedBox.boundingBox, bar.boundingBox);
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
