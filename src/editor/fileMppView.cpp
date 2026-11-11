// Rendering and input for M++ documents (FileMpp).
// Loading and saving live in fileMpp.cpp.
//
// TODO: editing is not implemented yet:
//   - HandleInput():   route keys/clicks to the node under the cursor
//   (KeyPress/MouseClick)
//   - DrawOverlay():   draw the cursor/selection using cursorPos/selectionStart
//   - CursorBounds():  return the cursor rectangle so the editor can scroll to
//   it

#include "FileMpp.h"
#include "clay.h"
#include "math/NodeStyle.h"
#include "math/TextNode.h"
#include "ui/EditorView.h"

#include <algorithm>
#include <string_view>
#include <vector>

namespace {
// One item on a document row: a node, or a slice of a top-level TextNode.
struct RowItem {
  EditorNode *node = nullptr;
  std::string_view text;
};

// Split the top-level nodes into rows at the newlines in their text, so the
// document keeps the source's line structure.
std::vector<std::vector<RowItem>>
SplitIntoRows(const std::vector<std::unique_ptr<EditorNode>> &content) {
  std::vector<std::vector<RowItem>> rows(1);
  for (const auto &node : content) {
    const auto *text = dynamic_cast<const TextNode *>(node.get());
    if (!text) {
      rows.back().push_back({node.get(), {}});
      continue;
    }
    std::string_view rest = text->GetText();
    while (true) {
      const std::size_t newline = rest.find('\n');
      std::string_view line = rest.substr(0, newline);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      if (!line.empty()) {
        rows.back().push_back({nullptr, line});
      }
      if (newline == std::string_view::npos) {
        break;
      }
      rows.emplace_back();
      rest.remove_prefix(newline + 1);
    }
  }
  return rows;
}
} // namespace

void FileMpp::LayoutContent() const {
  const float fontSize = editor_style::fontSize;
  const auto rows = SplitIntoRows(content);
  CLAY(CLAY_ID("MppDocument"),
       {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
      // Rows are at least one text line tall; rows holding fractions or
      // matrices grow to fit them.
      CLAY(CLAY_IDI("MppRow", static_cast<uint32_t>(i)),
           {.layout = {.sizing = {CLAY_SIZING_FIT(0),
                                  CLAY_SIZING_FIT(editor_style::lineHeight)},
                       .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                       .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        for (const RowItem &item : rows[i]) {
          if (item.node) {
            item.node->Render(fontSize);
          } else {
            node_style::Text(item.text, fontSize);
          }
        }
      }
    }
  }
}

bool FileMpp::HandleInput(const EditorView & /*view*/) { return false; }

std::size_t FileMpp::LineCount() const {
  return static_cast<std::size_t>(
             std::count(source.begin(), source.end(), '\n')) +
         1;
}

void FileMpp::DrawOverlay(const EditorView & /*view*/) const {}

std::optional<Rectangle>
FileMpp::CursorBounds(const EditorView & /*view*/) const {
  return std::nullopt;
}
