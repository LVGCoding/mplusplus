#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>

// A subscript, _{…}, written after what it belongs to: x_{i}, v_{max},
// A_{i,j}. Typing _ inserts one. The cursor path is [0, ...] into it.
class SubNode : public EditorNode {
private:
    std::unique_ptr<EditorNode> index_;

public:
    explicit SubNode(std::unique_ptr<EditorNode> index);

    ~SubNode() override = default;

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float,
              float>
    GetNWCorner() override;

    std::pair<float,
              float>
    GetSize() override;

    bool HasChild() override;

    // Exits are passed up; Backspace at the start asks for the subscript to
    // be deleted.
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

    EditorNode* GetIndex() const {
        return index_.get();
    }
};
