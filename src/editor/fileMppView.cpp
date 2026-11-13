// Rendering and input for M++ documents (FileMpp).
// Loading and saving live in fileMpp.cpp.
//
// The cursor is a path into the node tree (see CursorPath in EditorNode.h).
// Arrow keys are sent down that path as a KeyEvent; each node either moves the
// cursor itself or reports that it is leaving, and its parent decides where it
// goes next. The document handles leaving a row.
//
// Editing (typing, deleting, the matrix insert wheel) is in fileMppEdit.cpp.
// TODO: selection and mouse clicks are not implemented yet.

#include "FileMpp.h"

#include "clay.h"
#include "math/MatrixNode.h"
#include "math/TextNode.h"
#include "raylib.h"
#include "ui/EditorView.h"
#include "ui/clay_renderer_raylib.h"
#include "ui/font.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr Color cursorColor = {20, 20, 20, 255};
constexpr float cursorWidth = 2.0f;

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
}  // namespace

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
    bool changed = false;
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

std::size_t FileMpp::LineCount() const {
    // Newlines inside a matrix or another node belong to that node, not the gutter.
    return content.size();
}

uint32_t FileMpp::LineElementId(std::size_t line) const {
    return RowId(line).id;
}

void FileMpp::DrawOverlay(const EditorView& view) const {
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
