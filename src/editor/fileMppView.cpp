// Rendering and input for M++ documents (FileMpp).
// Loading and saving live in fileMpp.cpp.
//
// The cursor is a path into the node tree (see CursorPath in EditorNode.h).
// Arrow keys are sent down that path as a KeyEvent; each node either moves the
// cursor itself or reports that it is leaving, and its parent decides where it
// goes next. The document handles leaving a row.
//
// Editing (typing, deleting, the matrix insert wheel) is in fileMppEdit.cpp,
// and what a selection covers is in fileMppSelection.cpp.

#include "FileMpp.h"

#include "clay.h"
#include "math/FracNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"
#include "math/TextNode.h"
#include "raylib.h"
#include "ui/EditorView.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr Color cursorColor = {20, 20, 20, 255};
constexpr float cursorWidth = 2.0f;
constexpr Color selectionColor = {65, 105, 168, 80};  // same as plain text files

// Matrix insert wheel.
constexpr float wheelGap = 6.0f;          // between the cell and the ring
constexpr float wheelThickness = 52.0f;   // width of the ring
constexpr float wheelFontSize = 13.0f;
constexpr Color wheelFill = {245, 245, 245, 235};
constexpr Color wheelChosen = {46, 160, 67, 240};
constexpr Color wheelEdge = {150, 150, 150, 255};
constexpr Color wheelShadow = {0, 0, 0, 40};
constexpr Color wheelText = {40, 40, 40, 255};

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

float DistanceToInterval(float value, float start, float length) {
    return std::max({start - value, value - start - length, 0.0f});
}

// Descend through rendered boxes so a click chooses the correct slot before
// choosing a text offset. In particular, x alone cannot distinguish fraction
// slots or matrix rows. All coordinates already include scrolling.
CursorPath NodePositionAt(const EditorNode& node, float x, float y, const TextMeasure* measure) {
    if (dynamic_cast<const TextNode*>(&node)) {
        return node.PositionNearX(x, VerticalEntry::FromAbove, measure);
    }

    const EditorNode* best = nullptr;
    CursorPath prefix;
    float distance = std::numeric_limits<float>::max();
    auto consider = [&](const EditorNode* child, CursorPath path,
                        std::optional<Clay_BoundingBox> bounds) {
        if (!child || !bounds) return;
        const float dx = DistanceToInterval(x, bounds->x, bounds->width);
        const float dy = DistanceToInterval(y, bounds->y, bounds->height);
        const float candidate = dx * dx + dy * dy;
        // Text wins shared boundaries, including zero-width insertion points.
        if (candidate < distance ||
            (candidate == distance && dynamic_cast<const TextNode*>(child))) {
            best = child;
            prefix = std::move(path);
            distance = candidate;
        }
    };
    if (const auto* line = dynamic_cast<const LineNode*>(&node)) {
        for (std::size_t i = 0; i < line->ChildCount(); ++i) {
            const auto* child = line->GetChild(i);
            consider(child, {i}, LastLayoutBounds(child->GetClayId()));
        }
        // Symbols cannot hold the cursor; resolve to the text on either side.
        if (best && best->FirstPosition().empty()) {
            const auto bounds = LastLayoutBounds(best->GetClayId());
            const auto i = prefix.front();
            if (bounds && x < bounds->x + bounds->width / 2 && i > 0) {
                return JoinPath({i - 1}, line->GetChild(i - 1)->LastPosition());
            }
            if (i + 1 < line->ChildCount()) {
                return JoinPath({i + 1}, line->GetChild(i + 1)->FirstPosition());
            }
        }
    } else if (const auto* fraction = dynamic_cast<const FracNode*>(&node)) {
        const auto [numerator, denominator] = fraction->GetChildren();
        if (numerator) consider(numerator, {0}, LastLayoutBounds(numerator->GetClayId()));
        if (denominator) consider(denominator, {1}, LastLayoutBounds(denominator->GetClayId()));
    } else if (const auto* root = dynamic_cast<const SqrtNode*>(&node)) {
        const auto [index, argument] = root->GetChildren();
        if (index) consider(index, {0}, LastLayoutBounds(index->GetClayId()));
        if (argument) consider(argument, {1}, LastLayoutBounds(argument->GetClayId()));
    } else if (const auto* matrix = dynamic_cast<const MatrixNode*>(&node)) {
        for (std::size_t row = 0; row < matrix->RowCount(); ++row) {
            for (std::size_t column = 0; column < matrix->ColumnCount(row); ++column) {
                consider(matrix->GetCell(row, column), {row, column}, matrix->CellBounds(row, column));
            }
        }
    }
    if (!best) return node.FirstPosition();
    auto position = NodePositionAt(*best, x, y, measure);
    if (position.empty()) return {};
    prefix.insert(prefix.end(), position.begin(), position.end());
    return prefix;
}

