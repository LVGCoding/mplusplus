#include "FracNode.h"

FracNode::FracNode(std::unique_ptr<EditorNode> numerator, std::unique_ptr<EditorNode> denominator)
    : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {}

// TODO: rendering and input are not implemented yet.
void FracNode::Render(float /*fontSize*/) {}
void FracNode::KeyPress() {}
void FracNode::MouseClick() {}
std::pair<float, float> FracNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> FracNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string FracNode::ToText() const {
    return "\\frac{" + (numerator_ ? numerator_->ToText() : std::string()) + "}{" +
           (denominator_ ? denominator_->ToText() : std::string()) + "}";
}

bool FracNode::HasChild() {
    return numerator_ || denominator_;
}

std::pair<EditorNode*, EditorNode*> FracNode::GetChildren() const {
    return {numerator_.get(), denominator_.get()};
}

void FracNode::SetChildren(std::unique_ptr<EditorNode> numerator,
                           std::unique_ptr<EditorNode> denominator) {
    numerator_ = std::move(numerator);
    denominator_ = std::move(denominator);
}
