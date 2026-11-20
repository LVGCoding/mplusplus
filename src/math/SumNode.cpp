#include "SumNode.h"

#include "LineNode.h"
#include "NodeStyle.h"

#include <tuple>

SumNode::SumNode(std::unique_ptr<EditorNode> summand,
                 std::unique_ptr<EditorNode> index,
                 std::unique_ptr<EditorNode> upperLimit)
    : summand(std::move(summand)),
      index(std::move(index)),
      upperLimit(std::move(upperLimit)) {}

// Sigma in the middle, upper and lower limits above and below it, and the
// summand on the right.
void SumNode::Render(const RenderContext& context) {
    const float fontSize = context.fontSize;
    const float limitSize = node_style::Scaled(fontSize, node_style::INDEX_SCALE);
    const bool outlined = context.activeStructure == this;

    auto renderLimit = [&](const std::unique_ptr<EditorNode>& node) {
        node_style::Slot(outlined, [&] {
            if (node) {
                node->Render(context.WithFontSize(limitSize));
            } else {
                node_style::Placeholder(limitSize);
            }
        });
    };

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .childGap = 2,
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 1,
                                 .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
            renderLimit(upperLimit);
            node_style::Text("\xCE\xA3", fontSize);
            renderLimit(index);
        }
        node_style::Slot(outlined, [&] {
            if (summand) {
                summand->Render(context);
            } else {
                node_style::Placeholder(fontSize);
            }
        });
    }
}

KeyResult SumNode::KeyPress(const KeyEvent& event) {
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
    if (result.kind == KeyResult::Kind::EXIT_RIGHT) {
        if (slot == 1 && upperLimit) {
            return KeyResult::Moved(JoinPath({2}, upperLimit->FirstPosition()));
        }
        if (slot == 2 && summand) {
            return KeyResult::Moved(JoinPath({0}, summand->FirstPosition()));
        }
    }
    if (result.kind == KeyResult::Kind::EXIT_LEFT) {
        if (slot == 2 && index) {
            return KeyResult::Moved(JoinPath({1}, index->LastPosition()));
        }
        if (slot == 0 && index) {
            return KeyResult::Moved(JoinPath({1}, index->LastPosition()));
        }
    }
    if (result.kind == KeyResult::Kind::EXIT_UP) {
        if (slot == 1 && upperLimit) {
            return KeyResult::Moved(JoinPath(
                {2}, upperLimit->PositionNearX(event.cursorX, VerticalEntry::FROM_BELOW, event.measure)));
        }
    }
    if (result.kind == KeyResult::Kind::EXIT_DOWN) {
        if (slot == 2 && index) {
            return KeyResult::Moved(JoinPath(
                {1}, index->PositionNearX(event.cursorX, VerticalEntry::FROM_ABOVE, event.measure)));
        }
    }
    if (result.kind == KeyResult::Kind::DELETE_LEFT) {
        if (slot == 0 && index) {
            return KeyResult::Moved(JoinPath({1}, index->LastPosition()));
        }
        if (slot == 1 || slot == 2) {
            return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
        }
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    }
    if (result.kind == KeyResult::Kind::DELETE_RIGHT) {
        return {};
    }
    return std::move(result).Prepend({slot});
}

void SumNode::MouseClick() {}

std::pair<float, float> SumNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float, float> SumNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string SumNode::ToText() const {
    return "\\sum_" + (index ? "{" + index->ToText() + "}" : "{}") +
           "^" + (upperLimit ? "{" + upperLimit->ToText() + "}" : "{}") +
           "{" + (summand ? summand->ToText() : std::string()) + "}";
}

bool SumNode::HasChild() {
    return summand || index || upperLimit;
}

CursorPath SumNode::FirstPosition() const {
    if (index) {
        return JoinPath({1}, index->FirstPosition());
    }
    if (upperLimit) {
        return JoinPath({2}, upperLimit->FirstPosition());
    }
    return summand ? JoinPath({0}, summand->FirstPosition()) : CursorPath{};
}

CursorPath SumNode::LastPosition() const {
    return summand ? JoinPath({0}, summand->LastPosition()) : CursorPath{};
}

CursorPath SumNode::PositionNearX(float x,
                                  VerticalEntry entry,
                                  const TextMeasure* measure) const {
    if (entry == VerticalEntry::FROM_ABOVE && upperLimit) {
        return JoinPath({2}, upperLimit->PositionNearX(x, entry, measure));
    }
    if (entry == VerticalEntry::FROM_BELOW && index) {
        return JoinPath({1}, index->PositionNearX(x, entry, measure));
    }
    return summand ? JoinPath({0}, summand->PositionNearX(x, entry, measure))
                    : FirstPosition();
}

EditorNode* SumNode::PathChild(PathView path,
                               std::size_t& used) const {
    if (path.empty() || path[0] > 2) {
        return nullptr;
    }
    used = 1;
    if (path[0] == 0) return summand.get();
    if (path[0] == 1) return index.get();
    return upperLimit.get();
}

std::vector<EditorNode::ChildPath> SumNode::ChildPaths() const {
    std::vector<ChildPath> children;
    if (summand) children.push_back({{0}, summand.get()});
    if (index) children.push_back({{1}, index.get()});
    if (upperLimit) children.push_back({{2}, upperLimit.get()});
    return children;
}

void SumNode::Normalize() {
    summand = LineNode::EnsureLine(std::move(summand));
    index = LineNode::EnsureLine(std::move(index));
    upperLimit = LineNode::EnsureLine(std::move(upperLimit));
    summand->Normalize();
    index->Normalize();
    upperLimit->Normalize();
}

std::tuple<EditorNode*, EditorNode*, EditorNode*> SumNode::GetChildren() const {
    return {summand.get(), index.get(), upperLimit.get()};
}

void SumNode::SetChildren(std::unique_ptr<EditorNode> summand,
                          std::unique_ptr<EditorNode> index,
                          std::unique_ptr<EditorNode> upperLimit) {
    summand = std::move(summand);
    index = std::move(index);
    upperLimit = std::move(upperLimit);
}

std::unique_ptr<EditorNode> SumNode::Clone() const {
    auto copy = std::make_unique<SumNode>(
        CloneOf(summand), CloneOf(index), CloneOf(upperLimit));
    copy->CopyIdFrom(*this);
    return copy;
}

