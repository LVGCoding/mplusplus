#pragma once

// Fixed panels of the window: title bar, editor workspace (gutter + text), terminal.

#include "layout_common.h"
#include "layout_linenum.h"
#include "layout_state.h"
#include "layout_style.h"

#include <algorithm>

namespace layout_internal {
// Green title bar from the mockup
inline void LayoutTitleBar(const FileBase* activeFile) {
    // Clay borrows the title until rendering finishes.
    static std::string title;
    title = "M++ Editor";
    if (activeFile) {
        title += " - [";
        title += activeFile->path.empty() ? activeFile->TabName() : activeFile->path.string();
        title += "]";
    }
    CLAY(CLAY_ID("TitleBar"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                     .padding = {8, 8, 0, 0},
                     .childGap = 10,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = TITLE_BAR_COLOR}) {
        CLAY_TEXT(CLAY_STRING("M"),
                  {.textColor = MENU_TEXT,
                   .fontId = FONT_BOLD,
                   .fontSize = 26,
                   .wrapMode = CLAY_TEXT_WRAP_NONE});
        Clay_String titleText = {.isStaticallyAllocated = false,
                                 .length = static_cast<int32_t>(title.size()),
                                 .chars = title.c_str()};
        CLAY_TEXT(titleText,
                  {.textColor = MENU_TEXT,
                   .fontId = FONT_BODY,
                   .fontSize = 20,
                   .wrapMode = CLAY_TEXT_WRAP_NONE});
    }
}

inline void LayoutWorkspace(const FileBase* activeFile) {
    const bool variableLineHeight = activeFile && activeFile->LineElementId(0) != 0;
    CLAY(CLAY_ID("Workspace"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          // One offset keeps the gutter and editor text aligned while scrolling.
          .clip = {.horizontal = true, .vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
        LayoutLineNumberGutter(activeFile, variableLineHeight);
        CLAY(CLAY_ID("Editor"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = {6, 6, 6, 6}},
              .backgroundColor = EDITOR_COLOR}) {
            if (activeFile) {
                activeFile->LayoutContent();
                if (variableLineHeight) {
                    LayoutRowLineNumbers(*activeFile);
                }
            }
        }
    }
}

// Follow the mouse while the terminal's top bar is being dragged, and keep the
// height within bounds (also when the window shrinks). Call before the Root
// element is opened: once it is open, its size this frame reads as zero.
inline void UpdateTerminalHeight() {
    if (terminalResizing) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !unsavedChangesOpen) {
            terminalHeight =
                terminalResizeStartHeight + (terminalResizeStartY - GetMousePosition().y);
        } else {
            terminalResizing = false;
        }
    }
    // At least the header plus one line; at most what leaves the title bar,
    // menu bar, tab bar and a few lines of editor visible.
    constexpr float minHeight = TERMINAL_RESIZE_HANDLE + 30 + EDITOR_LINE_HEIGHT + 12;
    const auto root = Clay_GetElementData(CLAY_ID("Root"));
    const float windowHeight =
        root.found ? root.boundingBox.height : static_cast<float>(GetScreenHeight());
    const float maxHeight = std::max(minHeight, windowHeight - 3 * 36 - 120);
    terminalHeight = std::clamp(terminalHeight, minHeight, maxHeight);

    // Show the up/down resize cursor over the bar and while dragging it.
    static bool resizeCursor = false;
    const bool wantResizeCursor =
        terminalResizing || Clay_PointerOver(CLAY_ID("TerminalResizeHandle"));
    if (wantResizeCursor != resizeCursor) {
        SetMouseCursor(wantResizeCursor ? MOUSE_CURSOR_RESIZE_NS : MOUSE_CURSOR_DEFAULT);
        resizeCursor = wantResizeCursor;
    }
}

// Terminal panel: shows terminalOutput (e.g. from Run -> Run), then a ">" prompt.
// No input is wired up yet. Call UpdateTerminalHeight() first, before the Root
// element is opened.
inline void LayoutTerminal() {
    // Follow new output: scroll so the last line (from last frame's layout) shows.
    if (terminalScrollToEnd > 0) {
        --terminalScrollToEnd;
        const Clay_ScrollContainerData scroll =
            Clay_GetScrollContainerData(CLAY_ID("TerminalOutput"));
        if (scroll.found) {
            scroll.scrollPosition->y = std::min(
                0.0f, scroll.scrollContainerDimensions.height - scroll.contentDimensions.height);
        }
    }
    CLAY(CLAY_ID("Terminal"),
         {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(terminalHeight)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = EDITOR_COLOR}) {
        // Drag this bar up or down to resize the terminal.
        const Clay_ElementId handleId = CLAY_ID("TerminalResizeHandle");
        CLAY(handleId,
             {.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                    CLAY_SIZING_FIXED(TERMINAL_RESIZE_HANDLE)}},
              .backgroundColor = terminalResizing || Clay_PointerOver(handleId)
                                     ? TERMINAL_HANDLE_HOVER_COLOR
                                     : TERMINAL_BORDER_COLOR}) {
            if (!unsavedChangesOpen && !IsAnyMenuOpen() && Clay_Hovered() &&
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                terminalResizing = true;
                terminalResizeStartY = GetMousePosition().y;
                terminalResizeStartHeight = terminalHeight;
            }
        }
        CLAY(CLAY_ID("TerminalHeader"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)},
                         .padding = {static_cast<uint16_t>(GUTTER_WIDTH + 6), 8, 0, 0},
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = MENU_BAR_COLOR}) {
            CLAY_TEXT(CLAY_STRING("Terminal"),
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BODY,
                       .fontSize = 18,
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
        CLAY(CLAY_ID("TerminalBody"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
            CLAY(CLAY_ID("TerminalGutter"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(GUTTER_WIDTH), CLAY_SIZING_GROW(0)}},
                  .backgroundColor = GUTTER_COLOR}) {}
            CLAY(CLAY_ID("TerminalOutput"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {6, 6, 6, 6},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM},
                  .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
                for (const run::OutputLine& line : terminalOutput) {
                    const Clay_String text = {.isStaticallyAllocated = false,
                                              .length = static_cast<int32_t>(line.text.size()),
                                              .chars = line.text.data()};
                    const Clay_Color color =
                        line.kind == run::OutputLine::Kind::Error  ? FORM_ERROR_COLOR
                        : line.kind == run::OutputLine::Kind::Info ? FORM_MUTED_COLOR
                                                                   : MENU_TEXT;
                    CLAY_TEXT(text,
                              {.textColor = color,
                               .fontId = FONT_BODY,
                               .fontSize = EDITOR_FONT_SIZE,
                               .lineHeight = EDITOR_LINE_HEIGHT});
                }
                CLAY_TEXT(CLAY_STRING(">"),
                          {.textColor = MENU_TEXT,
                           .fontId = FONT_BODY,
                           .fontSize = EDITOR_FONT_SIZE,
                           .lineHeight = EDITOR_LINE_HEIGHT});
            }
        }
    }
}
}  // namespace layout_internal
