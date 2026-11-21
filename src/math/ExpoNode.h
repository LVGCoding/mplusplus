#pragma once

#include "EditorNode.h"

#include <memory>
#include <string>

// An exponent, ^{…}, written after what it raises: x^{2}, e^{-x}, ℝ^{n}.
// Like LaTeX, it holds only the exponent; the base is whatever comes before
// it in the line. Typing ^ inserts one. The cursor path is [0, ...] into it.
// Files from before this saved \exp{base}{exponent}; the parser still reads
// those, as the base followed by ^{exponent}.
class ExpoNode : public EditorNode {
private:
    std::unique_ptr<EditorNode> exponent_;

public:
    explicit ExpoNode(std::unique_ptr<EditorNode> exponent);

    ~ExpoNode() override = default;

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    // Exits are passed up; Backspace at the start asks for the exponent to be
    // deleted.
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

    EditorNode* GetExponent() const {
        return exponent_.get();
    }
};
