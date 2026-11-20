#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>
#include <utility>

class SumNode : public EditorNode {
private:
    std::unique_ptr<EditorNode> summand;
    std::unique_ptr<EditorNode> index;
    std::unique_ptr<EditorNode> upperLimit;

public:
    SumNode() = default;

    SumNode(std::unique_ptr<EditorNode> summand,
            std::unique_ptr<EditorNode> index,
            std::unique_ptr<EditorNode> upperLimit);

    ~SumNode() override = default;

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float,float>GetNWCorner() override;

    std::pair<float,float>GetSize() override;

    bool HasChild() override;

    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x,
                             VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path,
                          std::size_t& used) const override;
    std::vector<ChildPath> ChildPaths() const override;
    bool OutlinesSlots() const override {
        return true;
    }
    void Normalize() override;

    std::tuple<EditorNode*, EditorNode*, EditorNode*> GetChildren() const;

    EditorNode* GetSummand() const { return summand.get(); }
    EditorNode* GetIndex() const { return index.get(); }
    EditorNode* GetUpperLimit() const { return upperLimit.get(); }

    void SetChildren(std::unique_ptr<EditorNode> summand,
                     std::unique_ptr<EditorNode> index,
                     std::unique_ptr<EditorNode> upperLimit);
};