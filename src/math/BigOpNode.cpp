#include "BigOpNode.h"

#include "LineNode.h"
#include "NodeStyle.h"
#include "ui/font.h"

#include <cmath>

BigOpNode::BigOpNode(Kind kind,
                     std::unique_ptr<EditorNode> lower,
                     std::unique_ptr<EditorNode> upper)
    : kind_(kind),
      lower_(std::move(lower)),
      upper_(std::move(upper)) {}

namespace {
constexpr float SYMBOL_SCALE = 1.6f;
constexpr float LIMIT_SCALE = 0.65f;
}  // namespace

// The upper limit, the symbol, then the lower limit, centred in a column.
void BigOpNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const RenderContext limits =
        context.WithFontSize(node_style::Scaled(fontSize, LIMIT_SCALE));
    const bool outlined = context.activeStructure == this;
    auto renderLimit = [&](const std::unique_ptr<EditorNode>& node) {
        node_style::Slot(outlined, [&] {
            if (node) {
                node->Render(limits);
            } else {
                node_style::Placeholder(limits.fontSize);
            }
        });
    };
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .padding = {2, 2, 0, 0},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        // An empty upper limit (Σ_{x ∈ S}) only shows while editing the operator.
        const auto* upperLine = dynamic_cast<const LineNode*>(upper_.get());
        if (outlined || !upperLine || !upperLine->IsEmpty()) {
            renderLimit(upper_);
        }
        node_style::Text(Glyph(kind_), std::round(fontSize * SYMBOL_SCALE), FONT_MATH);
        renderLimit(lower_);
    }
}

KeyResult BigOpNode::KeyPress(const KeyEvent& event) {
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
    if (result.kind == KeyResult::Kind::EXIT_UP && slot == 0 && upper_) {
        return KeyResult::Moved(JoinPath(
            {1}, upper_->PositionNearX(event.cursorX, VerticalEntry::FROM_BELOW, event.measure)));
    }
    if (result.kind == KeyResult::Kind::EXIT_DOWN && slot == 1 && lower_) {
        return KeyResult::Moved(JoinPath(
            {0}, lower_->PositionNearX(event.cursorX, VerticalEntry::FROM_ABOVE, event.measure)));
    }
    if (result.kind == KeyResult::Kind::DELETE_LEFT) {
        if (slot == 0 || !lower_) {
            return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
        }
        return KeyResult::Moved(JoinPath({0}, lower_->LastPosition()));
    }
    if (result.kind == KeyResult::Kind::DELETE_RIGHT) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

void BigOpNode::MouseClick() {}

std::pair<float,
          float>
BigOpNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,
          float>
BigOpNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string BigOpNode::ToText() const {
    return "\\" + std::string(CommandName(kind_)) + "_{" +
           (lower_ ? lower_->ToText() : std::string()) + "}^{" +
           (upper_ ? upper_->ToText() : std::string()) + "}";
}

bool BigOpNode::HasChild() {
    return lower_ || upper_;
}

CursorPath BigOpNode::FirstPosition() const {
    return lower_ ? JoinPath({0}, lower_->FirstPosition()) : CursorPath{};
}

CursorPath BigOpNode::LastPosition() const {
    return lower_ ? JoinPath({0}, lower_->LastPosition()) : CursorPath{};
}

CursorPath BigOpNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    if (entry == VerticalEntry::FROM_ABOVE && upper_) {
        return JoinPath({1}, upper_->PositionNearX(x, entry, measure));
    }
    return lower_ ? JoinPath({0}, lower_->PositionNearX(x, entry, measure)) : CursorPath{};
}

EditorNode* BigOpNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.empty() || path[0] > 1) {
        return nullptr;
    }
    used = 1;
    return path[0] == 0 ? lower_.get() : upper_.get();
}

std::vector<EditorNode::ChildPath> BigOpNode::ChildPaths() const {
    std::vector<ChildPath> children;
    if (lower_) {
        children.push_back({{0}, lower_.get()});
    }
    if (upper_) {
        children.push_back({{1}, upper_.get()});
    }
    return children;
}

void BigOpNode::Normalize() {
    lower_ = LineNode::EnsureLine(std::move(lower_));
    upper_ = LineNode::EnsureLine(std::move(upper_));
    lower_->Normalize();
    upper_->Normalize();
}

std::unique_ptr<EditorNode> BigOpNode::Clone() const {
    auto copy = std::make_unique<BigOpNode>(kind_, CloneOf(lower_), CloneOf(upper_));
    copy->CopyIdFrom(*this);
    return copy;
}

std::string_view BigOpNode::CommandName(Kind kind) {
    switch (kind) {
    case Kind::PRODUCT:
        return "prod";
    case Kind::UNION:
        return "bigcup";
    case Kind::INTERSECTION:
        return "bigcap";
    case Kind::SUM:
        break;
    }
    return "sum";
}

std::optional<BigOpNode::Kind> BigOpNode::KindFromCommand(std::string_view name) {
    for (Kind kind : {Kind::SUM, Kind::PRODUCT, Kind::UNION, Kind::INTERSECTION}) {
        if (CommandName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}

std::string_view BigOpNode::Glyph(Kind kind) {
    switch (kind) {
    case Kind::PRODUCT:
        return "∏";
    case Kind::UNION:
        return "⋃";
    case Kind::INTERSECTION:
        return "⋂";
    case Kind::SUM:
        break;
    }
    return "∑";
}
