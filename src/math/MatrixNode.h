#pragma once

#include "EditorNode.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// \begin{matrix} a & b \\ c & d \end{matrix}
// Also used for the pmatrix/bmatrix/Bmatrix/vmatrix/Vmatrix variants.
class MatrixNode : public EditorNode {
public:
    using Row = std::vector<std::unique_ptr<EditorNode>>;

private:
    std::vector<Row> cells_;

    // Clay id of the element around a cell, stable across frames.
    Clay_ElementId CellId(std::size_t row, std::size_t column) const;

    // LaTeX environment name, e.g. "matrix" or "pmatrix".
    std::string environment_ = "matrix";

public:
    // A new, empty 2x2 matrix.
    MatrixNode();

    MatrixNode(std::string environment, std::vector<Row> cells);

    ~MatrixNode() override = default;

    void Render(const RenderContext& context) override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // Arrows move between neighbouring cells; at the matrix's edge the exit is
    // passed up.
    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x, VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path, std::size_t& used) const override;
    bool OutlinesSlots() const override { return true; }
    void Normalize() override;

    std::size_t RowCount() const;

    std::size_t ColumnCount(std::size_t row) const;

    EditorNode* GetCell(std::size_t row, std::size_t column) const;

    const std::string& GetEnvironment() const;

    // True if `name` is a matrix environment this node supports.
    static bool IsMatrixEnvironment(const std::string& name);
};
