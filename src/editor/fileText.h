#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "fileBase.h"

// A plain text file currently open in the M++ editor.
struct FileText : FileBase {
    std::string text;

    // Position where the cursor is located in the text.
    std::size_t cursorPos = 0;

    // Starting position of the selected text.
    // When selectionStart == cursorPos, nothing is selected.
    std::size_t selectionStart = 0;

    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;
};

// Shared helpers for reading/writing a whole file as raw bytes.
bool ReadWholeFile(const std::filesystem::path& path, std::string& text, std::string& error);
bool WriteWholeFile(const std::filesystem::path& path, const std::string& text, std::string& error);
