#pragma once

#include "TextBuffer.h"
#include "fileBase.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

// A plain text file currently open in the M++ editor.
// Loading/saving lives in fileText.cpp; rendering and mouse input in
// fileTextView.cpp. All the editing (text, cursor, selection, keys, undo,
// clipboard) comes from TextBuffer, which the UI's text boxes use too.
struct FileText : FileBase, TextBuffer {
    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;

    // FileBase's undo interface (used by the Edit menu), done by TextBuffer.
    bool Undo() override { return TextBuffer::Undo(); }
    bool Redo() override { return TextBuffer::Redo(); }
    bool CanUndo() const override { return TextBuffer::CanUndo(); }
    bool CanRedo() const override { return TextBuffer::CanRedo(); }

    // FileBase's clipboard interface (used by the Edit menu), done by TextBuffer.
    std::string SelectedText() const override { return TextBuffer::SelectedText(); }
    bool PasteText(std::string text) override { return TextBuffer::PasteText(std::move(text)); }
    bool DeleteSelection() override { return TextBuffer::DeleteSelection(); }
    // Both bases have one; they agree, TextBuffer's doesn't copy the text.
    bool HasSelection() const { return TextBuffer::HasSelection(); }

    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override;
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& view) const override;

protected:
    // Keeps `modified` in step with the text, so undoing back to what was
    // saved clears it.
    void TextChanged() override;

private:
    // True while a mouse drag that started in this file's editor is selecting text.
    bool dragging_ = false;

    // The text last loaded or saved; nullopt for a file never saved.
    std::optional<std::string> savedText_;

    // Click/drag to place the cursor and select. Returns true if the cursor or selection moved.
    bool HandleMouse(const EditorView& view);
    void DrawSelection(const EditorView& view) const;
    void DrawCursor(const EditorView& view) const;
};

// Shared helpers for reading/writing a whole file as raw bytes.
bool ReadWholeFile(const std::filesystem::path& path, std::string& text, std::string& error);
bool WriteWholeFile(const std::filesystem::path& path, const std::string& text, std::string& error);
