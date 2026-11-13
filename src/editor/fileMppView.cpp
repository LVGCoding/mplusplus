// Rendering and input for M++ documents (FileMpp).
// Loading and saving live in fileMpp.cpp.
//
// The cursor is a path into the node tree (see CursorPath in EditorNode.h).
// Arrow keys are sent down that path as a KeyEvent; each node either moves the
// cursor itself or reports that it is leaving, and its parent decides where it
// goes next. The document handles leaving a row.
//
// TODO: typing, deleting, selection and mouse clicks are not implemented yet.

#include "FileMpp.h"

#include "clay.h"
#include "math/TextNode.h"
#include "raylib.h"
#include "ui/EditorView.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <cmath>

namespace {
constexpr Color cursorColor = {20, 20, 20, 255};
constexpr float cursorWidth = 2.0f;

// Measures text the same way Clay does when laying out a TextNode.
TextMeasure MakeMeasure(Font* fonts) {
    return [fonts](std::string_view text, float fontSize) {
        Clay_StringSlice slice = {.length = static_cast<int32_t>(text.size()),
                                  .chars = text.data(),
                                  .baseChars = text.data()};
        Clay_TextElementConfig config = {.fontId = FONT_BODY,
                                         .fontSize = static_cast<uint16_t>(fontSize)};
        return ClayRaylib_MeasureText(slice, &config, fonts).width;
    };
}

Clay_ElementId RowId(std::size_t row) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("MppRow"), static_cast<uint32_t>(row));
}
}  // namespace

void FileMpp::LayoutContent() const {
    RenderContext context;
    context.fontSize = editor_style::fontSize;
    context.activeStructure = ActiveStructure();
    CLAY(CLAY_ID("MppDocument"), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                             .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        for (std::size_t i = 0; i < content.size(); ++i) {
            // Rows are at least one text line tall; rows holding fractions or
            // matrices grow to fit them.
            CLAY(RowId(i),
                 {.layout = {
                      .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(editor_style::lineHeight)},
                      .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                      .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
                content[i]->RenderChildren(context);
            }
        }
    }
}

TextNode* FileMpp::CursorText(const CursorPath& path, std::size_t* offset) const {
    if (path.empty() || path[0] >= content.size()) {
        return nullptr;
    }
    const EditorNode* node = content[path[0]].get();
    PathView rest = PathView(path).subspan(1);
    while (true) {
        if (auto* text = dynamic_cast<const TextNode*>(node)) {
            if (rest.size() != 1 || rest[0] > text->GetText().size()) {
                return nullptr;
            }
            if (offset) {
                *offset = rest[0];
            }
            return const_cast<TextNode*>(text);
        }
        std::size_t used = 0;
        node = node->PathChild(rest, used);
        if (!node) {
            return nullptr;
        }
        rest = rest.subspan(used);
    }
}

const EditorNode* FileMpp::ActiveStructure() const {
    if (cursorPos.empty() || cursorPos[0] >= content.size()) {
        return nullptr;
    }
    const EditorNode* active = nullptr;
    const EditorNode* node = content[cursorPos[0]].get();
    PathView rest = PathView(cursorPos).subspan(1);
    while (node) {
        std::size_t used = 0;
        const EditorNode* child = node->PathChild(rest, used);
        if (!child) {
            break;
        }
        if (node->OutlinesSlots()) {
            active = node;
        }
        node = child;
        rest = rest.subspan(used);
    }
    return active;
}

std::optional<Clay_BoundingBox> FileMpp::CaretBox(const TextMeasure* measure) const {
    std::size_t offset = 0;
    const TextNode* text = CursorText(cursorPos, &offset);
    return text ? text->CaretBox(offset, measure) : std::nullopt;
}

bool FileMpp::MoveCursor(int key, bool shift, const TextMeasure* measure) {
    if (!CursorText(cursorPos)) {
        // The tree changed under the cursor; start over at the beginning.
        cursorPos = JoinPath({0}, content.front()->FirstPosition());
    }
    const bool vertical = key == KEY_UP || key == KEY_DOWN;
    if (!vertical || cursorPos != goalPath_) {
        goalX_.reset();
    }
    if (vertical && !goalX_) {
        const auto caret = CaretBox(measure);
        goalX_ = caret ? caret->x : 0.0f;
    }

    const std::size_t row = cursorPos[0];
    KeyEvent event;
    event.key = key;
    event.shift = shift;
    event.path = PathView(cursorPos).subspan(1);
    event.cursorX = goalX_.value_or(0.0f);
    event.measure = measure;
    KeyResult result = content[row]->KeyPress(event);

    CursorPath next;
    switch (result.kind) {
    case KeyResult::Kind::Moved:
        next = JoinPath({row}, std::move(result.path));
        break;
    case KeyResult::Kind::ExitLeft:
        if (row > 0) {
            next = JoinPath({row - 1}, content[row - 1]->LastPosition());
        }
        break;
    case KeyResult::Kind::ExitRight:
        if (row + 1 < content.size()) {
            next = JoinPath({row + 1}, content[row + 1]->FirstPosition());
        }
        break;
    case KeyResult::Kind::ExitUp:
        if (row > 0) {
            next = JoinPath({row - 1}, content[row - 1]->PositionNearX(
                                           event.cursorX, VerticalEntry::FromBelow, measure));
        }
        break;
    case KeyResult::Kind::ExitDown:
        if (row + 1 < content.size()) {
            next = JoinPath({row + 1}, content[row + 1]->PositionNearX(
                                           event.cursorX, VerticalEntry::FromAbove, measure));
        }
        break;
    case KeyResult::Kind::Ignored:
        break;
    }

    if (next.empty() || next == cursorPos) {
        return false;
    }
    cursorPos = std::move(next);
    if (vertical) {
        goalPath_ = cursorPos;
    }
    if (!shift) {
        selectionStart = cursorPos;
    }
    return true;
}

bool FileMpp::HandleInput(const EditorView& view) {
    const TextMeasure measure = MakeMeasure(view.fonts);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool moved = false;
    for (const int key : {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN}) {
        if (IsKeyPressed(key) || IsKeyPressedRepeat(key)) {
            moved |= MoveCursor(key, shift, &measure);
        }
    }
    return moved;
}

std::size_t FileMpp::LineCount() const {
    // Newlines inside a matrix or another node belong to that node, not the gutter.
    return content.size();
}

uint32_t FileMpp::LineElementId(std::size_t line) const {
    return RowId(line).id;
}

void FileMpp::DrawOverlay(const EditorView& view) const {
    if (!view.cursorVisible) {
        return;
    }
    const auto bounds = CursorBounds(view);
    if (!bounds) {
        return;
    }
    BeginEditorClip(view);
    DrawRectangleRec(*bounds, cursorColor);
    EndScissorMode();
}

std::optional<Rectangle> FileMpp::CursorBounds(const EditorView& view) const {
    const TextMeasure measure = MakeMeasure(view.fonts);
    const auto caret = CaretBox(&measure);
    if (!caret) {
        return std::nullopt;
    }
    return Rectangle{std::round(caret->x - cursorWidth / 2), caret->y, cursorWidth, caret->height};
}
