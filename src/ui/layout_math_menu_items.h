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
// One entry of the math popup. `make` builds the node it inserts; the popup
// also renders one copy of it as the item's preview (see MathMenuPreview), so
// the preview always matches what ends up in the document. Empty slots
// (LineNodes with nothing in them) show as grey placeholder boxes.
struct MathMenuItem {
  std::string_view label;
  std::function<std::unique_ptr<EditorNode>()> make;
};

// Insert `node` at the cursor of the active M++ file (no-op for other files).
inline void InsertIntoActiveFile(Editor &editor, std::unique_ptr<EditorNode> node) {
  auto *file = dynamic_cast<FileMpp *>(editor.ActiveFile());
  if (file && file->InsertNode(std::move(node))) {
    ResetEditorCursorBlink();
  }
}

inline std::unique_ptr<EditorNode> MakeMathFrac() {
  return std::make_unique<FracNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>());
}

inline std::unique_ptr<EditorNode> MakeMathSqrt() {
  return std::make_unique<SqrtNode>(nullptr, std::make_unique<LineNode>());
}

inline std::unique_ptr<EditorNode> MakeMathRoot() {
  return std::make_unique<SqrtNode>(std::make_unique<LineNode>(), std::make_unique<LineNode>());
}

// A 2x2 matrix in the given environment (matrix, pmatrix, bmatrix, ...).
inline std::function<std::unique_ptr<EditorNode>()> MakeMathMatrix(const char *environment) {
  return [environment]() -> std::unique_ptr<EditorNode> {
    std::vector<MatrixNode::Row> rows;
    for (int row = 0; row < 2; ++row) {
      MatrixNode::Row cells;
      cells.push_back(std::make_unique<LineNode>());
      cells.push_back(std::make_unique<LineNode>());
      rows.push_back(std::move(cells));
    }
    return std::make_unique<MatrixNode>(environment, std::move(rows));
  };
}

inline std::function<std::unique_ptr<EditorNode>()>
MakeMathGreek(GreekNode::GreekSymbolType symbol) {
  return [symbol]() -> std::unique_ptr<EditorNode> { return std::make_unique<GreekNode>(symbol); };
}

inline const std::vector<MathMenuItem> &MathMenuItems() {
  static const std::vector<MathMenuItem> items = [] {
    std::vector<MathMenuItem> list = {
        {"\\frac", MakeMathFrac},
        {"\\sqrt", MakeMathSqrt},
        {"\\sqrt[n]", MakeMathRoot},
        {"\\matrix", MakeMathMatrix("matrix")},
        {"\\pmatrix", MakeMathMatrix("pmatrix")},
        {"\\bmatrix", MakeMathMatrix("bmatrix")},
        {"\\vmatrix", MakeMathMatrix("vmatrix")},
    };
    // Greek letters, named as in LaTeX (\alpha ... \omega).
    static std::vector<std::string> greekLabels;
    for (int i = GreekNode::alpha; i <= GreekNode::omega; ++i) {
      const auto symbol = static_cast<GreekNode::GreekSymbolType>(i);
      greekLabels.push_back("\\" + std::string(GreekNode::SymbolName(symbol)));
    }
    for (int i = GreekNode::alpha; i <= GreekNode::omega; ++i) {
      list.push_back({greekLabels[static_cast<std::size_t>(i)],
                      MakeMathGreek(static_cast<GreekNode::GreekSymbolType>(i))});
    }
    return list;
  }();
  return items;
}

// The preview node for MathMenuItems()[index], built once and kept for the
// program's lifetime so its Clay ids stay stable between frames (matrices
// size their rows from last frame's layout).
inline EditorNode *MathMenuPreview(std::size_t index) {
  static std::vector<std::unique_ptr<EditorNode>> previews = [] {
    std::vector<std::unique_ptr<EditorNode>> nodes;
    for (const MathMenuItem &item : MathMenuItems()) {
      nodes.push_back(item.make ? item.make() : nullptr);
    }
    return nodes;
  }();
  return index < previews.size() ? previews[index].get() : nullptr;
}

// Insert a fresh copy of MathMenuItems()[index] into the active file.
inline void SelectMathMenuItem(Editor &editor, std::size_t index) {
  const auto &items = MathMenuItems();
  if (index < items.size() && items[index].make) {
    InsertIntoActiveFile(editor, items[index].make());
  }
}
} // namespace layout_internal
