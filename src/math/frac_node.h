#pragma once

#include "editor_node.h"


#include <memory>
#include <vector>
#include <string>

class FracNode : EditorNode{
private:
    EditorNode* numerator;
    EditorNode* denominator;

public:
    FracNode() {};

    ~FracNode() {};

    void Render(float fontSize) override;

    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;

    std::pair<EditorNode*,EditorNode*> GetChildren();

    void SetChildren(EditorNode& numerator, EditorNode& denominator);
};