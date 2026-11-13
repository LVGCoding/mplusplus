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

    void Render(const RenderContext& context) override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // Left from the start of the argument goes to the index (if any) and Right
    // from the end of the index goes to the argument; other exits are passed up.
    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x, VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path, std::size_t& used) const override;
    bool OutlinesSlots() const override { return true; }
    void Normalize() override;

    // {root, argument}; root is null for a plain square root.
    std::pair<EditorNode*, EditorNode*> GetChildren() const;

    void SetChildren(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument);
};
