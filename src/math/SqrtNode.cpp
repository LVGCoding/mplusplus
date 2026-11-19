#include "SqrtNode.h"

#include "LineNode.h"
#include "NodeStyle.h"
#include "raylib.h"

#include <cmath>

SqrtNode::SqrtNode(std::unique_ptr<EditorNode> root,
                   std::unique_ptr<EditorNode> argument)
    : root_(std::move(root)),
      argument_(std::move(argument)) {}

// [index] surd, then the argument under an overline. The surd stretches to the
// argument's height and its top-right corner meets the overline.
void SqrtNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float rule = node_style::RuleThickness(fontSize);
    const auto ruleWidth = static_cast<uint16_t>(rule);
    const auto gap = static_cast<uint16_t>(std::round(fontSize * 0.1f));
    const bool outlined = context.activeStructure == this;
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        if (root_) {
            CLAY_AUTO_ID({.layout = {.padding = {0, 0, 0, gap}}}) {
                node_style::Slot(outlined, [&] {
                    root_->Render(context.WithFontSize(
                        node_style::Scaled(fontSize, node_style::INDEX_SCALE)));
                });
            }
        }
        node_style::Radical(fontSize);
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .padding = {gap, gap, static_cast<uint16_t>(ruleWidth + gap), 0},
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
                      .border = {.color = node_style::TEXT_COLOR, .width = {.top = ruleWidth}}}) {
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
    if (result.kind == KeyResult::Kind::MOVED) {
        return std::move(result).Prepend({slot});
    }
    if (result.kind == KeyResult::Kind::EXIT_LEFT && slot == 1 && root_) {
        return KeyResult::Moved(JoinPath({0}, root_->LastPosition()));
    }
    if (result.kind == KeyResult::Kind::EXIT_RIGHT && slot == 0 && argument_) {
        return KeyResult::Moved(JoinPath({1}, argument_->FirstPosition()));
    }
    if (result.kind == KeyResult::Kind::DELETE_LEFT) {
        // Backspace at the start of the first slot requests deletion of the root; at the
        // start of the argument after an index it moves into the index.
        if (slot == 1 && root_) {
            return KeyResult::Moved(JoinPath({0}, root_->LastPosition()));
        }
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    }
    if (result.kind == KeyResult::Kind::DELETE_RIGHT) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

void SqrtNode::MouseClick() {}

std::pair<float,float>SqrtNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,float>SqrtNode::GetSize() {
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
CursorPath SqrtNode::PositionNearX(float x,
                                   VerticalEntry entry,
                                   const TextMeasure* measure) const {
    if (root_) {
        const auto rootBounds = LastLayoutBounds(root_->GetClayId());
        if (rootBounds && x <= rootBounds->x + rootBounds->width) {
            return JoinPath({0}, root_->PositionNearX(x, entry, measure));
        }
    }
    return argument_ ? JoinPath({1}, argument_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* SqrtNode::PathChild(PathView path,
                                std::size_t& used) const {
    if (path.empty() || path[0] > 1) {
        return nullptr;
    }
    used = 1;
    return path[0] == 0 ? root_.get() : argument_.get();
}

std::vector<EditorNode::ChildPath> SqrtNode::ChildPaths() const {
    std::vector<ChildPath> children;
    if (root_) {
        children.push_back({{0}, root_.get()});
    }
    if (argument_) {
        children.push_back({{1}, argument_.get()});
    }
    return children;
}

void SqrtNode::Normalize() {
    if (root_) {
        root_ = LineNode::EnsureLine(std::move(root_));
        root_->Normalize();
    }
    argument_ = LineNode::EnsureLine(std::move(argument_));
    argument_->Normalize();
}

std::pair<EditorNode*,EditorNode*>SqrtNode::GetChildren() const {
    return {root_.get(), argument_.get()};
}

void SqrtNode::SetChildren(std::unique_ptr<EditorNode> root,
                           std::unique_ptr<EditorNode> argument) {
    root_ = std::move(root);
    argument_ = std::move(argument);
}

std::unique_ptr<EditorNode> SqrtNode::Clone() const {
    auto copy = std::make_unique<SqrtNode>(CloneOf(root_), CloneOf(argument_));
    copy->CopyIdFrom(*this);
    return copy;
}
