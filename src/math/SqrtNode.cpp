#include "SqrtNode.h"

SqrtNode::SqrtNode(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument)
    : root_(std::move(root)), argument_(std::move(argument)) {}

// TODO: rendering and input are not implemented yet.
void SqrtNode::Render(float /*fontSize*/) {}
void SqrtNode::KeyPress() {}
void SqrtNode::MouseClick() {}
std::pair<float, float> SqrtNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> SqrtNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string SqrtNode::ToText() const {
    std::string text = "\\sqrt";
    if (root_) {
        text += "[" + root_->ToText() + "]";
    }
    text += "{" + (argument_ ? argument_->ToText() : std::string()) + "}";
    return text;
}

bool SqrtNode::HasChild() {
    return root_ || argument_;
}

std::pair<EditorNode*, EditorNode*> SqrtNode::GetChildren() const {
    return {root_.get(), argument_.get()};
}

void SqrtNode::SetChildren(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument) {
    root_ = std::move(root);
    argument_ = std::move(argument);
}
