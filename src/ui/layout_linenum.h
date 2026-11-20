#pragma once

// Line-number gutter layout for fixed-height text and dynamically sized math rows.

#include "layout_common.h"
#include "layout_style.h"

#include <vector>

namespace layout_internal {
inline void LayoutRowLineNumbers(const Tab& file) {
    const std::size_t rowCount = file.LineCount();
    const std::size_t labelCount = std::max(rowCount, std::size_t(GUTTER_MIN_LINES));
    // Clay retains the label strings until rendering completes.
    static std::vector<std::string> labels;
    labels.resize(labelCount);
    for (std::size_t line = 0; line < labelCount; ++line) {
        labels[line] = std::to_string(line + 1);
    }
    for (std::size_t line = 0; line < labelCount; ++line) {
        const bool trailingLine = line >= rowCount;
        const uint32_t rowId = file.LineElementId(trailingLine ? rowCount - 1 : line);
        const float offsetY = trailingLine ? (line - rowCount) * EDITOR_LINE_HEIGHT : 0;
        // Attach to this frame's completed row, so even newly resized nodes line up
        // immediately. Extra blank labels begin below the final rendered row.
        CLAY(CLAY_IDI("GutterLine", static_cast<uint32_t>(line)),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(GUTTER_WIDTH - 10),
                                    CLAY_SIZING_FIXED(EDITOR_LINE_HEIGHT)}},
              .floating = {.offset = {10 - GUTTER_WIDTH - EDITOR_PADDING, offsetY},
                           .parentId = rowId,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                            .parent = trailingLine ? CLAY_ATTACH_POINT_LEFT_BOTTOM
                                                                   : CLAY_ATTACH_POINT_LEFT_TOP},
                           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                           .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
                           .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT}}) {
            const Clay_String label = {.isStaticallyAllocated = false,
                                       .length = static_cast<int32_t>(labels[line].size()),
                                       .chars = labels[line].c_str()};
            CLAY_TEXT(label,
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BODY,
                       .fontSize = EDITOR_FONT_SIZE,
                       .lineHeight = EDITOR_LINE_HEIGHT,
                       .wrapMode = CLAY_TEXT_WRAP_NONE});
        }
    }
}

inline void LayoutLineNumberGutter(const Tab* activeFile,
                                   bool variableLineHeight) {
    // Line-number gutter from the mockup (same font size/line height as the editor text)
    static std::string lineNumbers;
    lineNumbers.clear();
    int lineCount = activeFile ? static_cast<int>(activeFile->LineCount()) : 1;
    if (lineCount < GUTTER_MIN_LINES) {
        lineCount = GUTTER_MIN_LINES;
    }
    for (int i = 1; i <= lineCount; ++i) {
        lineNumbers += std::to_string(i);
        if (i < lineCount) {
            lineNumbers += '\n';
        }
    }
    CLAY(CLAY_ID("Gutter"),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(GUTTER_WIDTH),
                                CLAY_SIZING_GROW(
                                    variableLineHeight
                                        ? float(lineCount * EDITOR_LINE_HEIGHT + EDITOR_PADDING)
                                        : 0)},
                     .padding = {10, 0, 6, 0}},
          .backgroundColor = GUTTER_COLOR}) {
        Clay_String numbersText = {.isStaticallyAllocated = false,
                                   .length = static_cast<int32_t>(lineNumbers.size()),
                                   .chars = lineNumbers.c_str()};
        if (activeFile && !variableLineHeight) {
            CLAY_TEXT(numbersText,
                      {.textColor = MENU_TEXT,
                       .fontId = FONT_BODY,
                       .fontSize = EDITOR_FONT_SIZE,
                       .lineHeight = EDITOR_LINE_HEIGHT,
                       .wrapMode = CLAY_TEXT_WRAP_NEWLINES});
        }
    }
}
}  // namespace layout_internal
