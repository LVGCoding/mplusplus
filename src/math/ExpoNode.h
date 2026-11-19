#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>
#include <utility>

// \exp or ctrl+^
class ExpoNode : public EditorNode {
private:
    std::unique_ptr<EditorNode> argument;

public:

    ExpoNode() = default;

    ExpoNode(std::unique_ptr<EditorNode> argument);

    ~ExpoNode() override = default;
    
    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float,float>GetNWCorner() override;

    std::pair<float,float>GetSize() override;

    bool HasChild() override;

    // Left from the start of the argument goes to the index (if any) and Right
    // from the end of the index goes to the argument; other exits are passed up.
    // Backspace at the start of the first slot asks for the root to be deleted.
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


    std::pair<EditorNode*,
              EditorNode*>
    GetChildren() const;

    EditorNode* GetArgument() const {
        return argument.get();
    }

    void SetChildren(std::unique_ptr<EditorNode> root,
                     std::unique_ptr<EditorNode> argument);
    
};