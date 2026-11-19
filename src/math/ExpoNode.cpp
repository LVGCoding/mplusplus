#include "ExpoNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

ExpoNode::ExpoNode(std::unique_ptr<EditorNode> base,
                   std::unique_ptr<EditorNode> exponent)
    : base(std::move(base)),
      exponent(std::move(exponent)) {}


void ExpoNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float exponentSize = node_style::Scaled(fontSize, node_style::INDEX_SCALE);
    const bool outlined = context.activeStructure == this;

    auto renderPart = [&](const std::unique_ptr<EditorNode>& node,
                          const RenderContext& partContext) {
        node_style::Slot(outlined, [&] {
            if (node) {
                node->Render(partContext);
            } else {
                node_style::Placeholder(partContext.fontSize);
            }
        });
    };

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .childGap = 1,
                     .childAlignment = {.y = CLAY_ALIGN_Y_TOP},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        renderPart(base, context);
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .childAlignment = {.y = CLAY_ALIGN_Y_TOP}}}) {
            renderPart(exponent, context.WithFontSize(exponentSize));
        }
    }
}

KeyResult ExpoNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* child = PathChild(event.path, used);
    if (!child) {
        return {};
    }
    const std::size_t slot = event.path[0];
    KeyResult result = child->KeyPress(event.Descend(used));
    if (result.kind == KeyResult::Kind::MOVED) {
        return std::move(result).Prepend({slot});
    }
    if (result.kind == KeyResult::Kind::EXIT_RIGHT && slot == 0 && exponent) {
        return KeyResult::Moved(JoinPath({1}, exponent->FirstPosition()));
    }
    if (result.kind == KeyResult::Kind::EXIT_LEFT && slot == 1 && base) {
        return KeyResult::Moved(JoinPath({0}, base->LastPosition()));
    }
    if (result.kind == KeyResult::Kind::EXIT_UP && slot == 1 && base) {
        return KeyResult::Moved(JoinPath(
            {0}, base->PositionNearX(event.cursorX, VerticalEntry::FROM_BELOW, event.measure)));
    }
    if (result.kind == KeyResult::Kind::EXIT_DOWN && slot == 0 && exponent) {
        return KeyResult::Moved(JoinPath(
            {1}, exponent->PositionNearX(event.cursorX, VerticalEntry::FROM_ABOVE, event.measure)));
    }
    if (result.kind == KeyResult::Kind::DELETE_LEFT) {
        if (slot == 1 && base) {
            return KeyResult::Moved(JoinPath({0}, base->LastPosition()));
        }
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    }
    if (result.kind == KeyResult::Kind::DELETE_RIGHT) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

void ExpoNode::MouseClick() {}

std::pair<float, float> ExpoNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float, float> ExpoNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string ExpoNode::ToText() const {
    return "\\exp{" + (base ? base->ToText() : std::string()) + "}{" +
           (exponent ? exponent->ToText() : std::string()) + "}";
}

bool ExpoNode::HasChild() {
    return base || exponent;
}

CursorPath ExpoNode::FirstPosition() const {
    return base ? JoinPath({0}, base->FirstPosition()) : CursorPath{};
}

CursorPath ExpoNode::LastPosition() const {
    return exponent ? JoinPath({1}, exponent->LastPosition()) : CursorPath{};
}

CursorPath ExpoNode::PositionNearX(float x,
                                   VerticalEntry entry,
                                   const TextMeasure* measure) const {
    if (entry == VerticalEntry::FROM_ABOVE && exponent) {
        return JoinPath({1}, exponent->PositionNearX(x, entry, measure));
    }
    return base ? JoinPath({0}, base->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* ExpoNode::PathChild(PathView path,
                                std::size_t& used) const {
    if (path.empty() || path[0] > 1) {
        return nullptr;
    }
    used = 1;
    return path[0] == 0 ? base.get() : exponent.get();
}

std::vector<EditorNode::ChildPath> ExpoNode::ChildPaths() const {
    std::vector<ChildPath> children;
    if (base) {
        children.push_back({{0}, base.get()});
    }
    if (exponent) {
        children.push_back({{1}, exponent.get()});
    }
    return children;
}

void ExpoNode::Normalize() {
    base = LineNode::EnsureLine(std::move(base));
    exponent = LineNode::EnsureLine(std::move(exponent));
    base->Normalize();
    exponent->Normalize();
}

std::pair<EditorNode*, EditorNode*> ExpoNode::GetChildren() const {
    return {base.get(), exponent.get()};
}

void ExpoNode::SetChildren(std::unique_ptr<EditorNode> base,
                           std::unique_ptr<EditorNode> exponent) {
    this->base = std::move(base);
    this->exponent = std::move(exponent);
}

std::unique_ptr<EditorNode> ExpoNode::Clone() const {
    auto copy = std::make_unique<ExpoNode>(CloneOf(base), CloneOf(exponent));
    copy->CopyIdFrom(*this);
    return copy;
}
