#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include "layout_common.h"
#include "layout_state.h"
#include "math/FracNode.h"
#include "math/GreekNode.h"
#include "math/LineNode.h"
#include "math/MatrixNode.h"
#include "math/SqrtNode.h"

namespace layout_internal {
struct MathMenuItem {
  std::string_view label;
  std::function<void(Editor &)> onSelect;
};

// Insert `node` at the cursor of the active M++ file (no-op for other files).
inline void InsertIntoActiveFile(Editor &editor, std::unique_ptr<EditorNode> node) {
  auto *file = dynamic_cast<FileMpp *>(editor.ActiveFile());
  if (file && file->InsertNode(std::move(node))) {
    ResetEditorCursorBlink();
  }
}

inline void OnMathFrac(Editor &editor) {
  InsertIntoActiveFile(editor, std::make_unique<FracNode>(std::make_unique<LineNode>(),
                                                          std::make_unique<LineNode>()));
}

inline void OnMathSqrt(Editor &editor) {
  InsertIntoActiveFile(editor, std::make_unique<SqrtNode>(nullptr, std::make_unique<LineNode>()));
}

inline void OnMathRoot(Editor &editor) {
  InsertIntoActiveFile(editor, std::make_unique<SqrtNode>(std::make_unique<LineNode>(),
                                                          std::make_unique<LineNode>()));
}

// A 2x2 matrix in the given environment (matrix, pmatrix, bmatrix, ...).
inline std::function<void(Editor &)> OnMathMatrix(const char *environment) {
  return [environment](Editor &editor) {
    std::vector<MatrixNode::Row> rows;
    for (int row = 0; row < 2; ++row) {
      MatrixNode::Row cells;
      cells.push_back(std::make_unique<LineNode>());
      cells.push_back(std::make_unique<LineNode>());
      rows.push_back(std::move(cells));
    }
    InsertIntoActiveFile(editor, std::make_unique<MatrixNode>(environment, std::move(rows)));
  };
}

inline std::function<void(Editor &)> OnMathGreek(GreekNode::GreekSymbolType symbol) {
  return [symbol](Editor &editor) {
    InsertIntoActiveFile(editor, std::make_unique<GreekNode>(symbol));
  };
}

inline const std::vector<MathMenuItem> &MathMenuItems() {
  static const std::vector<MathMenuItem> items = [] {
    std::vector<MathMenuItem> list = {
        {"\\frac", OnMathFrac},
        {"\\sqrt", OnMathSqrt},
        {"\\sqrt[n]", OnMathRoot},
        {"\\matrix", OnMathMatrix("matrix")},
        {"\\pmatrix", OnMathMatrix("pmatrix")},
        {"\\bmatrix", OnMathMatrix("bmatrix")},
        {"\\vmatrix", OnMathMatrix("vmatrix")},
    };
    // Greek letters, named as in LaTeX (\alpha ... \omega).
    static std::vector<std::string> greekLabels;
    for (int i = GreekNode::alpha; i <= GreekNode::omega; ++i) {
      const auto symbol = static_cast<GreekNode::GreekSymbolType>(i);
      greekLabels.push_back("\\" + std::string(GreekNode::SymbolName(symbol)));
    }
    for (int i = GreekNode::alpha; i <= GreekNode::omega; ++i) {
      list.push_back({greekLabels[static_cast<std::size_t>(i)],
                      OnMathGreek(static_cast<GreekNode::GreekSymbolType>(i))});
    }
    return list;
  }();
  return items;
}
} // namespace layout_internal