// Screen box covering a line from [startIndex, startOffset] to
// [endIndex, endOffset]: selected text plus whole objects in between. Starts
// as the (zero-width) caret box at the start, so it is never empty.
std::optional<Rectangle> RangeBox(const LineNode& line, std::size_t startIndex,
                                  std::size_t startOffset, std::size_t endIndex,
                                  std::size_t endOffset, const TextMeasure& measure) {
    const auto* first = dynamic_cast<const TextNode*>(line.GetChild(startIndex));
    const auto caret = first ? first->CaretBox(startOffset, &measure) : std::nullopt;
    if (!caret) {
        return std::nullopt;
    }
    float left = caret->x;
    float top = caret->y;
    float right = caret->x;
    float bottom = caret->y + caret->height;
    auto add = [&](float x0, float y0, float x1, float y1) {
        left = std::min(left, x0);
        top = std::min(top, y0);
        right = std::max(right, x1);
        bottom = std::max(bottom, y1);
    };
    for (std::size_t i = startIndex; i <= endIndex && i < line.ChildCount(); ++i) {
        const EditorNode* child = line.GetChild(i);
        if (const auto* text = dynamic_cast<const TextNode*>(child)) {
            const std::size_t from = i == startIndex ? startOffset : 0;
            const std::size_t to = i == endIndex ? endOffset : text->GetText().size();
            const auto a = text->CaretBox(from, &measure);
            const auto b = text->CaretBox(to, &measure);
            if (from < to && a && b) {
                add(a->x, a->y, b->x, b->y + b->height);
            }
        } else if (const auto bounds = LastLayoutBounds(child->GetClayId())) {
            add(bounds->x, bounds->y, bounds->x + bounds->width, bounds->y + bounds->height);
        }
    }
    return Rectangle{left, top, right - left, bottom - top};
}
}  // namespace

std::optional<CursorPath> FileMpp::PositionAt(float x, float y,
                                             const TextMeasure* measure) const {
    std::optional<std::size_t> row;
    float distance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < content.size(); ++i) {
        const auto bounds = LastLayoutBounds(RowId(i));
        if (!bounds) continue;
        const float candidate = DistanceToInterval(y, bounds->y, bounds->height);
        if (candidate < distance || (candidate == distance && y >= bounds->y)) {
            row = i;
            distance = candidate;
        }
    }
    if (!row) return std::nullopt;
    auto position = JoinPath({*row}, NodePositionAt(*content[*row], x, y, measure));
    if (!CursorText(position)) return std::nullopt;
    return position;
}

bool FileMpp::ClickAt(float x, float y, const TextMeasure* measure) {
    auto position = PositionAt(x, y, measure);
    if (!position) return false;
    cursorPos = std::move(*position);
    selectionStart = cursorPos;
    goalX_.reset();
    goalPath_.clear();
    pendingDelete_.reset();
    insertWheel_.reset();
    return true;
}

