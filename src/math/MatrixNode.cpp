#include "MatrixNode.h"

#include "LineNode.h"

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

// TODO: rendering and input are not implemented yet.
void MatrixNode::Render(float /*fontSize*/) {}
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
