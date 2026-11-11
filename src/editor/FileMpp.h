#pragma once

#include "fileBase.h"
#include "math/EditorNode.h"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// An M++ (.mpp) document currently open in the M++ editor.
struct FileMpp : FileBase {
  // Raw LaTeX/M++ source as read from disk. Kept so the file round-trips on
  // save until the node tree can be serialized back to text.
  std::string source;

  // Parsed document. EditorNode is abstract, so nodes are held by pointer.
  // Filled from `source` by the Parser when the file is loaded.
  std::vector<std::unique_ptr<EditorNode>> content;

  // Position where the cursor is located in the node tree.
  std::vector<std::size_t> cursorPos = {0};

  // Starting position of the selected nodes.
  // When selectionStart == cursorPos, nothing is selected.
  std::vector<std::size_t> selectionStart = {0};

  bool Load(const std::filesystem::path &path, std::string &error) override;
  bool Save(const std::filesystem::path &path, std::string &error) override;

  // Rendering and input, in fileMppView.cpp. The node tree is rendered
  // read-only; editing is not written yet.
  bool HandleInput(const EditorView &view) override;
  void LayoutContent() const override;
  std::size_t LineCount() const override;
  void DrawOverlay(const EditorView &view) const override;
  std::optional<Rectangle> CursorBounds(const EditorView &view) const override;
};
