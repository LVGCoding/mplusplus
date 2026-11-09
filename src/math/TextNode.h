#pragma once

#include "EditorNode.h"


#include <memory>
#include <vector>
#include <string>

class TextNode : EditorNode{
private:
    std::string text;

public:
    TextNode() {};

    ~TextNode() {};

    void Render(float fontSize) override;

    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;

    std::string GetText();
};