void FileMpp::LayoutContent() const {
    RenderContext context;
    context.fontSize = editor_style::fontSize;
    context.activeStructure = ActiveStructure();
    if (pendingDelete_) {
        context.deleteHighlight.node = NodeAt(pendingDelete_->node);
        context.deleteHighlight.part = pendingDelete_->part;
        context.deleteHighlight.index = pendingDelete_->part == DeleteHighlight::Part::Column
                                            ? pendingDelete_->column
                                            : pendingDelete_->row;
    }
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

bool FileMpp::HandleInput(const EditorView& view) {
    const TextMeasure measure = MakeMeasure(view.fonts);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool changed = HandleMouse(view, measure);
    if (ctrl && IsKeyPressed(KEY_A)) {
        SelectAll();
        changed = true;
    }
    // Use Command on macOS and Control elsewhere for undo/redo.
#if defined(__APPLE__)
    const bool historyModifier = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
#else
    const bool historyModifier = ctrl;
#endif
    // Modifier+Z undoes; Modifier+Y or Modifier+Shift+Z redoes. Held keys repeat.
    auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    if (historyModifier && pressed(KEY_Z)) {
        changed |= shift ? Redo() : Undo();
    }
    if (historyModifier && pressed(KEY_Y)) {
        changed |= Redo();
    }
    for (const int key : {KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_BACKSPACE, KEY_DELETE}) {
        if (IsKeyPressed(key) || IsKeyPressedRepeat(key)) {
            changed |= PressKey(key, shift, &measure);
        }
    }
    for (const int key : {KEY_ENTER, KEY_KP_ENTER, KEY_TAB, KEY_ESCAPE}) {
        if (IsKeyPressed(key)) {
            changed |= PressKey(key == KEY_KP_ENTER ? KEY_ENTER : key, shift, &measure);
        }
    }
    while (const int codepoint = GetCharPressed()) {
        changed |= TypeCharacter(static_cast<char32_t>(codepoint));
        if (mathMenuRequested_) {
            break;  // the rest of this frame's typing belongs to the popup
        }
    }
    return changed;
}

// Click to place the cursor, drag or Shift+click to select.
bool FileMpp::HandleMouse(const EditorView& view, const TextMeasure& measure) {
    if (!view.acceptsMouse || !IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        dragging_ = false;
        return false;
    }
    const Vector2 mouse = GetMousePosition();
    const bool clicked =
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, view.viewport);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (clicked) {
        dragging_ = true;
        if (!shift) {
            return ClickAt(mouse.x, mouse.y, &measure);
        }
        pendingDelete_.reset();
        insertWheel_.reset();
    }
    if (!dragging_) {
        return false;
    }
    // Dragging or Shift+click: move only the cursor end of the selection.
    auto position = PositionAt(mouse.x, mouse.y, &measure);
    if (!position || *position == cursorPos) {
        return clicked;
    }
    cursorPos = std::move(*position);
    goalX_.reset();
    return true;
}

std::size_t FileMpp::LineCount() const {
    // Newlines inside a matrix or another node belong to that node, not the gutter.
    return content.size();
}

uint32_t FileMpp::LineElementId(std::size_t line) const {
    return RowId(line).id;
}

void FileMpp::DrawOverlay(const EditorView& view) const {
    DrawSelection(view);
    if (view.cursorVisible) {
        if (const auto bounds = CursorBounds(view)) {
            BeginEditorClip(view);
            DrawRectangleRec(*bounds, cursorColor);
            EndScissorMode();
        }
    }
    if (insertWheel_) {
        DrawInsertWheel(view);
    }
}

