#include "FracNode.h"
#include "NodeStyle.h"

FracNode::FracNode(std::unique_ptr<EditorNode> numerator,
                   std::unique_ptr<EditorNode> denominator)
    : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {}

// Numerator over a fraction bar over the denominator, both slightly smaller
// and centred. The bar spans the wider of the two plus a little overhang.
void FracNode::Render(float fontSize) {
  const float partSize = node_style::Scaled(fontSize, node_style::fractionScale);
  const float bar = node_style::RuleThickness(fontSize);
  const uint16_t overhang = static_cast<uint16_t>(std::round(fontSize * 0.15f));
  CLAY(GetClayId(),
       {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                   .padding = {overhang, overhang, 0, 0},
                   .childGap = 2,
                   .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
    if (numerator_) {
      numerator_->Render(partSize);
    } else {
      node_style::Placeholder(partSize);
    }
    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                        CLAY_SIZING_FIXED(bar)}},
                  .backgroundColor = node_style::textColor}) {}
    if (denominator_) {
      denominator_->Render(partSize);
    } else {
      node_style::Placeholder(partSize);
    }
  }
}
void FracNode::KeyPress() {}
void FracNode::MouseClick() {}
std::pair<float, float> FracNode::GetNWCorner() { return {0.0f, 0.0f}; }
std::pair<float, float> FracNode::GetSize() { return {0.0f, 0.0f}; }

std::string FracNode::ToText() const {
  return "\\frac{" + (numerator_ ? numerator_->ToText() : std::string()) +
         "}{" + (denominator_ ? denominator_->ToText() : std::string()) + "}";
}

bool FracNode::HasChild() { return numerator_ || denominator_; }

std::pair<EditorNode *, EditorNode *> FracNode::GetChildren() const {
  return {numerator_.get(), denominator_.get()};
}

void FracNode::SetChildren(std::unique_ptr<EditorNode> numerator,
                           std::unique_ptr<EditorNode> denominator) {
  numerator_ = std::move(numerator);
  denominator_ = std::move(denominator);
}
