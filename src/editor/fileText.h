#pragma once

#include "fileBase.h"

#include <cstddef>
#include <filesystem>
#include <string>

// A plain text file currently open in the M++ editor.
// Loading/saving lives in fileText.cpp; rendering and input in fileTextView.cpp.
struct FileText : FileBase {
    std::string text;

    // Position where the cursor is located in the text.
    std::size_t cursorPos = 0;

    // Starting position of the selected text.
    // When selectionStart == cursorPos, nothing is selected.
    std::size_t selectionStart = 0;

    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;

    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override;
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& view) const override;

private:
    // True while a mouse drag that started in this file's editor is selecting text.
    bool dragging_ = false;

    // Apply this frame's typed characters and editing keys. Returns true if the text changed.
    bool HandleKeyboard();
    // Click/drag to place the cursor and select. Returns true if the cursor or selection moved.
    bool HandleMouse(const EditorView& view);
    void DrawSelection(const EditorView& view) const;
    void DrawCursor(const EditorView& view) const;
};

// Shared helpers for reading/writing a whole file as raw bytes.
bool ReadWholeFile(const std::filesystem::path& path, std::string& text, std::string& error);
bool WriteWholeFile(const std::filesystem::path& path, const std::string& text, std::string& error);
