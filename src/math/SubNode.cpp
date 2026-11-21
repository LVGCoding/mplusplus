#include "SubNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <cmath>

SubNode::SubNode(std::unique_ptr<EditorNode> index)
    : index_(std::move(index)) {}

// The index in a smaller font, at the bottom of a box a little taller than
// its base (lines center their children), so it sits at the base's bottom
// right, below the line.
void SubNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float indexSize = node_style::Scaled(fontSize, node_style::INDEX_SCALE);
    const bool outlined = context.activeStructure == this;
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(std::round(context.ScriptBoxHeight()))},
                     .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM}}}) {
        node_style::Slot(outlined, [&] {
            if (index_) {
                index_->Render(context.WithFontSize(indexSize));
            } else {
                node_style::Placeholder(indexSize);
            }
        });
    }
}

KeyResult SubNode::KeyPress(const KeyEvent& event) {
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

void SubNode::MouseClick() {}

std::pair<float,
          float>
SubNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,
          float>
SubNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string SubNode::ToText() const {
    return "_{" + (index_ ? index_->ToText() : std::string()) + "}";
}

bool SubNode::HasChild() {
    return index_ != nullptr;
}

CursorPath SubNode::FirstPosition() const {
    return index_ ? JoinPath({0}, index_->FirstPosition()) : CursorPath{};
}

CursorPath SubNode::LastPosition() const {
    return index_ ? JoinPath({0}, index_->LastPosition()) : CursorPath{};
}

CursorPath SubNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    return index_ ? JoinPath({0}, index_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* SubNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.empty() || path[0] != 0) {
        return nullptr;
    }
    used = 1;
    return index_.get();
}

std::vector<EditorNode::ChildPath> SubNode::ChildPaths() const {
    if (!index_) {
        return {};
    }
    return {{{0}, index_.get()}};
}

void SubNode::Normalize() {
    index_ = LineNode::EnsureLine(std::move(index_));
    index_->Normalize();
}

std::unique_ptr<EditorNode> SubNode::Clone() const {
    auto copy = std::make_unique<SubNode>(CloneOf(index_));
    copy->CopyIdFrom(*this);
    return copy;
}

