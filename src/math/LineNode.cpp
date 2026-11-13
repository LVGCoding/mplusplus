#include "LineNode.h"

#include "NodeStyle.h"
#include "TextNode.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
TextNode* AsText(const std::unique_ptr<EditorNode>& node) {
    return dynamic_cast<TextNode*>(node.get());
}
}  // namespace

LineNode::LineNode(std::vector<std::unique_ptr<EditorNode>> children, bool braced)
    : children_(std::move(children)), braced_(braced) {}

std::unique_ptr<EditorNode> LineNode::EnsureLine(std::unique_ptr<EditorNode> node) {
    if (!node) {
        return std::make_unique<LineNode>();
    }
    if (dynamic_cast<LineNode*>(node.get())) {
        return node;
    }
    std::vector<std::unique_ptr<EditorNode>> children;
    children.push_back(std::move(node));
    return std::make_unique<LineNode>(std::move(children));
}

void LineNode::AddChild(std::unique_ptr<EditorNode> child) {
    children_.push_back(std::move(child));
}

void LineNode::InsertChild(std::size_t index, std::unique_ptr<EditorNode> child) {
    if (index > children_.size()) {
        index = children_.size();
    }
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
}

void LineNode::RemoveChild(std::size_t index) {
    if (index < children_.size()) {
        children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

EditorNode* LineNode::GetChild(std::size_t index) const {
    return index < children_.size() ? children_[index].get() : nullptr;
}

std::size_t LineNode::ChildCount() const {
    return children_.size();
}

bool LineNode::IsBraced() const {
    return braced_;
}

std::size_t LineNode::InsertAtText(std::size_t textIndex, std::size_t offset,
                                   std::unique_ptr<EditorNode> node) {
    auto* text = AsText(children_.at(textIndex));
    const std::string& whole = text->GetText();
    offset = std::min(offset, whole.size());
    auto after = std::make_unique<TextNode>(whole.substr(offset));
    text->SetText(whole.substr(0, offset));
    node->Normalize();
    const auto at = children_.begin() + static_cast<std::ptrdiff_t>(textIndex) + 1;
    children_.insert(at, std::move(after));
    children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(textIndex) + 1,
                     std::move(node));
    return textIndex + 1;
}

CursorPath LineNode::RemoveAndJoin(std::size_t index) {
    RemoveChild(index);
    const std::size_t left = index > 0 ? index - 1 : 0;
    std::size_t offset = 0;
    if (auto* text = left < children_.size() ? AsText(children_[left]) : nullptr) {
        offset = index > 0 ? text->GetText().size() : 0;
    }
    Normalize();  // joins the two texts that are now next to each other
    return {left, offset};
}

std::unique_ptr<LineNode> LineNode::SplitAt(std::size_t textIndex, std::size_t offset) {
    auto* text = AsText(children_.at(textIndex));
    const std::string& whole = text->GetText();
    offset = std::min(offset, whole.size());
    std::vector<std::unique_ptr<EditorNode>> moved;
    moved.push_back(std::make_unique<TextNode>(whole.substr(offset)));
    text->SetText(whole.substr(0, offset));
    for (std::size_t i = textIndex + 1; i < children_.size(); ++i) {
        moved.push_back(std::move(children_[i]));
    }
    children_.resize(textIndex + 1);
    auto line = std::make_unique<LineNode>(std::move(moved));
    line->Normalize();
    return line;
}

void LineNode::Append(LineNode& other) {
    for (auto& child : other.children_) {
        children_.push_back(std::move(child));
    }
    other.children_.clear();
    Normalize();
}

bool LineNode::IsEmpty() const {
    for (const auto& child : children_) {
        const auto* text = AsText(child);
        if (!text || !text->GetText().empty()) {
            return false;
        }
    }
    return true;
}

// Children laid out left to right and centred vertically. Braces of a {...}
// group are only grouping in LaTeX, so they are not drawn. An empty line shows
// a grey box with its (empty) text inside, so the cursor has somewhere to go.
void LineNode::Render(const RenderContext& context) {
    if (IsEmpty()) {
        const float size = context.fontSize;
        CLAY(GetClayId(),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(std::round(size * 0.6f)),
                                    CLAY_SIZING_FIXED(std::round(size))},
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
              .border = {.color = node_style::placeholderColor, .width = CLAY_BORDER_OUTSIDE(1)}}) {
            RenderChildren(context);
        }
        return;
    }
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER},
                                  .layoutDirection = CLAY_LEFT_TO_RIGHT}}) {
        RenderChildren(context);
    }
}

