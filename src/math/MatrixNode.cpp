#include "MatrixNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <algorithm>
#include <array>
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

// Clay has no grid layout, so the matrix is laid out as columns of centred
// cells. To keep rows lined up across columns, each cell is given at least the
// height of the tallest cell in its row, measured from the previous frame
// (cell ids are stable, so last frame's layout can be looked up).
// TODO: input is not implemented yet.
void MatrixNode::Render(float fontSize) {
    std::size_t columns = 0;
    for (const auto& row : cells_) {
        columns = std::max(columns, row.size());
    }

    auto cellId = [&](std::size_t row, std::size_t column) {
        return Clay__HashStringWithOffset(CLAY_STRING("MatrixCell"),
                                          static_cast<uint32_t>(row * columns + column), GetId());
    };

    std::vector<float> rowHeights(cells_.size(), 0.0f);
    for (std::size_t row = 0; row < cells_.size(); ++row) {
        for (std::size_t column = 0; column < cells_[row].size(); ++column) {
            const Clay_ElementData data = Clay_GetElementData(cellId(row, column));
            if (data.found) {
                rowHeights[row] = std::max(rowHeights[row], data.boundingBox.height);
            }
        }
    }

    const Delimiters delimiters = DelimitersFor(environment_);
    const auto columnGap = static_cast<uint16_t>(std::round(fontSize));
    const auto rowGap = static_cast<uint16_t>(std::round(fontSize * 0.3f));
    const auto pad = static_cast<uint16_t>(std::round(fontSize * 0.15f));

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
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
                                CLAY(cellId(row, column),
                                     {.layout = {.sizing = {CLAY_SIZING_FIT(0),
                                                            CLAY_SIZING_FIT(0)}}}) {
                                    if (cells_[row][column]) {
                                        cells_[row][column]->Render(fontSize);
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
void MatrixNode::KeyPress() {}
void MatrixNode::MouseClick() {}
std::pair<float, float> MatrixNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> MatrixNode::GetSize() {
    return {0.0f, 0.0f};
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
