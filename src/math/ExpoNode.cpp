#include "ExpoNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <cmath>

ExpoNode::ExpoNode(std::unique_ptr<EditorNode> argument)
    : argument(std::move(argument)) {}


void ExpoNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float exponentSize = node_style::Scaled(fontSize, node_style::INDEX_SCALE);
    const bool outlined = context.activeStructure == this;

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                    .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_BOTTOM},}}) {
        node_style::Slot(outlined, [&] {
            if (argument) {
                argument->Render(context.WithFontSize(exponentSize));
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
    if (result.kind == KeyResult::Kind::MOVED) {
        return std::move(result).Prepend({0});
    }
    if (result.kind == KeyResult::Kind::DELETE_LEFT) {
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    }
    if (result.kind == KeyResult::Kind::DELETE_RIGHT) {
        return {};
    }
    return std::move(result).Prepend({0});
}

void ExpoNode::MouseClick() {}

std::pair<float, float> ExpoNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float, float> ExpoNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string ExpoNode::ToText() const {
    return "\\exp{" + (argument ? argument->ToText() : std::string()) + "}";
}

bool ExpoNode::HasChild() {
    return argument != nullptr;
}

CursorPath ExpoNode::FirstPosition() const {
    return argument ? JoinPath({0}, argument->FirstPosition()) : CursorPath{};
}

CursorPath ExpoNode::LastPosition() const {
    return argument ? JoinPath({0}, argument->LastPosition()) : CursorPath{};
}

CursorPath ExpoNode::PositionNearX(float x,
                                   VerticalEntry entry,
                                   const TextMeasure* measure) const {
    return argument ? JoinPath({0}, argument->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* ExpoNode::PathChild(PathView path,
                                std::size_t& used) const {
    if (path.empty() || path[0] != 0) {
        return nullptr;
    }
    used = 1;
    return argument.get();
}

std::vector<EditorNode::ChildPath> ExpoNode::ChildPaths() const {
    if (!argument) {
        return {};
    }
    return {{{0}, argument.get()}};
}

void ExpoNode::Normalize() {
    argument = LineNode::EnsureLine(std::move(argument));
    argument->Normalize();
}

std::pair<EditorNode*, EditorNode*> ExpoNode::GetChildren() const {
    return {nullptr, argument.get()};
}

void ExpoNode::SetChildren(std::unique_ptr<EditorNode> root,
                           std::unique_ptr<EditorNode> argument) {
    (void)root;
    this->argument = std::move(argument);
}

std::unique_ptr<EditorNode> ExpoNode::Clone() const {
    auto copy = std::make_unique<ExpoNode>(CloneOf(argument));
    copy->CopyIdFrom(*this);
    return copy;
}