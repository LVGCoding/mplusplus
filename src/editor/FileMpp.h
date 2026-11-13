#pragma once

#include "fileBase.h"
#include "math/EditorNode.h"
#include "math/LineNode.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TextNode;

// An M++ (.mpp) document currently open in the M++ editor.
struct FileMpp : FileBase {
    // Raw LaTeX/M++ source as read from disk. Kept so the file round-trips on
    // save until the node tree can be serialized back to text.
    std::string source;

    // Parsed document: one LineNode per source line, each normalized (see
    // LineNode::Normalize). Built from `source` by SetSource().
    std::vector<std::unique_ptr<LineNode>> content;

    // Cursor position: [row, ...path inside that row's LineNode]. Always ends
    // at an offset in a TextNode (see CursorPath).
    CursorPath cursorPos = {0, 0, 0};

    // Starting position of the selected nodes.
    // When selectionStart == cursorPos, nothing is selected.
    CursorPath selectionStart = {0, 0, 0};

    FileMpp();

    bool Load(const std::filesystem::path& path, std::string& error) override;
    bool Save(const std::filesystem::path& path, std::string& error) override;

    // Replace the document with parsed `text` and put the cursor at the start.
    void SetSource(std::string text);

    // The document serialized back to M++ source.
    std::string ContentText() const;

    // Move the cursor for an arrow key (raylib KEY_*). `measure` may be null
    // (e.g. in tests); then Up/Down land at the start of the target.
    // Returns true if the cursor moved.
    bool MoveCursor(int key, bool shift, const TextMeasure* measure);

    // The node that `path` ends in and the offset into it, or null if the path
    // doesn't lead to a TextNode.
    TextNode* CursorText(const CursorPath& path, std::size_t* offset = nullptr) const;

    // The innermost fraction/root/matrix holding the cursor (outlined in green).
    const EditorNode* ActiveStructure() const;

    // Rendering and input, in fileMppView.cpp. Only cursor navigation is
    // implemented; typing and selection are not written yet.
    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override;
    uint32_t LineElementId(std::size_t line) const override;
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& view) const override;

private:
    // Line ending used to join rows back into source ("\n" or "\r\n").
    std::string lineEnding_ = "\n";

    // Screen x kept across consecutive Up/Down presses so the cursor returns to
    // the same column after passing through shorter rows.
    // Only valid while the cursor is still at goalPath_, where the last
    // vertical move left it.
    std::optional<float> goalX_;
    CursorPath goalPath_;

    std::optional<Clay_BoundingBox> CaretBox(const TextMeasure* measure) const;
};
