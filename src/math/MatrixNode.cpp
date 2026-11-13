#include "MatrixNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

MatrixNode::MatrixNode() {
    for (int row = 0; row < 2; ++row) {
        Row cells;
        cells.push_back(std::make_unique<LineNode>());
        cells.push_back(std::make_unique<LineNode>());
        cells_.push_back(std::move(cells));
    }
}

MatrixNode::MatrixNode(std::string environment, std::vector<Row> cells)
    : cells_(std::move(cells)), environment_(std::move(environment)) {}

namespace {
struct Delimiters {
    bool present;
    node_style::Delimiter kind;
};

Delimiters DelimitersFor(const std::string& environment) {
    using node_style::Delimiter;
    if (environment == "pmatrix") return {true, Delimiter::Paren};
    if (environment == "bmatrix") return {true, Delimiter::Bracket};
    if (environment == "Bmatrix") return {true, Delimiter::Brace};
    if (environment == "vmatrix") return {true, Delimiter::Bar};
    if (environment == "Vmatrix") return {true, Delimiter::DoubleBar};
    return {false, Delimiter::Paren};
}
}  // namespace

Clay_ElementId MatrixNode::CellId(std::size_t row, std::size_t column) const {
    // Rows can be ragged, so number cells by a fixed stride rather than by
    // the current column count.
    return Clay__HashStringWithOffset(CLAY_STRING("MatrixCell"),
                                      static_cast<uint32_t>(row * 1024 + column), GetId());
}

