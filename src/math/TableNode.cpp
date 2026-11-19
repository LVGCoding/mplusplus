#include "TableNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr Clay_Color GRID_COLOR = {190, 190, 190, 255};
constexpr Clay_Color HEADER_COLOR = {236, 236, 236, 255};
constexpr Clay_Color TABLE_COLOR = {255, 255, 255, 255};

std::unique_ptr<EditorNode> EmptyLine() {
    auto line = std::make_unique<LineNode>();
    line->Normalize();
    return line;
}

std::vector<MatrixNode::Row> EmptyCells(std::size_t rows,
                                        std::size_t columns) {
    std::vector<MatrixNode::Row> cells(rows);
    for (auto& row : cells) {
        for (std::size_t column = 0; column < columns; ++column) {
            row.push_back(EmptyLine());
        }
    }
    return cells;
}
}  // namespace

TableNode::TableNode() : TableNode(nullptr, false, {}, EmptyCells(3, 3)) {}

TableNode::TableNode(std::unique_ptr<EditorNode> title,
                     bool header,
                     std::vector<options::Option> unknown,
                     std::vector<Row> cells)
    : MatrixNode(ENVIRONMENT, std::move(cells)), title_(LineNode::EnsureLine(std::move(title))),
      header_(header), unknown_(std::move(unknown)) {
    title_->Normalize();
    MakeRectangular();
}

void TableNode::MakeRectangular() {
    if (cells_.empty()) {
        cells_.emplace_back();
    }
    const std::size_t columns = std::max<std::size_t>(1, MaxColumnCount());
    for (auto& row : cells_) {
        while (row.size() < columns) {
            row.push_back(EmptyLine());
        }
    }
}

void TableNode::Normalize() {
    title_ = LineNode::EnsureLine(std::move(title_));
    title_->Normalize();
    MakeRectangular();
    MatrixNode::Normalize();
}

// --- Rows, columns and the "+" buttons --------------------------------------------

std::size_t TableNode::InsertRow(std::size_t row) {
    if (header_ && row == 0) {
        row = 1;
    }
    return MatrixNode::InsertRow(row);
}

bool TableNode::RemoveRow(std::size_t row) {
    if (!MatrixNode::RemoveRow(row)) {
        return false;
    }
    if (row == 0) {
        header_ = false;
    }
    return true;
}

bool TableNode::RunAction(int action) {
    switch (action) {
    case ADD_ROW:
        InsertRow(RowCount());
        break;
    case ADD_COLUMN:
        InsertColumn(ColumnCount());
        break;
    case ADD_HEADER:
        if (header_) {
            return false;
        }
        MatrixNode::InsertRow(0);
        header_ = true;
        break;
    default:
        return false;
    }
    Normalize();
    return true;
}

// --- Cursor -----------------------------------------------------------------------

KeyResult TableNode::KeyPress(const KeyEvent& event) {
    if (event.path.empty()) {
        return {};
    }
    if (event.path[0] == TITLE_ROW) {
        if (event.path.size() < 2) {
            return {};
        }
        KeyResult result = title_->KeyPress(event.Descend(2));
        switch (result.kind) {
        case KeyResult::Kind::EXIT_DOWN:
            return KeyResult::Moved(
                MatrixNode::PositionNearX(event.cursorX, VerticalEntry::FROM_ABOVE, event.measure));
        case KeyResult::Kind::EXIT_RIGHT:
            return KeyResult::Moved(MatrixNode::FirstPosition());
        case KeyResult::Kind::DELETE_LEFT:
            return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
        case KeyResult::Kind::DELETE_RIGHT:
            return {};
        default:
            return std::move(result).Prepend({TITLE_ROW, 0});
        }
    }

    KeyResult result = MatrixNode::KeyPress(event);
    const bool topRow = event.path[0] == 0;
    const bool firstCell = topRow && event.path.size() > 1 && event.path[1] == 0;
    if (result.kind == KeyResult::Kind::EXIT_UP && topRow) {
        return KeyResult::Moved(JoinPath(
            {TITLE_ROW, 0},
            title_->PositionNearX(event.cursorX, VerticalEntry::FROM_BELOW, event.measure)));
    }
    if (result.kind == KeyResult::Kind::EXIT_LEFT && firstCell) {
        return KeyResult::Moved(JoinPath({TITLE_ROW, 0}, title_->LastPosition()));
    }
    return result;
}

