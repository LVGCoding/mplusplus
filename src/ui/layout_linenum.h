#pragma once

// Line-number gutter layout for fixed-height text and dynamically sized math rows.

#include "layout_common.h"
#include "layout_style.h"

#include <vector>

namespace layout_internal {
inline void LayoutRowLineNumbers(const FileBase& file) {
    const std::size_t rowCount = file.LineCount();
    // Clay retains the label strings until rendering completes.
    static std::vector<std::string> labels;
    labels.resize(rowCount);
    for (std::size_t line = 0; line < rowCount; ++line) {
        labels[line] = std::to_string(line + 1);
    }
    for (std::size_t line = 0; line < rowCount; ++line) {
        const uint32_t rowId = file.LineElementId(line);
        // Attach to this frame's completed row, so even newly resized nodes line up
        // immediately. Only document rows receive labels.
        CLAY(CLAY_IDI("GutterLine", static_cast<uint32_t>(line)),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(GUTTER_WIDTH - 10),
                                    CLAY_SIZING_FIXED(EDITOR_LINE_HEIGHT)}},
              .floating = {.offset = {10 - GUTTER_WIDTH - EDITOR_PADDING, 0},
                           .parentId = rowId,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                            .parent = CLAY_ATTACH_POINT_LEFT_TOP},
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

inline void LayoutLineNumberGutter(const FileBase* activeFile,
                                   bool variableLineHeight) {
    // Line-number gutter from the mockup (same font size/line height as the editor text)
    static std::string lineNumbers;
    lineNumbers.clear();
    const std::size_t lineCount = activeFile ? activeFile->LineCount() : 0;
    for (std::size_t line = 0; line < lineCount; ++line) {
        lineNumbers += std::to_string(line + 1);
        if (line + 1 < lineCount) {
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
