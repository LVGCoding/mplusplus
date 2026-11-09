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
  // TODO: fill this from `source` once the Parser is working.
  std::vector<std::unique_ptr<EditorNode>> content;

  // Position where the cursor is located in the node tree.
  std::vector<std::size_t> cursorPos = {0};

  // Starting position of the selected nodes.
  // When selectionStart == cursorPos, nothing is selected.
  std::vector<std::size_t> selectionStart = {0};

  bool Load(const std::filesystem::path &path, std::string &error) override;
  bool Save(const std::filesystem::path &path, std::string &error) override;
};
