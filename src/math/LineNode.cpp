#include "LineNode.h"

#include <utility>

LineNode::LineNode(std::vector<std::unique_ptr<EditorNode>> children, bool braced)
    : children_(std::move(children)), braced_(braced) {}

void LineNode::AddChild(std::unique_ptr<EditorNode> child) {
    children_.push_back(std::move(child));
}

void LineNode::InsertChild(std::size_t index, std::unique_ptr<EditorNode> child) {
    if (index > children_.size()) {
        index = children_.size();
    }
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
}

void LineNode::RemoveChild(std::size_t index) {
    if (index < children_.size()) {
        children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

EditorNode* LineNode::GetChild(std::size_t index) const {
    return index < children_.size() ? children_[index].get() : nullptr;
}

std::size_t LineNode::ChildCount() const {
    return children_.size();
}

bool LineNode::IsBraced() const {
    return braced_;
}

// TODO: rendering and input are not implemented yet.
void LineNode::Render(float /*fontSize*/) {}
void LineNode::KeyPress() {}
void LineNode::MouseClick() {}
std::pair<float, float> LineNode::GetNWCorner() {
    return {0.0f, 0.0f};
}
std::pair<float, float> LineNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string LineNode::ToText() const {
    std::string text;
    if (braced_) {
        text += '{';
    }
    for (const auto& child : children_) {
        text += child->ToText();
    }
    if (braced_) {
        text += '}';
    }
    return text;
}

bool LineNode::HasChild() {
    return !children_.empty();
}
