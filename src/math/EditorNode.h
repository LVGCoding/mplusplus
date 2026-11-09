#pragma once

#include <string>
#include <utility>

class EditorNode {
public:
    virtual ~EditorNode() = default;

    virtual void Render(float fontSize) = 0;
    virtual void KeyPress() = 0;
    virtual void MouseClick() = 0;

    // Serialize this node (and its children) back to M++/LaTeX source.
    virtual std::string ToText() const = 0;

    virtual std::pair<float, float> GetNWCorner() = 0;
    virtual std::pair<float, float> GetSize() = 0;
    virtual bool HasChild() = 0;
};
