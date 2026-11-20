#include "DelimNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <cmath>

DelimNode::DelimNode(Kind kind,
                     std::unique_ptr<EditorNode> argument)
    : kind_(kind),
      argument_(std::move(argument)) {}

namespace {
node_style::Delimiter DelimiterFor(DelimNode::Kind kind) {
    switch (kind) {
    case DelimNode::Kind::FLOOR:
        return node_style::Delimiter::FLOOR;
    case DelimNode::Kind::CEIL:
        return node_style::Delimiter::CEIL;
    case DelimNode::Kind::ABS:
        break;
    }
    return node_style::Delimiter::BAR;
}
}  // namespace

void DelimNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const auto gap = static_cast<uint16_t>(std::round(fontSize * 0.08f));
    const bool outlined = context.activeStructure == this;
    const node_style::Delimiter delimiter = DelimiterFor(kind_);
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .childGap = gap,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        node_style::DelimiterSymbol(delimiter, false, fontSize);
        node_style::Slot(outlined, [&] {
            if (argument_) {
                argument_->Render(context);
            } else {
                node_style::Placeholder(fontSize);
            }
        });
        node_style::DelimiterSymbol(delimiter, true, fontSize);
    }
}

KeyResult DelimNode::KeyPress(const KeyEvent& event) {
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

void DelimNode::MouseClick() {}

std::pair<float,
          float>
DelimNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,
          float>
DelimNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string DelimNode::ToText() const {
    return "\\" + std::string(CommandName(kind_)) + "{" +
           (argument_ ? argument_->ToText() : std::string()) + "}";
}

bool DelimNode::HasChild() {
    return argument_ != nullptr;
}

CursorPath DelimNode::FirstPosition() const {
    return argument_ ? JoinPath({0}, argument_->FirstPosition()) : CursorPath{};
}

CursorPath DelimNode::LastPosition() const {
    return argument_ ? JoinPath({0}, argument_->LastPosition()) : CursorPath{};
}

CursorPath DelimNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    return argument_ ? JoinPath({0}, argument_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* DelimNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.empty() || path[0] != 0) {
        return nullptr;
    }
    used = 1;
    return argument_.get();
}

std::vector<EditorNode::ChildPath> DelimNode::ChildPaths() const {
    if (!argument_) {
        return {};
    }
    return {{{0}, argument_.get()}};
}

void DelimNode::Normalize() {
    argument_ = LineNode::EnsureLine(std::move(argument_));
    argument_->Normalize();
}

std::unique_ptr<EditorNode> DelimNode::Clone() const {
    auto copy = std::make_unique<DelimNode>(kind_, CloneOf(argument_));
    copy->CopyIdFrom(*this);
    return copy;
}

std::string_view DelimNode::CommandName(Kind kind) {
    switch (kind) {
    case Kind::FLOOR:
        return "floor";
    case Kind::CEIL:
        return "ceil";
    case Kind::ABS:
        break;
    }
    return "abs";
}

std::optional<DelimNode::Kind> DelimNode::KindFromCommand(std::string_view name) {
    for (Kind kind : {Kind::ABS, Kind::FLOOR, Kind::CEIL}) {
        if (CommandName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}
