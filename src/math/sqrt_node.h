#pragma once

#include "editor_node.h"
#include "line_node.h"

#include <memory>
#include <vector>
#include <string>

class SqrtNode : EditorNode{
private:
    std::unique_ptr<LineNode> root;
    std::unique_ptr<LineNode> arguement;
public:
    void Render(float fontSize) override;
    
    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;
};