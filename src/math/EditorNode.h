#pragma once

#include <utility>

class EditorNode{
public:
    virtual ~EditorNode() = default;

    virtual void Render(float fontSize) = 0;
    virtual void KeyPress() = 0;
    virtual void MouseClick() = 0;
    virtual void ToText() = 0;
    virtual std::pair<float,float> GetNWCorner() = 0;
    virtual std::pair<float,float> GetSize() = 0;
    virtual bool HasChild() = 0;
};