// Clay has no grid layout, so the matrix is laid out as columns of centred
// cells. To keep rows lined up across columns, each cell is given at least the
// height of the tallest cell in its row, measured from the previous frame
// (cell ids are stable, so last frame's layout can be looked up).
void MatrixNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    std::size_t columns = 0;
    for (const auto& row : cells_) {
        columns = std::max(columns, row.size());
    }

    std::vector<float> rowHeights(cells_.size(), 0.0f);
    for (std::size_t row = 0; row < cells_.size(); ++row) {
        for (std::size_t column = 0; column < cells_[row].size(); ++column) {
            if (const auto bounds = LastLayoutBounds(CellId(row, column))) {
                rowHeights[row] = std::max(rowHeights[row], bounds->height);
            }
        }
    }

    const Delimiters delimiters = DelimitersFor(environment_);
    const auto columnGap = static_cast<uint16_t>(std::round(fontSize));
    const auto rowGap = static_cast<uint16_t>(std::round(fontSize * 0.3f));
    const auto pad = static_cast<uint16_t>(std::round(fontSize * 0.15f));
    const bool outlined = context.activeStructure == this;
    const DeleteHighlight& marked = context.deleteHighlight;
    // Cells in the row/column marked for deletion are drawn in red.
    auto markedForDelete = [&](std::size_t row, std::size_t column) {
        if (marked.node != this) {
            return false;
        }
        switch (marked.part) {
        case DeleteHighlight::Part::Row:
            return row == marked.index;
        case DeleteHighlight::Part::Column:
            return column == marked.index;
        case DeleteHighlight::Part::Whole:
            return true;
        }
        return false;
    };

    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                  .childGap = pad,
                                  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                                  .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        if (delimiters.present) {
            node_style::DelimiterSymbol(delimiters.kind, false, fontSize);
        }
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .padding = {pad, pad, pad, pad},
                                 .childGap = columnGap,
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
            for (std::size_t column = 0; column < columns; ++column) {
                CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                         .childGap = rowGap,
                                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                                         .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                    for (std::size_t row = 0; row < cells_.size(); ++row) {
                        // Row slot: at least as tall as the row's tallest cell.
                        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0),
                                                            CLAY_SIZING_FIT(rowHeights[row])},
                                                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                                                    .y = CLAY_ALIGN_Y_CENTER}}}) {
                            // A ragged row leaves the slot empty.
                            if (column < cells_[row].size()) {
                                const bool red = markedForDelete(row, column);
                                const bool border = red || outlined;
                                CLAY(CellId(row, column),
                                     {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                                 .padding = CLAY_PADDING_ALL(1)},
                                      .backgroundColor = red ? node_style::deleteFillColor
                                                             : Clay_Color{0, 0, 0, 0},
                                      .border = {.color = red ? node_style::deleteColor
                                                              : node_style::slotOutlineColor,
                                                 .width = border ? Clay_BorderWidth{1, 1, 1, 1, 0}
                                                                 : Clay_BorderWidth{0, 0, 0, 0, 0}}}) {
                                    if (cells_[row][column]) {
                                        cells_[row][column]->Render(context);
                                    } else {
                                        node_style::Placeholder(fontSize);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (delimiters.present) {
            node_style::DelimiterSymbol(delimiters.kind, true, fontSize);
        }
    }
}

KeyResult MatrixNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* cell = PathChild(event.path, used);
    if (!cell) {
        return {};
    }
    const std::size_t row = event.path[0];
    const std::size_t column = event.path[1];
    if (event.codepoint == U'&') {
        return {};  // would split the cell when saved
    }
    KeyResult result = cell->KeyPress(event.Descend(used));
    if (result.kind == KeyResult::Kind::Ignored && event.key == KEY_ENTER) {
        return KeyResult::WithPath(KeyResult::Kind::MatrixInsert, {row, column});
    }
    if (result.kind == KeyResult::Kind::DeleteLeft) {
        return KeyResult::WithPath(KeyResult::Kind::MatrixLine, {row, column});
    }
    if (result.kind == KeyResult::Kind::DeleteRight) {
        return {};
    }

    // Cell (r, c) if it exists and can hold the cursor.
    auto enter = [&](std::size_t r, std::size_t c, auto&& position) -> KeyResult {
        EditorNode* target = GetCell(r, c);
        if (!target) {
            return {};
        }
        CursorPath path = position(*target);
        if (path.empty()) {
            return {};
        }
        return KeyResult::Moved(JoinPath({r, c}, std::move(path)));
    };
    auto nearX = [&](VerticalEntry entry) {
        return [&event, entry](const EditorNode& node) {
            return node.PositionNearX(event.cursorX, entry, event.measure);
        };
    };

    KeyResult moved;
    switch (result.kind) {
    case KeyResult::Kind::Moved:
        return std::move(result).Prepend({row, column});
    case KeyResult::Kind::ExitLeft:
        if (column > 0) {
            moved = enter(row, column - 1, [](const EditorNode& n) { return n.LastPosition(); });
        }
        break;
    case KeyResult::Kind::ExitRight:
        moved = enter(row, column + 1, [](const EditorNode& n) { return n.FirstPosition(); });
        break;
    case KeyResult::Kind::ExitUp:
        if (row > 0) {
            const std::size_t above = ColumnCount(row - 1);
            if (above > 0) {
                moved = enter(row - 1, std::min(column, above - 1), nearX(VerticalEntry::FromBelow));
            }
        }
        break;
    case KeyResult::Kind::ExitDown:
        if (row + 1 < cells_.size()) {
            const std::size_t below = ColumnCount(row + 1);
            if (below > 0) {
                moved = enter(row + 1, std::min(column, below - 1), nearX(VerticalEntry::FromAbove));
            }
        }
        break;
    default:
        break;
    }
    if (moved.kind == KeyResult::Kind::Moved) {
        return moved;
    }
    return std::move(result).Prepend({row, column});
}

void MatrixNode::MouseClick() {}
std::pair<float, float> MatrixNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> MatrixNode::GetSize() {
    return {0.0f, 0.0f};
}

// Entering from the left lands in the first cell, from the right in the last
// cell of the first row.
CursorPath MatrixNode::FirstPosition() const {
    EditorNode* cell = GetCell(0, 0);
    return cell ? JoinPath({0, 0}, cell->FirstPosition()) : CursorPath{};
}

CursorPath MatrixNode::LastPosition() const {
    const std::size_t columns = ColumnCount(0);
    EditorNode* cell = columns > 0 ? GetCell(0, columns - 1) : nullptr;
    return cell ? JoinPath({0, columns - 1}, cell->LastPosition()) : CursorPath{};
}