CursorPath TableNode::FirstPosition() const {
    return JoinPath({TITLE_ROW, 0}, title_->FirstPosition());
}

CursorPath TableNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    if (entry == VerticalEntry::FROM_ABOVE) {
        return JoinPath({TITLE_ROW, 0}, title_->PositionNearX(x, entry, measure));
    }
    return MatrixNode::PositionNearX(x, entry, measure);
}

EditorNode* TableNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.size() >= 2 && path[0] == TITLE_ROW) {
        used = 2;
        return title_.get();
    }
    return MatrixNode::PathChild(path, used);
}

std::vector<EditorNode::ChildPath> TableNode::ChildPaths() const {
    std::vector<ChildPath> children = {{{TITLE_ROW, 0}, title_.get()}};
    for (ChildPath& cell : MatrixNode::ChildPaths()) {
        children.push_back(std::move(cell));
    }
    return children;
}

// --- Drawing ----------------------------------------------------------------------

void TableNode::RenderCompact(const RenderContext& context) {
    // A small 3x3 grid icon.
    const float cell = std::round(context.fontSize * 0.55f);
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = TABLE_COLOR,
          .border = {.color = node_style::TEXT_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        for (int row = 0; row < 3; ++row) {
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
                for (int column = 0; column < 3; ++column) {
                    CLAY_AUTO_ID(
                        {.layout = {.sizing = {CLAY_SIZING_FIXED(cell * 1.4f),
                                               CLAY_SIZING_FIXED(cell)}},
                         .backgroundColor = row == 0 ? HEADER_COLOR : TABLE_COLOR,
                         .border = {.color = GRID_COLOR,
                                    .width = {.right = static_cast<uint16_t>(column < 2 ? 1 : 0),
                                              .bottom = static_cast<uint16_t>(row < 2 ? 1 : 0)}}}) {
                    }
                }
            }
        }
    }
}

