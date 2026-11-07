#pragma once

#include "editor_node.h"

#include <memory>
#include <vector>
#include <string>


//not important right now
class LineNode : public EditorNode {
private:
    std::vector<std::unique_ptr<EditorNode>> children_;

    float spacing_ = 0.0f;

public:
    LineNode() = default;

    void AddChild(std::unique_ptr<EditorNode> child);

    void InsertChild(size_t index, std::unique_ptr<EditorNode> child);

    void RemoveChild(size_t index);

    EditorNode* GetChild(size_t index);

    size_t ChildCount() const;

    void Render(float fontSize) override;

    void KeyPress() override;
    
    void MouseClick() override;
    
    void ToText() override;
    
    std::pair<float,float> GetNWCorner() override;
    
    std::pair<float,float> GetSize() override;
    
    bool HasChild() override;

  
};