#pragma once

#include "EditorNode.h"

#include <cstddef>
#include <optional>
#include <string>

// A run of plain text (letters, digits, operators, whitespace, and any LaTeX
// commands that don't have a dedicated node type yet). The cursor always ends
// in a TextNode, at a byte offset on a UTF-8 character boundary.
class TextNode : public EditorNode {
private:
    std::string text_;

    // Font size from the last Render(), used to measure where the cursor goes.
    float fontSize_ = 18.0f;

public:
    explicit TextNode(std::string text = {});

    void Render(const RenderContext& context) override;

    // Typing inserts at the cursor and Backspace/Delete remove one character.
    // Left/Right move one character, leaving at either end; Up/Down always leave.
    // At either end, Backspace/Delete report DeleteLeft/DeleteRight.
    KeyResult KeyPress(const KeyEvent& event) override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x, VerticalEntry entry,
                             const TextMeasure* measure) const override;

    // Screen box of the cursor at byte `offset`, from the last frame's layout:
    // a zero-width box as tall as the text.
    std::optional<Clay_BoundingBox> CaretBox(std::size_t offset,
                                             const TextMeasure* measure) const;

    const std::string& GetText() const;

    void SetText(std::string text);

    void AppendText(const std::string& text);

    // Byte offsets of the UTF-8 characters before and after `offset`.
    std::size_t PreviousCharacter(std::size_t offset) const;
    std::size_t NextCharacter(std::size_t offset) const;
};
