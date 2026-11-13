#include "TextNode.h"

#include "NodeStyle.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
bool IsContinuationByte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}
}  // namespace

TextNode::TextNode(std::string text) : text_(std::move(text)) {}

// Wrapped in its own element so the cursor can find where the text was drawn.
// Empty text still gets a line-tall element for the cursor to sit in.
void TextNode::Render(const RenderContext& context) {
    fontSize_ = context.fontSize;
    CLAY(GetClayId(), {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(context.fontSize)},
                                  .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        node_style::Text(text_, context.fontSize);
    }
}

KeyResult TextNode::KeyPress(const KeyEvent& event) {
    const std::size_t offset = event.path.empty() ? 0 : std::min(event.path[0], text_.size());
    switch (event.key) {
    case KEY_LEFT:
        if (offset == 0) {
            return KeyResult::Exit(KeyResult::Kind::ExitLeft);
        }
        return KeyResult::Moved({PreviousCharacter(offset)});
    case KEY_RIGHT:
        if (offset == text_.size()) {
            return KeyResult::Exit(KeyResult::Kind::ExitRight);
        }
        return KeyResult::Moved({NextCharacter(offset)});
    case KEY_UP:
        return KeyResult::Exit(KeyResult::Kind::ExitUp);
    case KEY_DOWN:
        return KeyResult::Exit(KeyResult::Kind::ExitDown);
    default:
        return {};
    }
}

void TextNode::MouseClick() {}

std::pair<float, float> TextNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float, float> TextNode::GetSize() {
    return {0.0f, 0.0f};
}

std::string TextNode::ToText() const {
    return text_;
}

bool TextNode::HasChild() {
    return false;
}

CursorPath TextNode::FirstPosition() const {
    return {0};
}

CursorPath TextNode::LastPosition() const {
    return {text_.size()};
}

CursorPath TextNode::PositionNearX(float x, VerticalEntry /*entry*/,
                                   const TextMeasure* measure) const {
    const auto bounds = LastLayoutBounds(GetClayId());
    if (!bounds || !measure) {
        return FirstPosition();
    }
    std::size_t best = 0;
    float bestDistance = std::abs(bounds->x - x);
    for (std::size_t offset = NextCharacter(0); offset <= text_.size();
         offset = NextCharacter(offset)) {
        const float caretX =
            bounds->x + (*measure)(std::string_view(text_).substr(0, offset), fontSize_);
        const float distance = std::abs(caretX - x);
        if (distance < bestDistance) {
            best = offset;
            bestDistance = distance;
        }
        if (offset == text_.size()) {
            break;
        }
    }
    return {best};
}

std::optional<Clay_BoundingBox> TextNode::CaretBox(std::size_t offset,
                                                   const TextMeasure* measure) const {
    auto bounds = LastLayoutBounds(GetClayId());
    if (!bounds) {
        return std::nullopt;
    }
    offset = std::min(offset, text_.size());
    if (measure && offset > 0) {
        bounds->x += (*measure)(std::string_view(text_).substr(0, offset), fontSize_);
    }
    bounds->width = 0;
    return bounds;
}

const std::string& TextNode::GetText() const {
    return text_;
}

void TextNode::SetText(std::string text) {
    text_ = std::move(text);
}

void TextNode::AppendText(const std::string& text) {
    text_ += text;
}

std::size_t TextNode::PreviousCharacter(std::size_t offset) const {
    offset = std::min(offset, text_.size());
    if (offset == 0) {
        return 0;
    }
    --offset;
    while (offset > 0 && IsContinuationByte(text_[offset])) {
        --offset;
    }
    return offset;
}

std::size_t TextNode::NextCharacter(std::size_t offset) const {
    if (offset >= text_.size()) {
        return text_.size();
    }
    ++offset;
    while (offset < text_.size() && IsContinuationByte(text_[offset])) {
        ++offset;
    }
    return offset;
}
