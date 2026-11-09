#include "TextNode.h"

#include <utility>

TextNode::TextNode(std::string text) : text_(std::move(text)) {}

// TODO: rendering and input are not implemented yet.
void TextNode::Render(float /*fontSize*/) {}
void TextNode::KeyPress() {}
void TextNode::MouseClick() {}
std::pair<float, float> TextNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> TextNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string TextNode::ToText() const {
    return text_;
}

bool TextNode::HasChild() {
    return false;
}

const std::string& TextNode::GetText() const {
    return text_;
}

void TextNode::SetText(std::string text) {
    text_ = std::move(text);
}
