#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>
#include <utility>

// \sqrt{argument} or \sqrt[root]{argument}
class SqrtNode : public EditorNode {
private:
    // Null for a plain square root.
    std::unique_ptr<EditorNode> root_;
    std::unique_ptr<EditorNode> argument_;

public:
    SqrtNode() = default;

    SqrtNode(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument);

    ~SqrtNode() override = default;

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // {root, argument}; root is null for a plain square root.
    std::pair<EditorNode*, EditorNode*> GetChildren() const;

    void SetChildren(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument);
};