// Clay has no grid layout, so like a matrix the table is laid out as columns
// of cells, each cell at least as tall as the tallest cell in its row last
// frame. Cells grow to their column's width, so grid lines line up.
void TableNode::Render(const RenderContext& context) {
    if (compact_) {
        RenderCompact(context);
        return;
    }
    const float fontSize = context.fontSize;
    const std::size_t rows = RowCount();
    const std::size_t columns = ColumnCount();
    std::vector<float> rowHeights(rows, 0.0f);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            if (const auto bounds = LastLayoutBounds(CellId(row, column))) {
                rowHeights[row] = std::max(rowHeights[row], bounds->height);
            }
        }
    }

    const bool outlined = context.activeStructure == this;
    const Clay_Color lineColor = outlined ? node_style::SLOT_OUTLINE_COLOR : GRID_COLOR;
    const DeleteHighlight& marked = context.deleteHighlight;
    auto markedForDelete = [&](std::size_t row, std::size_t column) {
        if (marked.node != this) {
            return false;
        }
        switch (marked.part) {
        case DeleteHighlight::Part::ROW:
            return row == marked.index;
        case DeleteHighlight::Part::COLUMN:
            return column == marked.index;
        case DeleteHighlight::Part::WHOLE:
            return true;
        }
        return false;
    };

    const auto cellPadX = static_cast<uint16_t>(std::round(fontSize * 0.4f));
    const auto cellPadY = static_cast<uint16_t>(std::round(fontSize * 0.2f));
    const float minCellWidth = std::round(fontSize * 2.5f);
    const float buttonSize = std::round(fontSize * 1.1f);

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(8),
                     .childGap = 6,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = node_style::CARD_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = node_style::CARD_BORDER_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        // Title, and the Settings button on the right.
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 8,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
            node_style::Slot(outlined, [&] { title_->Render(context); });
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
            node_style::SettingsButton(GetId(), fontSize);
        }
        if (!header_) {
            node_style::ActionButton(GetId(),
                                     ADD_HEADER,
                                     "+ header",
                                     fontSize * 0.75f,
                                     CLAY_SIZING_FIT(0),
                                     CLAY_SIZING_FIXED(buttonSize));
        }

        // The grid with "+ column" on its right and "+ row" under it.
        CLAY_AUTO_ID(
            {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .childGap = 2}}) {
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                     .childGap = 2,
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                         .layoutDirection = CLAY_LEFT_TO_RIGHT},
                              .backgroundColor = TABLE_COLOR,
                              .border = {.color = lineColor, .width = CLAY_BORDER_OUTSIDE(1)}}) {
                    for (std::size_t column = 0; column < columns; ++column) {
                        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(minCellWidth),
                                                            CLAY_SIZING_FIT(0)},
                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                            for (std::size_t row = 0; row < rows; ++row) {
                                const bool red = markedForDelete(row, column);
                                const bool header = header_ && row == 0;
                                const auto right =
                                    static_cast<uint16_t>(column + 1 < columns ? 1 : 0);
                                const auto bottom = static_cast<uint16_t>(row + 1 < rows ? 1 : 0);
                                CLAY(
                                    CellId(row, column),
                                    {.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                                           CLAY_SIZING_FIT(rowHeights[row])},
                                                .padding = {cellPadX, cellPadX, cellPadY, cellPadY},
                                                .childAlignment = {.x = CLAY_ALIGN_X_CENTER,
                                                                   .y = CLAY_ALIGN_Y_CENTER}},
                                     .backgroundColor = red      ? node_style::DELETE_FILL_COLOR
                                                        : header ? HEADER_COLOR
                                                                 : Clay_Color{0, 0, 0, 0},
                                     .border = {.color = red ? node_style::DELETE_COLOR : lineColor,
                                                .width = red ? Clay_BorderWidth{1, 1, 1, 1, 0}
                                                             : Clay_BorderWidth{
                                                                   0, right, 0, bottom, 0}}}) {
                                    if (const auto& cell = cells_[row][column]) {
                                        cell->Render(context);
                                    } else {
                                        node_style::Placeholder(fontSize);
                                    }
                                }
                            }
                        }
                    }
                }
                node_style::ActionButton(GetId(),
                                         ADD_ROW,
                                         "+",
                                         fontSize * 0.85f,
                                         CLAY_SIZING_GROW(0),
                                         CLAY_SIZING_FIXED(buttonSize));
            }
            node_style::ActionButton(GetId(),
                                     ADD_COLUMN,
                                     "+",
                                     fontSize * 0.85f,
                                     CLAY_SIZING_FIXED(buttonSize),
                                     CLAY_SIZING_GROW(0));
        }
    }
}

// --- Copying and saving -------------------------------------------------------------

std::unique_ptr<EditorNode> TableNode::Clone() const {
    std::vector<Row> cells;
    for (const auto& row : cells_) {
        Row copy;
        for (const auto& cell : row) {
            copy.push_back(CloneOf(cell));
        }
        cells.push_back(std::move(copy));
    }
    auto copy = std::make_unique<TableNode>(CloneOf(title_), header_, unknown_, std::move(cells));
    copy->compact_ = compact_;
    copy->CopyIdFrom(*this);
    return copy;
}

std::string TableNode::ToText() const {
    std::vector<std::string> items;
    if (const std::string title = title_->ToText(); !title.empty()) {
        items.push_back(options::Item("title", title));
    }
    if (header_) {
        items.push_back(options::Item("header", "true"));
    }
    for (const auto& [key, value] : unknown_) {
        items.push_back(options::Item(key, value));
    }

    std::string body;
    for (std::size_t row = 0; row < cells_.size(); ++row) {
        if (row > 0) {
            body += "\\\\";
        }
        for (std::size_t column = 0; column < cells_[row].size(); ++column) {
            if (column > 0) {
                body += "&";
            }
            if (cells_[row][column]) {
                body += cells_[row][column]->ToText();
            }
        }
    }
    return options::Environment(ENVIRONMENT, options::Join(items), body);
}
