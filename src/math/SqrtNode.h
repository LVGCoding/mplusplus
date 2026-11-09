#pragma once

#include "EditorNode.h"


#include <memory>
#include <vector>
#include <string>

class SqrtNode : EditorNode{
private:
    EditorNode* root;
    EditorNode* arguement;
public:
    SqrtNode() {};

    ~SqrtNode() {};

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