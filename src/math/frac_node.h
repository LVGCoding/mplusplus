#pragma once

#include "editor_node.h"
#include "line_node.h"

#include <memory>
#include <vector>
#include <string>

class FracNode : EditorNode{
private:
    std::unique_ptr<LineNode> numerator;
    std::unique_ptr<LineNode> denominator;

public:
    void Render(float fontSize) override;
    
    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;
};