// A child marked for deletion is drawn inside a red box.
void LineNode::RenderChildren(const RenderContext& context) {
    const DeleteHighlight& marked = context.deleteHighlight;
    for (const auto& child : children_) {
        if (marked.node == child.get() && marked.part == DeleteHighlight::Part::Whole) {
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}},
                          .backgroundColor = node_style::deleteFillColor,
                          .border = {.color = node_style::deleteColor,
                                     .width = CLAY_BORDER_OUTSIDE(1)}}) {
                child->Render(context);
            }
        } else {
            child->Render(context);
        }
    }
}

KeyResult LineNode::KeyPress(const KeyEvent& event) {
    if (event.path.empty() || event.path[0] >= children_.size()) {
        return {};
    }
    const std::size_t index = event.path[0];
    KeyResult result = children_[index]->KeyPress(event.Descend(1));

    switch (result.kind) {
    case KeyResult::Kind::Moved:
        return std::move(result).Prepend({index});
    case KeyResult::Kind::ExitRight:
        // Enter the next child that can hold the cursor; symbols are skipped,
        // which puts the cursor in the text just after them.
        for (std::size_t next = index + 1; next < children_.size(); ++next) {
            if (auto position = children_[next]->FirstPosition(); !position.empty()) {
                return KeyResult::Moved(JoinPath({next}, std::move(position)));
            }
        }
        return result;
    case KeyResult::Kind::ExitLeft:
        for (std::size_t previous = index; previous-- > 0;) {
            if (auto position = children_[previous]->LastPosition(); !position.empty()) {
                return KeyResult::Moved(JoinPath({previous}, std::move(position)));
            }
        }
        return result;
    case KeyResult::Kind::DeleteSelf:
        return KeyResult::WithPath(KeyResult::Kind::DeleteNode, {index});
    case KeyResult::Kind::DeleteLeft:
        if (index == 0) {
            return result;
        }
        if (!AsText(children_[index])) {
            // Backspace at the start of a {...} group just steps out of it.
            return KeyResult::Moved(JoinPath({index - 1}, children_[index - 1]->LastPosition()));
        }
        return KeyResult::WithPath(KeyResult::Kind::DeleteNode, {index - 1});
    case KeyResult::Kind::DeleteRight:
        if (index + 1 >= children_.size()) {
            return result;
        }
        if (!AsText(children_[index])) {
            return KeyResult::Moved(JoinPath({index + 1}, children_[index + 1]->FirstPosition()));
        }
        return KeyResult::WithPath(KeyResult::Kind::DeleteNode, {index + 1});
    default:
        // Up/Down are for the enclosing structure or row; paths get our index.
        return std::move(result).Prepend({index});
    }
}

void LineNode::MouseClick() {}

std::pair<float, float> LineNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float, float> LineNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string LineNode::ToText() const {
    std::string text;
    if (braced_) {
        text += '{';
    }
    for (const auto& child : children_) {
        text += child->ToText();
    }
    if (braced_) {
        text += '}';
    }
    return text;
}

bool LineNode::HasChild() {
    return !children_.empty();
}

CursorPath LineNode::FirstPosition() const {
    for (std::size_t i = 0; i < children_.size(); ++i) {
        if (auto position = children_[i]->FirstPosition(); !position.empty()) {
            return JoinPath({i}, std::move(position));
        }
    }
    return {};
}

