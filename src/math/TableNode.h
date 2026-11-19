#pragma once

#include "MatrixNode.h"
#include "Options.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// A table of editable cells, with an editable title above it:
//   \begin{table}[title=Squares, header=true] x & x^2 \\ 1 & 1 \\ 2 & 4 \end{table}
// It is a matrix underneath, so cells edit exactly like matrix cells: arrows
// move between cells, Enter opens the insert wheel for a new row or column,
// and Backspace at the start of a cell marks its row/column for deletion.
// "+" buttons add a row, a column, or a header row. The Settings button opens
// the table settings popup (a placeholder for now).
//
// The header row (optional, light grey) is the first row. It is meant to
// hold the autofill setup, a variable or an equation per column (x, x^2, ...),
// from which the rows below will be filled once equations can be evaluated.
// New rows never go above it; deleting it makes the table headerless.
//
// Cursor path: [row, column, ...] in a cell, as in a matrix, or
// [TITLE_ROW, 0, ...] in the title. Tables are always rectangular: short rows
// are padded with empty cells.
class TableNode : public MatrixNode {
public:
    static constexpr const char* ENVIRONMENT = "table";
    // The title's row index in cursor paths: far past any real row.
    static constexpr std::size_t TITLE_ROW = std::size_t{1} << 20;

    // What the table's "+" buttons do (see RunAction).
    enum Action : int { ADD_ROW = 1, ADD_COLUMN = 2, ADD_HEADER = 3 };

    // An empty 3x3 table with no title and no header row.
    TableNode();

    // A null title is empty. `unknown` holds options this version doesn't
    // understand, written back unchanged.
    TableNode(std::unique_ptr<EditorNode> title,
              bool header,
              std::vector<options::Option> unknown,
              std::vector<Row> cells);

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;
    std::string ToText() const override;
    void Normalize() override;

    // Cells behave as in a matrix; Up from the top row (or Left from the first
    // cell) goes to the title, Down or Right from the title into the cells.
    // Backspace at the start of the title asks for the table to be deleted.
    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath PositionNearX(float x,
                             VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path,
                          std::size_t& used) const override;
    std::vector<ChildPath> ChildPaths() const override;
    bool RunAction(int action) override;

    // Keep the header row first: a row inserted at 0 goes below it.
    std::size_t InsertRow(std::size_t row) override;
    // Removing the header row makes the table headerless.
    bool RemoveRow(std::size_t row) override;

    EditorNode* Title() const {
        return title_.get();
    }
    bool HasHeader() const {
        return header_;
    }
    std::size_t ColumnCount() const {
        return MaxColumnCount();
    }

    // Draw as a small icon instead of the full table (the math menu preview).
    void SetCompact(bool compact) {
        compact_ = compact;
    }

private:
    std::unique_ptr<EditorNode> title_;
    bool header_ = false;
    std::vector<options::Option> unknown_;
    bool compact_ = false;

    // Pad short rows with empty cells; make sure there is at least one cell.
    void MakeRectangular();
    void RenderCompact(const RenderContext& context);
};
