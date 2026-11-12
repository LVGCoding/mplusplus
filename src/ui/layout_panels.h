#pragma once

// Fixed panels of the window: title bar, editor workspace (gutter + text), terminal.

#include "layout_common.h"
#include "layout_style.h"

namespace layout_internal {
    // Green title bar from the mockup
    inline void LayoutTitleBar(const FileBase* activeFile) {
        static std::string title;
        title = "M++ Editor";
        if (activeFile) {
            title += " - [";
            title += activeFile->path.empty() ? activeFile->TabName()
                                              : activeFile->path.string();
            title += "]";
        }
        CLAY(CLAY_ID("TitleBar"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36)},
                         .padding = {8, 8, 0, 0}, .childGap = 10,
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = titleBarColor}) {
            CLAY_TEXT(CLAY_STRING("M"), {.textColor = menuText, .fontId = FONT_BOLD,
                                         .fontSize = 26, .wrapMode = CLAY_TEXT_WRAP_NONE});
            Clay_String titleText = {.isStaticallyAllocated = false,
                                     .length = static_cast<int32_t>(title.size()),
                                     .chars = title.c_str()};
            CLAY_TEXT(titleText, {.textColor = menuText, .fontId = FONT_BODY,
                                  .fontSize = 20, .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
    }

    inline void LayoutWorkspace(const FileBase* activeFile) {
        CLAY(CLAY_ID("Workspace"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
              // One offset keeps the gutter and editor text aligned while scrolling.
              .clip = {.horizontal = true, .vertical = true,
                       .childOffset = Clay_GetScrollOffset()}}) {
            // Line-number gutter from the mockup (same font size/line height as the editor text)
            static std::string lineNumbers;
            lineNumbers.clear();
            int lineCount = activeFile ? static_cast<int>(activeFile->LineCount()) : 1;
            if (lineCount < gutterMinLines) lineCount = gutterMinLines;
            for (int i = 1; i <= lineCount; ++i) {
                lineNumbers += std::to_string(i);
                if (i < lineCount) lineNumbers += '\n';
            }
            CLAY(CLAY_ID("Gutter"),
                 {.layout = {.sizing = {CLAY_SIZING_FIXED(gutterWidth), CLAY_SIZING_GROW(0)},
                             .padding = {10, 0, 6, 0}},
                  .backgroundColor = gutterColor}) {
                Clay_String numbersText = {.isStaticallyAllocated = false,
                                           .length = static_cast<int32_t>(lineNumbers.size()),
                                           .chars = lineNumbers.c_str()};
                if (activeFile) {  // only number lines when a file is open
                    CLAY_TEXT(numbersText, {.textColor = menuText, .fontId = FONT_BODY,
                                            .fontSize = editorFontSize, .lineHeight = editorLineHeight,
                                            .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
                }
            }
            CLAY(CLAY_ID("Editor"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {6, 6, 6, 6}},
                  .backgroundColor = editorColor}) {
                if (activeFile) {
                    activeFile->LayoutContent();
                }
            }
        }
    }

    // Terminal panel from the mockup (placeholder: no input/output wired up yet)
    inline void LayoutTerminal() {
        CLAY(CLAY_ID("Terminal"),
             {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(140)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = editorColor,
              .border = {.color = terminalBorderColor, .width = {.top = 2}}}) {
            CLAY(CLAY_ID("TerminalHeader"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)},
                             .padding = {static_cast<uint16_t>(gutterWidth + 6), 8, 0, 0},
                             .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = menuBarColor}) {
                CLAY_TEXT(CLAY_STRING("Terminal"), {.textColor = menuText, .fontId = FONT_BODY,
                                                    .fontSize = 18,
                                                    .wrapMode = CLAY_TEXT_WRAP_NONE});
            }
            CLAY(CLAY_ID("TerminalBody"),
                 {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
                CLAY(CLAY_ID("TerminalGutter"),
                     {.layout = {.sizing = {CLAY_SIZING_FIXED(gutterWidth), CLAY_SIZING_GROW(0)}},
                      .backgroundColor = gutterColor}) {}
                CLAY(CLAY_ID("TerminalOutput"),
                     {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                 .padding = {6, 6, 6, 6}},
                      .clip = {.vertical = true}}) {
                    CLAY_TEXT(CLAY_STRING(">"), {.textColor = menuText, .fontId = FONT_BODY,
                                                 .fontSize = editorFontSize,
                                                 .lineHeight = editorLineHeight});
                }
            }
        }
    }
}
