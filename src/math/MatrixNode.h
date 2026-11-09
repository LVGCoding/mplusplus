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

    // LaTeX environment name, e.g. "matrix" or "pmatrix".
    std::string environment_ = "matrix";

public:
    // A new, empty 2x2 matrix.
    MatrixNode();

    MatrixNode(std::string environment, std::vector<Row> cells);

    ~MatrixNode() override = default;

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    std::size_t RowCount() const;

    std::size_t ColumnCount(std::size_t row) const;

    EditorNode* GetCell(std::size_t row, std::size_t column) const;

    const std::string& GetEnvironment() const;

    // True if `name` is a matrix environment this node supports.
    static bool IsMatrixEnvironment(const std::string& name);
};
