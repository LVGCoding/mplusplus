#include "SqrtNode.h"

#include "NodeStyle.h"

SqrtNode::SqrtNode(std::unique_ptr<EditorNode> root, std::unique_ptr<EditorNode> argument)
    : root_(std::move(root)), argument_(std::move(argument)) {}

// [index] surd, then the argument under an overline. The surd stretches to the
// argument's height and its top-right corner meets the overline.
// TODO: input is not implemented yet.
void SqrtNode::Render(float fontSize) {
    const float rule = node_style::RuleThickness(fontSize);
    const auto ruleWidth = static_cast<uint16_t>(rule);
    const auto gap = static_cast<uint16_t>(std::round(fontSize * 0.1f));
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        if (root_) {
            CLAY_AUTO_ID({.layout = {.padding = {0, 0, 0, gap}}}) {
                root_->Render(node_style::Scaled(fontSize, node_style::indexScale));
            }
        }
        node_style::Radical(fontSize);
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .padding = {gap, gap, static_cast<uint16_t>(ruleWidth + gap), 0},
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .border = {.color = node_style::textColor, .width = {.top = ruleWidth}}}) {
            if (argument_) {
                argument_->Render(fontSize);
            } else {
                node_style::Placeholder(fontSize);
            }
        }
    }
}
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
