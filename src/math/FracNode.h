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

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // {numerator, denominator}; either may be null.
    std::pair<EditorNode*, EditorNode*> GetChildren() const;

    void SetChildren(std::unique_ptr<EditorNode> numerator,
                     std::unique_ptr<EditorNode> denominator);
};
