#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>
#include <utility>

// \frac{numerator}{denominator}
class FracNode : public EditorNode {
private:
    std::unique_ptr<EditorNode> numerator_;
    std::unique_ptr<EditorNode> denominator_;

public:
    FracNode() = default;

    FracNode(std::unique_ptr<EditorNode> numerator, std::unique_ptr<EditorNode> denominator);

    ~FracNode() override = default;

    void Render(const RenderContext& context) override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // Up from the denominator goes to the numerator and Down goes back; all
    // other exits are passed up, so Left/Right leave the fraction. Backspace at
    // the start of the numerator asks for the fraction to be deleted.
    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x, VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path, std::size_t& used) const override;
    bool OutlinesSlots() const override { return true; }
    void Normalize() override;

    // {numerator, denominator}; either may be null.
    std::pair<EditorNode*, EditorNode*> GetChildren() const;

    void SetChildren(std::unique_ptr<EditorNode> numerator,
                     std::unique_ptr<EditorNode> denominator);
};
