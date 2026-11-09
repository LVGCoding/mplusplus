#pragma once

#include "EditorNode.h"

#include <string>

// A run of plain text (letters, digits, operators, whitespace, and any LaTeX
// commands that don't have a dedicated node type yet).
class TextNode : public EditorNode {
private:
    std::string text_;

public:
    explicit TextNode(std::string text = {});

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    const std::string& GetText() const;

    void SetText(std::string text);
};