CursorPath LineNode::LastPosition() const {
    for (std::size_t i = children_.size(); i-- > 0;) {
        if (auto position = children_[i]->LastPosition(); !position.empty()) {
            return JoinPath({i}, std::move(position));
        }
    }
    return {};
}

// Pick the child drawn nearest to x. A symbol the cursor can't enter resolves
// to the text on whichever side of it x is closer to.
CursorPath LineNode::PositionNearX(float x, VerticalEntry entry,
                                   const TextMeasure* measure) const {
    std::size_t best = children_.size();
    float bestDistance = std::numeric_limits<float>::max();
    float bestCentre = 0.0f;
    for (std::size_t i = 0; i < children_.size(); ++i) {
        const auto bounds = LastLayoutBounds(children_[i]->GetClayId());
        if (!bounds) {
            continue;
        }
        const float left = bounds->x;
        const float right = bounds->x + bounds->width;
        const float distance = x < left ? left - x : (x > right ? x - right : 0.0f);
        // Prefer text on ties so a boundary between text and a structure stays
        // in the text rather than jumping into the structure.
        if (distance < bestDistance || (distance == bestDistance && AsText(children_[i]))) {
            best = i;
            bestDistance = distance;
            bestCentre = left + bounds->width / 2;
        }
    }
    if (best == children_.size()) {
        return FirstPosition();
    }

    if (auto position = children_[best]->PositionNearX(x, entry, measure); !position.empty()) {
        return JoinPath({best}, std::move(position));
    }
    // Can't enter it: land just before or just after it.
    if (x < bestCentre && best > 0) {
        return JoinPath({best - 1}, children_[best - 1]->LastPosition());
    }
    if (best + 1 < children_.size()) {
        return JoinPath({best + 1}, children_[best + 1]->FirstPosition());
    }
    return LastPosition();
}

CursorPath LineNode::EraseRange(std::size_t startIndex, std::size_t startOffset,
                                std::size_t endIndex, std::size_t endOffset) {
    auto* first = AsText(children_.at(startIndex));
    auto* last = AsText(children_.at(endIndex));
    std::string joined = first->GetText().substr(0, std::min(startOffset, first->GetText().size()));
    joined += last->GetText().substr(std::min(endOffset, last->GetText().size()));
    first->SetText(std::move(joined));
    if (endIndex > startIndex) {
        children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(startIndex) + 1,
                        children_.begin() + static_cast<std::ptrdiff_t>(endIndex) + 1);
    }
    return {startIndex, startOffset};
}

EditorNode* LineNode::PathChild(PathView path, std::size_t& used) const {
    if (path.empty() || path[0] >= children_.size()) {
        return nullptr;
    }
    used = 1;
    return children_[path[0]].get();
}

void LineNode::Normalize() {
    std::vector<std::unique_ptr<EditorNode>> normalized;
    normalized.reserve(children_.size() * 2 + 1);
    for (auto& child : children_) {
        if (!child) {
            continue;
        }
        child->Normalize();
        if (auto* text = AsText(child)) {
            if (!normalized.empty() && AsText(normalized.back())) {
                AsText(normalized.back())->AppendText(text->GetText());
            } else {
                normalized.push_back(std::move(child));
            }
            continue;
        }
        if (normalized.empty() || !AsText(normalized.back())) {
            normalized.push_back(std::make_unique<TextNode>());
        }
        normalized.push_back(std::move(child));
    }
    if (normalized.empty() || !AsText(normalized.back())) {
        normalized.push_back(std::make_unique<TextNode>());
    }
    children_ = std::move(normalized);
}

std::unique_ptr<EditorNode> LineNode::Clone() const {
    std::vector<std::unique_ptr<EditorNode>> children;
    children.reserve(children_.size());
    for (const auto& child : children_) {
        children.push_back(CloneOf(child));
    }
    auto copy = std::make_unique<LineNode>(std::move(children), braced_);
    copy->spacing_ = spacing_;
    copy->CopyIdFrom(*this);
    return copy;
}
