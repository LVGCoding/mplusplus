#pragma once

#include "EditorNode.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

// A big operator with limits, applied to what follows it:
//   \sum_{k=1}^{n}     ∑  sum            \prod_{k=1}^{n}  ∏  product
//   \bigcup_{i ∈ S}^{} ⋃  union          \bigcap_{…}^{…}  ⋂  intersection
// The cursor path is [0 lower | 1 upper, ...]; entering lands in the lower
// limit, where the variable is written.
class BigOpNode : public EditorNode {
public:
    enum class Kind { SUM, PRODUCT, UNION, INTERSECTION };

private:
    Kind kind_ = Kind::SUM;
    std::unique_ptr<EditorNode> lower_;
    std::unique_ptr<EditorNode> upper_;

public:
    BigOpNode(Kind kind,
              std::unique_ptr<EditorNode> lower,
              std::unique_ptr<EditorNode> upper);

    ~BigOpNode() override = default;

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

    // Up/Down move between the limits; other exits are passed up. Backspace
    // at the start of the lower limit asks for the node to be deleted.
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
    EditorNode* GetLower() const {
        return lower_.get();
    }
    EditorNode* GetUpper() const {
        return upper_.get();
    }

    // "sum", "prod", "bigcup", "bigcap", and back; and the drawn glyph.
    static std::string_view CommandName(Kind kind);
    static std::optional<Kind> KindFromCommand(std::string_view name);
    static std::string_view Glyph(Kind kind);
};
