#include "ExpoNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <cmath>

ExpoNode::ExpoNode(std::unique_ptr<EditorNode> exponent)
    : exponent_(std::move(exponent)) {}

// The exponent in a smaller font, at the top of a box a little taller than
// its base (lines center their children), so it sits at the base's top right.
void ExpoNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float exponentSize = node_style::Scaled(fontSize, node_style::INDEX_SCALE);
    const bool outlined = context.activeStructure == this;
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(std::round(context.ScriptBoxHeight()))},
                     .childAlignment = {.y = CLAY_ALIGN_Y_TOP}}}) {
        node_style::Slot(outlined, [&] {
            if (exponent_) {
                exponent_->Render(context.WithFontSize(exponentSize));
            } else {
                node_style::Placeholder(exponentSize);
            }
        });
    }
}

KeyResult ExpoNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* child = PathChild(event.path, used);
    if (!child) {
        return {};
    }
    KeyResult result = child->KeyPress(event.Descend(used));
    switch (result.kind) {
    case KeyResult::Kind::DELETE_LEFT:
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    case KeyResult::Kind::DELETE_RIGHT:
        return {};
    default:
        return std::move(result).Prepend({0});
    }
}

void ExpoNode::MouseClick() {}

std::pair<float,
          float>
ExpoNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,
          float>
ExpoNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string ExpoNode::ToText() const {
    return "^{" + (exponent_ ? exponent_->ToText() : std::string()) + "}";
}

bool ExpoNode::HasChild() {
    return exponent_ != nullptr;
}

CursorPath ExpoNode::FirstPosition() const {
    return exponent_ ? JoinPath({0}, exponent_->FirstPosition()) : CursorPath{};
}

CursorPath ExpoNode::LastPosition() const {
    return exponent_ ? JoinPath({0}, exponent_->LastPosition()) : CursorPath{};
}

CursorPath ExpoNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    return exponent_ ? JoinPath({0}, exponent_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* ExpoNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.empty() || path[0] != 0) {
        return nullptr;
    }
    used = 1;
    return exponent_.get();
}

std::vector<EditorNode::ChildPath> ExpoNode::ChildPaths() const {
    if (!exponent_) {
        return {};
    }
    return {{{0}, exponent_.get()}};
}

void ExpoNode::Normalize() {
    exponent_ = LineNode::EnsureLine(std::move(exponent_));
    exponent_->Normalize();
}

std::unique_ptr<EditorNode> ExpoNode::Clone() const {
    auto copy = std::make_unique<ExpoNode>(CloneOf(exponent_));
    copy->CopyIdFrom(*this);
    return copy;
}

