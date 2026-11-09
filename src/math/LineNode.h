#pragma once

#include "EditorNode.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// An ordered list of nodes. Used for the contents of command arguments
// (\frac{...}{...}, \sqrt{...}), matrix cells, and bare {...} groups.
class LineNode : public EditorNode {
private:
    std::vector<std::unique_ptr<EditorNode>> children_;

    // True for a bare {...} group in the source, so ToText() restores the braces.
    bool braced_ = false;

    float spacing_ = 0.0f;

public:
    LineNode() = default;

    explicit LineNode(std::vector<std::unique_ptr<EditorNode>> children, bool braced = false);

    void AddChild(std::unique_ptr<EditorNode> child);

    void InsertChild(std::size_t index, std::unique_ptr<EditorNode> child);

    void RemoveChild(std::size_t index);

    EditorNode* GetChild(std::size_t index) const;

    std::size_t ChildCount() const;

    bool IsBraced() const;

    void Render(float fontSize) override;

    void KeyPress() override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;
};
