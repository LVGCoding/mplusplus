#include "FracNode.h"
#include "clay.h"

FracNode::FracNode(std::unique_ptr<EditorNode> numerator, std::unique_ptr<EditorNode> denominator)
    : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {}

void FracNode::Render(float fontSize) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                    .padding = CLAY_PADDING_ALL(2),
                    .childGap = 2,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
          .border = {.color = {255, 255, 255, 255},
                     .width = {.betweenChildren = 1}}}) {
        if (numerator_) {
            numerator_->Render(fontSize*.8);
        }
        if (denominator_) {
            denominator_->Render(fontSize*.8);
        }
    }
}
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
