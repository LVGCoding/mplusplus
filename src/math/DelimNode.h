#pragma once

#include "EditorNode.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

// Content between a pair of delimiters that stretch to its height:
//   \abs{x}    |x|   absolute value
//   \floor{x}  ⌊x⌋   rounded down
//   \ceil{x}   ⌈x⌉   rounded up
// The cursor path is [0, ...] into the argument.
class DelimNode : public EditorNode {
public:
    enum class Kind { ABS, FLOOR, CEIL };

private:
    Kind kind_ = Kind::ABS;
    std::unique_ptr<EditorNode> argument_;

public:
    DelimNode(Kind kind,
              std::unique_ptr<EditorNode> argument);

    ~DelimNode() override = default;

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

    // Exits are passed up; Backspace at the start of the argument asks for the
    // whole node to be deleted.
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

    Kind GetKind() const {
        return kind_;
    }
    EditorNode* GetArgument() const {
        return argument_.get();
    }

    // The LaTeX command for each kind ("abs", "floor", "ceil"), and back.
    static std::string_view CommandName(Kind kind);
    static std::optional<Kind> KindFromCommand(std::string_view name);
};
