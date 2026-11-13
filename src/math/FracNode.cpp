#include "FracNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include "raylib.h"

#include <cmath>

FracNode::FracNode(std::unique_ptr<EditorNode> numerator, std::unique_ptr<EditorNode> denominator)
    : numerator_(std::move(numerator)), denominator_(std::move(denominator)) {}

// Numerator over a fraction bar over the denominator, both slightly smaller
// and centred. The bar spans the wider of the two plus a little overhang.
void FracNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const RenderContext part =
        context.WithFontSize(node_style::Scaled(fontSize, node_style::fractionScale));
    const float bar = node_style::RuleThickness(fontSize);
    const auto overhang = static_cast<uint16_t>(std::round(fontSize * 0.15f));
    const bool outlined = context.activeStructure == this;
    auto renderPart = [&](const std::unique_ptr<EditorNode>& node) {
        node_style::Slot(outlined, [&] {
            if (node) {
                node->Render(part);
            } else {
                node_style::Placeholder(part.fontSize);
            }
        });
    };
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                  .padding = {overhang, overhang, 0, 0},
                                  .childGap = 2,
                                  .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                                  .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        renderPart(numerator_);
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(bar)}},
                      .backgroundColor = node_style::textColor}) {}
        renderPart(denominator_);
    }
}

KeyResult FracNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* child = PathChild(event.path, used);
    if (!child) {
        return {};
    }
    const std::size_t slot = event.path[0];
    KeyResult result = child->KeyPress(event.Descend(used));
    if (result.kind == KeyResult::Kind::Moved) {
        return std::move(result).Prepend({slot});
    }
    if (result.kind == KeyResult::Kind::ExitUp && slot == 1 && numerator_) {
        return KeyResult::Moved(JoinPath(
            {0}, numerator_->PositionNearX(event.cursorX, VerticalEntry::FromBelow, event.measure)));
    }
    if (result.kind == KeyResult::Kind::ExitDown && slot == 0 && denominator_) {
        return KeyResult::Moved(JoinPath(
            {1}, denominator_->PositionNearX(event.cursorX, VerticalEntry::FromAbove, event.measure)));
    }
    if (result.kind == KeyResult::Kind::DeleteLeft) {
        // Backspace at the start of the numerator deletes the fraction; at the
        // start of the denominator it moves back to the numerator.
        if (slot == 0 || !numerator_) {
            return KeyResult::Exit(KeyResult::Kind::DeleteSelf);
        }
        return KeyResult::Moved(JoinPath({0}, numerator_->LastPosition()));
    }
    if (result.kind == KeyResult::Kind::DeleteRight) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

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

// Entering from either side lands in the numerator.
CursorPath FracNode::FirstPosition() const {
    return numerator_ ? JoinPath({0}, numerator_->FirstPosition()) : CursorPath{};
}

CursorPath FracNode::LastPosition() const {
    return numerator_ ? JoinPath({0}, numerator_->LastPosition()) : CursorPath{};
}

CursorPath FracNode::PositionNearX(float x, VerticalEntry entry, const TextMeasure* measure) const {
    if (entry == VerticalEntry::FromBelow && denominator_) {
        return JoinPath({1}, denominator_->PositionNearX(x, entry, measure));
    }
    return numerator_ ? JoinPath({0}, numerator_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* FracNode::PathChild(PathView path, std::size_t& used) const {
    if (path.empty() || path[0] > 1) {
        return nullptr;
    }
    used = 1;
    return path[0] == 0 ? numerator_.get() : denominator_.get();
}

void FracNode::Normalize() {
    numerator_ = LineNode::EnsureLine(std::move(numerator_));
    denominator_ = LineNode::EnsureLine(std::move(denominator_));
    numerator_->Normalize();
    denominator_->Normalize();
}

std::pair<EditorNode*, EditorNode*> FracNode::GetChildren() const {
    return {numerator_.get(), denominator_.get()};
}

void FracNode::SetChildren(std::unique_ptr<EditorNode> numerator,
                           std::unique_ptr<EditorNode> denominator) {
    numerator_ = std::move(numerator);
    denominator_ = std::move(denominator);
}

std::unique_ptr<EditorNode> FracNode::Clone() const {
    auto copy = std::make_unique<FracNode>(CloneOf(numerator_), CloneOf(denominator_));
    copy->CopyIdFrom(*this);
    return copy;
}
