#pragma once

#include "EditorNode.h"

#include "TextNode.h"

#include <memory>
#include <vector>
#include <string>

class MatrixNode : EditorNode{
private:
    std::vector<std::vector<EditorNode*>> dims;

public:
    MatrixNode() {
        std::vector<EditorNode*> row1;
        std::vector<EditorNode*> row2;
        
        EditorNode* entry1;
        EditorNode* entry2;
        row1.push_back(entry1);
        row1.push_back(entry2);

        EditorNode* entry3;
        EditorNode* entry4;
        row2.push_back(entry3);
        row2.push_back(entry4);

        dims.push_back(row1);
        dims.push_back(row2);
    };

    ~MatrixNode() {};

    void Render(float fontSize) override;

    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;


};