// From above, the top row; from below, the bottom row; in the column nearest x.
CursorPath MatrixNode::PositionNearX(float x, VerticalEntry entry,
                                     const TextMeasure* measure) const {
    if (cells_.empty()) {
        return {};
    }
    const std::size_t row = entry == VerticalEntry::FromAbove ? 0 : cells_.size() - 1;
    std::size_t best = 0;
    float bestDistance = -1.0f;
    for (std::size_t column = 0; column < cells_[row].size(); ++column) {
        const auto bounds = LastLayoutBounds(CellId(row, column));
        if (!bounds) {
            continue;
        }
        const float centre = bounds->x + bounds->width / 2;
        const float distance = std::abs(centre - x);
        if (bestDistance < 0 || distance < bestDistance) {
            best = column;
            bestDistance = distance;
        }
    }
    EditorNode* cell = GetCell(row, best);
    return cell ? JoinPath({row, best}, cell->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* MatrixNode::PathChild(PathView path, std::size_t& used) const {
    if (path.size() < 2) {
        return nullptr;
    }
    used = 2;
    return GetCell(path[0], path[1]);
}

std::size_t MatrixNode::MaxColumnCount() const {
    std::size_t columns = 0;
    for (const auto& row : cells_) {
        columns = std::max(columns, row.size());
    }
    return columns;
}

std::optional<Clay_BoundingBox> MatrixNode::CellBounds(std::size_t row, std::size_t column) const {
    return LastLayoutBounds(CellId(row, column));
}

namespace {
std::unique_ptr<EditorNode> EmptyCell() {
    auto cell = std::make_unique<LineNode>();
    cell->Normalize();
    return cell;
}
}  // namespace

void MatrixNode::InsertRow(std::size_t row) {
    row = std::min(row, cells_.size());
    Row cells;
    for (std::size_t i = 0; i < std::max<std::size_t>(1, MaxColumnCount()); ++i) {
        cells.push_back(EmptyCell());
    }
    cells_.insert(cells_.begin() + static_cast<std::ptrdiff_t>(row), std::move(cells));
}

void MatrixNode::InsertColumn(std::size_t column) {
    for (auto& row : cells_) {
        const std::size_t at = std::min(column, row.size());
        row.insert(row.begin() + static_cast<std::ptrdiff_t>(at), EmptyCell());
    }
}

bool MatrixNode::RemoveRow(std::size_t row) {
    if (cells_.size() <= 1 || row >= cells_.size()) {
        return false;
    }
    cells_.erase(cells_.begin() + static_cast<std::ptrdiff_t>(row));
    return true;
}

bool MatrixNode::RemoveColumn(std::size_t column) {
    if (MaxColumnCount() <= 1) {
        return false;
    }
    for (const auto& row : cells_) {
        if (row.size() == 1 && column == 0) {
            return false;
        }
    }
    for (auto& row : cells_) {
        if (column < row.size()) {
            row.erase(row.begin() + static_cast<std::ptrdiff_t>(column));
        }
    }
    return true;
}

void MatrixNode::Normalize() {
    for (auto& row : cells_) {
        for (auto& cell : row) {
            cell = LineNode::EnsureLine(std::move(cell));
            cell->Normalize();
        }
    }
}

std::string MatrixNode::ToText() const {
    std::string text = "\\begin{" + environment_ + "}";
    for (std::size_t row = 0; row < cells_.size(); ++row) {
        if (row > 0) {
            text += "\\\\";
        }
        for (std::size_t column = 0; column < cells_[row].size(); ++column) {
            if (column > 0) {
                text += "&";
            }
            if (cells_[row][column]) {
                text += cells_[row][column]->ToText();
            }
        }
    }
    text += "\\end{" + environment_ + "}";
    return text;
}

bool MatrixNode::HasChild() {
    return !cells_.empty();
}

std::size_t MatrixNode::RowCount() const {
    return cells_.size();
}

std::size_t MatrixNode::ColumnCount(std::size_t row) const {
    return row < cells_.size() ? cells_[row].size() : 0;
}

EditorNode* MatrixNode::GetCell(std::size_t row, std::size_t column) const {
    if (row >= cells_.size() || column >= cells_[row].size()) {
        return nullptr;
    }
    return cells_[row][column].get();
}

const std::string& MatrixNode::GetEnvironment() const {
    return environment_;
}

bool MatrixNode::IsMatrixEnvironment(const std::string& name) {
    static const std::array<std::string_view, 6> environments = {
        "matrix", "pmatrix", "bmatrix", "Bmatrix", "vmatrix", "Vmatrix"};
    for (const auto environment : environments) {
        if (environment == name) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<EditorNode> MatrixNode::Clone() const {
    std::vector<Row> cells;
    cells.reserve(cells_.size());
    for (const Row& row : cells_) {
        Row copied;
        copied.reserve(row.size());
        for (const auto& cell : row) {
            copied.push_back(CloneOf(cell));
        }
        cells.push_back(std::move(copied));
    }
    auto copy = std::make_unique<MatrixNode>(environment_, std::move(cells));
    copy->CopyIdFrom(*this);
    return copy;
}
