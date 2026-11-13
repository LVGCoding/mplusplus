#include "SqrtNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include "raylib.h"

#include <cmath>

SqrtNode::SqrtNode(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument)
    : root_(std::move(root)), argument_(std::move(argument)) {}

// [index] surd, then the argument under an overline. The surd stretches to the
// argument's height and its top-right corner meets the overline.
// TODO: mouse input is not implemented yet.
void SqrtNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float rule = node_style::RuleThickness(fontSize);
    const auto ruleWidth = static_cast<uint16_t>(rule);
    const auto gap = static_cast<uint16_t>(std::round(fontSize * 0.1f));
    const bool outlined = context.activeStructure == this;
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                  .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        if (root_) {
            CLAY_AUTO_ID({.layout = {.padding = {0, 0, 0, gap}}}) {
                node_style::Slot(outlined, [&] {
                    root_->Render(
                        context.WithFontSize(node_style::Scaled(fontSize, node_style::indexScale)));
                });
            }
        }
        node_style::Radical(fontSize);
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .padding = {gap, gap, static_cast<uint16_t>(ruleWidth + gap), 0},
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .border = {.color = node_style::textColor, .width = {.top = ruleWidth}}}) {
            node_style::Slot(outlined, [&] {
                if (argument_) {
                    argument_->Render(context);
                } else {
                    node_style::Placeholder(fontSize);
                }
            });
        }
    }
}

KeyResult SqrtNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* child = PathChild(event.path, used);
    if (!child) {
        return {};
    }
    const std::size_t slot = event.path[0];
    if (slot == 0 && event.codepoint == U']') {
        return {};  // would end the [index] early when saved
    }
    KeyResult result = child->KeyPress(event.Descend(used));
    if (result.kind == KeyResult::Kind::Moved) {
        return std::move(result).Prepend({slot});
    }
    if (result.kind == KeyResult::Kind::ExitLeft && slot == 1 && root_) {
        return KeyResult::Moved(JoinPath({0}, root_->LastPosition()));
    }
    if (result.kind == KeyResult::Kind::ExitRight && slot == 0 && argument_) {
        return KeyResult::Moved(JoinPath({1}, argument_->FirstPosition()));
    }
    if (result.kind == KeyResult::Kind::DeleteLeft) {
        // Backspace at the start of the first slot deletes the root; at the
        // start of the argument after an index it moves into the index.
        if (slot == 1 && root_) {
            return KeyResult::Moved(JoinPath({0}, root_->LastPosition()));
        }
        return KeyResult::Exit(KeyResult::Kind::DeleteSelf);
    }
    if (result.kind == KeyResult::Kind::DeleteRight) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

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

CursorPath SqrtNode::FirstPosition() const {
    if (root_) {
        return JoinPath({0}, root_->FirstPosition());
    }
    return argument_ ? JoinPath({1}, argument_->FirstPosition()) : CursorPath{};
}

CursorPath SqrtNode::LastPosition() const {
    return argument_ ? JoinPath({1}, argument_->LastPosition()) : CursorPath{};
}

// The index sits left of the surd, so x left of the argument picks the index.
CursorPath SqrtNode::PositionNearX(float x, VerticalEntry entry, const TextMeasure* measure) const {
    if (root_) {
        const auto rootBounds = LastLayoutBounds(root_->GetClayId());
        if (rootBounds && x <= rootBounds->x + rootBounds->width) {
            return JoinPath({0}, root_->PositionNearX(x, entry, measure));
        }
    }
    return argument_ ? JoinPath({1}, argument_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* SqrtNode::PathChild(PathView path, std::size_t& used) const {
    if (path.empty() || path[0] > 1) {
        return nullptr;
    }
    used = 1;
    return path[0] == 0 ? root_.get() : argument_.get();
}

void SqrtNode::Normalize() {
    if (root_) {
        root_ = LineNode::EnsureLine(std::move(root_));
        root_->Normalize();
    }
    argument_ = LineNode::EnsureLine(std::move(argument_));
    argument_->Normalize();
}

std::pair<EditorNode*, EditorNode*> SqrtNode::GetChildren() const {
    return {root_.get(), argument_.get()};
}

void SqrtNode::SetChildren(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument) {
    root_ = std::move(root);
    argument_ = std::move(argument);
}