// One translucent box per row: from the start to the end of the selection in
// that row, as tall as what is selected. A selected line break shows as a
// little extra width at the end of the row, as in plain text files.
void FileMpp::DrawSelection(const EditorView& view) const {
    const auto selection = GetSelection();
    if (!selection) {
        return;
    }
    const TextMeasure measure = MakeMeasure(view.fonts);
    const CursorPath& start = selection->start;
    const CursorPath& end = selection->end;
    BeginEditorClip(view);
    if (start[0] == end[0]) {
        const CursorPath linePath(start.begin(), start.end() - 2);
        if (const auto* line = dynamic_cast<const LineNode*>(NodeAt(linePath))) {
            if (const auto box = RangeBox(*line, start[start.size() - 2], start.back(),
                                          end[end.size() - 2], end.back(), measure)) {
                DrawRectangleRec(*box, selectionColor);
            }
        }
    } else {
        for (std::size_t row = start[0]; row <= end[0]; ++row) {
            // Skip rows that are off screen.
            const auto rowBounds = LastLayoutBounds(RowId(row));
            if (!rowBounds || rowBounds->y > view.viewport.y + view.viewport.height ||
                rowBounds->y + rowBounds->height < view.viewport.y) {
                continue;
            }
            const LineNode& line = *content[row];
            const CursorPath from = row == start[0] ? CursorPath{start[1], start[2]}
                                                    : line.FirstPosition();
            const CursorPath to = row == end[0] ? CursorPath{end[1], end[2]} : line.LastPosition();
            auto box = RangeBox(line, from[0], from[1], to[0], to[1], measure);
            if (!box) {
                continue;
            }
            if (row != end[0]) {
                box->width += editor_style::fontSize / 2.0f;
            }
            if (box->width > 0) {
                DrawRectangleRec(*box, selectionColor);
            }
        }
    }
    EndScissorMode();
}

// A ring around the matrix cell split into four wedges, one per direction.
// The chosen wedge is highlighted; Enter inserts a row/column that way.
void FileMpp::DrawInsertWheel(const EditorView& view) const {
    const auto* matrix = dynamic_cast<const MatrixNode*>(NodeAt(insertWheel_->matrix));
    const auto cell = matrix ? matrix->CellBounds(insertWheel_->row, insertWheel_->column)
                             : std::nullopt;
    if (!cell) {
        return;
    }
    const Vector2 centre = {cell->x + cell->width / 2, cell->y + cell->height / 2};
    const float inner = std::max(cell->width, cell->height) / 2 + wheelGap;
    const float outer = inner + wheelThickness;

    struct Wedge {
        WheelDirection direction;
        float angle;  // centre of the wedge in raylib degrees (0 = right, 90 = down)
        const char* label;
    };
    static constexpr Wedge wedges[] = {
        {WheelDirection::Right, 0.0f, "Col right"},
        {WheelDirection::Down, 90.0f, "Row below"},
        {WheelDirection::Left, 180.0f, "Col left"},
        {WheelDirection::Up, 270.0f, "Row above"},
    };

    DrawCircleV(centre, outer + 1, wheelShadow);
    for (const Wedge& wedge : wedges) {
        const bool chosen = insertWheel_->direction == wedge.direction;
        DrawRing(centre, inner, outer, wedge.angle - 44, wedge.angle + 44, 24,
                 chosen ? wheelChosen : wheelFill);
    }
    DrawRing(centre, inner - 1, inner, 0, 360, 48, wheelEdge);
    DrawRing(centre, outer, outer + 1, 0, 360, 48, wheelEdge);

    const Font font = view.fonts ? view.fonts[FONT_BODY] : GetFontDefault();
    const float radius = (inner + outer) / 2;
    for (const Wedge& wedge : wedges) {
        const bool chosen = insertWheel_->direction == wedge.direction;
        const float radians = wedge.angle * DEG2RAD;
        const Vector2 size = MeasureTextEx(font, wedge.label, wheelFontSize, 0);
        const Vector2 at = {centre.x + std::cos(radians) * radius - size.x / 2,
                            centre.y + std::sin(radians) * radius - size.y / 2};
        DrawTextEx(font, wedge.label, at, wheelFontSize, 0, chosen ? WHITE : wheelText);
    }
}

std::optional<Rectangle> FileMpp::CursorBounds(const EditorView& view) const {
    const TextMeasure measure = MakeMeasure(view.fonts);
    const auto caret = CaretBox(&measure);
    if (!caret) {
        return std::nullopt;
    }
    return Rectangle{std::round(caret->x - cursorWidth / 2), caret->y, cursorWidth, caret->height};
}
