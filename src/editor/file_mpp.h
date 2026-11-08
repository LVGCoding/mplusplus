#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <editor_node.h>
#include "fileBase.h"
#include <parser.h>
// The file class represents a file currently open in the M++ editor.
struct FileMpp: FileBase {
    std::vector<EditorNode> content;
    std::filesystem::path path;
    bool modified = false;

    // Position where the cursor is located in the text.
    std::vector<size_t> cursorPos = {0};

    // Starting position of the selected text.
    // When selectionStart == cursorPos, nothing is selected.
    std::vector<size_t> selectionStart = {0};

    const std::string& TabName() const;

    const Parser parser();

private:
    // Keep the label alive until Clay finishes rendering the frame.
    mutable std::string tabName_;
};

// bool LoadFile(File& file,
//               const std::filesystem::path& path,
//               std::string& error);

// bool SaveFile(File& file,
//               const std::filesystem::path& path,
//               std::string& error);