#include "GraphNode.h"

#include "LineNode.h"
#include "NodeStyle.h"
#include "Options.h"
#include "raylib.h"
#include "TextNode.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr Clay_Color PLOT_COLOR = {255, 255, 255, 255};
constexpr Clay_Color PLOT_BORDER = {215, 215, 215, 255};

// A normalized line holding `text` (or nothing).
std::unique_ptr<EditorNode> Line(const std::string& text = {}) {
    auto line = std::make_unique<LineNode>();
    if (!text.empty()) {
        line->AddChild(std::make_unique<TextNode>(text));
    }
    line->Normalize();
    return line;
}

// `node` as a normalized line (an empty one for null).
std::unique_ptr<EditorNode> AsLine(std::unique_ptr<EditorNode> node) {
    auto line = LineNode::EnsureLine(std::move(node));
    line->Normalize();
    return line;
}

std::string TextOf(const EditorNode* node) {
    return node ? node->ToText() : std::string();
}
}  // namespace

GraphNode::GraphNode() : title_(Line()), xLabel_(Line("x")), yLabel_(Line("y")) {
    functions_.push_back(Line());
}

GraphNode::GraphNode(GraphSettings settings,
                     std::unique_ptr<EditorNode> title,
                     std::unique_ptr<EditorNode> xLabel,
                     std::unique_ptr<EditorNode> yLabel,
                     std::vector<std::unique_ptr<EditorNode>> functions)
    : settings_(std::move(settings)), title_(AsLine(std::move(title))),
      xLabel_(AsLine(std::move(xLabel))), yLabel_(AsLine(std::move(yLabel))),
      functions_(std::move(functions)) {
    if (functions_.empty()) {
        functions_.push_back(Line());
    }
}

Clay_Color GraphNode::FunctionColor(std::size_t index) {
    // matplotlib's default colour cycle, so the swatches match the plots later.
    static constexpr Clay_Color COLORS[] = {
        {31, 119, 180, 255},
        {255, 127, 14, 255},
        {44, 160, 44, 255},
        {214, 39, 40, 255},
        {148, 103, 189, 255},
        {140, 86, 75, 255},
        {227, 119, 194, 255},
        {127, 127, 127, 255},
        {188, 189, 34, 255},
        {23, 190, 207, 255},
    };
    return COLORS[index % std::size(COLORS)];
}

void GraphNode::RenderCompact(const RenderContext& context) {
    // A small framed "f(x)" icon.
    const float size = context.fontSize;
    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIXED(std::round(size * 2.2f)),
                                CLAY_SIZING_FIXED(std::round(size * 1.4f))},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = PLOT_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(3),
          .border = {.color = node_style::TEXT_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        node_style::Text("f(x)", size * 0.7f);
    }
}

void GraphNode::Render(const RenderContext& context) {
    if (compact_) {
        RenderCompact(context);
        return;
    }
    const float fontSize = context.fontSize;
    const bool outlined = context.activeStructure == this;
    auto slot = [&](const std::unique_ptr<EditorNode>& node) {
        node_style::Slot(outlined, [&] { node->Render(context); });
    };

    CLAY(GetClayId(),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(8),
                     .childGap = 6,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = node_style::CARD_COLOR,
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = node_style::CARD_BORDER_COLOR, .width = CLAY_BORDER_OUTSIDE(1)}}) {
        // Title, and the Settings button (axis ranges) on the right.
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 8,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
            slot(title_);
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
            node_style::SettingsButton(GetId(), fontSize);
        }

        // The y label left of the plot area, the x label under it.
        CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                 .childGap = 6,
                                 .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
            slot(yLabel_);
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                     .childGap = 4,
                                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER},
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIXED(PLOT_WIDTH),
                                                    CLAY_SIZING_FIXED(PLOT_HEIGHT)}},
                              .backgroundColor = PLOT_COLOR,
                              .border = {.color = PLOT_BORDER, .width = CLAY_BORDER_OUTSIDE(1)}}) {}
                slot(xLabel_);
            }
        }

        // The functions: a colour swatch and an editable slot each.
        for (std::size_t i = 0; i < functions_.size(); ++i) {
            CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                     .childGap = 6,
                                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
                const float swatch = std::round(fontSize * 0.55f);
                CLAY_AUTO_ID(
                    {.layout = {.sizing = {CLAY_SIZING_FIXED(swatch), CLAY_SIZING_FIXED(swatch)}},
                     .backgroundColor = FunctionColor(i),
                     .cornerRadius = CLAY_CORNER_RADIUS(2)}) {}
                slot(functions_[i]);
            }
        }
    }
}

std::unique_ptr<EditorNode> GraphNode::Clone() const {
    std::vector<std::unique_ptr<EditorNode>> functions;
    for (const auto& function : functions_) {
        functions.push_back(CloneOf(function));
    }
    auto copy = std::make_unique<GraphNode>(
        settings_, CloneOf(title_), CloneOf(xLabel_), CloneOf(yLabel_), std::move(functions));
    copy->compact_ = compact_;
    copy->CopyIdFrom(*this);
    return copy;
}

void GraphNode::MouseClick() {}

std::string GraphNode::ToText() const {
    std::vector<std::string> items;
    if (const std::string title = TextOf(title_.get()); !title.empty()) {
        items.push_back(options::Item("title", title));
    }
    // Labels are written when they differ from the defaults; an empty label
    // is a bare key ("xlabel"), so it reads back as removed.
    if (const std::string label = TextOf(xLabel_.get()); label != "x") {
        items.push_back(options::Item("xlabel", label));
    }
    if (const std::string label = TextOf(yLabel_.get()); label != "y") {
        items.push_back(options::Item("ylabel", label));
    }
    for (std::string& item : settings_.OptionItems()) {
        items.push_back(std::move(item));
    }

    std::string body;
    for (std::size_t i = 0; i < functions_.size(); ++i) {
        if (i > 0) {
            body += "\\\\";
        }
        body += TextOf(functions_[i].get());
    }
    return options::Environment(ENVIRONMENT, options::Join(items), body);
}

std::pair<float,
          float>
GraphNode::GetNWCorner() {
    return {0.0f, 0.0f};
}

std::pair<float,
          float>
GraphNode::GetSize() {
    return {0.0f, 0.0f};
}

bool GraphNode::HasChild() {
    return true;
}

EditorNode* GraphNode::Slot(std::size_t slot) const {
    switch (slot) {
    case TITLE_SLOT:
        return title_.get();
    case Y_LABEL_SLOT:
        return yLabel_.get();
    case X_LABEL_SLOT:
        return xLabel_.get();
    default:
        return GetFunction(slot - FIRST_FUNCTION_SLOT);
    }
}

bool GraphNode::IsEmptySlot(std::size_t slot) const {
    const auto* line = dynamic_cast<const LineNode*>(Slot(slot));
    return line && line->IsEmpty();
}

KeyResult GraphNode::KeyPress(const KeyEvent& event) {
    std::size_t used = 0;
    EditorNode* child = PathChild(event.path, used);
    if (!child) {
        return {};
    }
    if (event.codepoint == U'&') {
        return {};  // reserved in the environment's source
    }
    const std::size_t slot = event.path[0];
    const std::size_t count = SlotCount();
    const bool function = slot >= FIRST_FUNCTION_SLOT;
    KeyResult result = child->KeyPress(event.Descend(used));

    auto moveTo = [&](std::size_t target, CursorPath position, bool changed = false) {
        return KeyResult::Moved(JoinPath({target}, std::move(position)), changed);
    };
    auto nearX = [&](std::size_t target, VerticalEntry entry) {
        return Slot(target)->PositionNearX(event.cursorX, entry, event.measure);
    };

    switch (result.kind) {
    case KeyResult::Kind::MOVED:
        return std::move(result).Prepend({slot});
    case KeyResult::Kind::IGNORED:
        // Enter in a function adds one below; elsewhere it does nothing.
        if (event.key == KEY_ENTER && function) {
            InsertFunction(slot - FIRST_FUNCTION_SLOT + 1);
            return moveTo(slot + 1, Slot(slot + 1)->FirstPosition(), true);
        }
        return result;
    case KeyResult::Kind::EXIT_UP:
        if (slot > 0) {
            return moveTo(slot - 1, nearX(slot - 1, VerticalEntry::FROM_BELOW));
        }
        return result;
    case KeyResult::Kind::EXIT_DOWN:
        if (slot + 1 < count) {
            return moveTo(slot + 1, nearX(slot + 1, VerticalEntry::FROM_ABOVE));
        }
        return result;
    case KeyResult::Kind::EXIT_LEFT:
        if (slot > 0) {
            return moveTo(slot - 1, Slot(slot - 1)->LastPosition());
        }
        return result;
    case KeyResult::Kind::EXIT_RIGHT:
        if (slot + 1 < count) {
            return moveTo(slot + 1, Slot(slot + 1)->FirstPosition());
        }
        return result;
    case KeyResult::Kind::DELETE_LEFT:
        // Backspace at the start of a slot.
        if (function && IsEmptySlot(slot) && functions_.size() > 1) {
            RemoveFunction(slot - FIRST_FUNCTION_SLOT);
            return moveTo(slot - 1, Slot(slot - 1)->LastPosition(), true);
        }
        if (slot > 0) {
            return moveTo(slot - 1, Slot(slot - 1)->LastPosition());
        }
        return KeyResult::Exit(KeyResult::Kind::DELETE_SELF);
    case KeyResult::Kind::DELETE_RIGHT:
        // Delete at the end of a function pulls out an empty one below it.
        if (function && slot + 1 < count && IsEmptySlot(slot + 1)) {
            RemoveFunction(slot + 1 - FIRST_FUNCTION_SLOT);
            return moveTo(slot, Slot(slot)->LastPosition(), true);
        }
        return {};
    default:
        return std::move(result).Prepend({slot});
    }
}

CursorPath GraphNode::FirstPosition() const {
    return JoinPath({TITLE_SLOT}, title_->FirstPosition());
}

CursorPath GraphNode::LastPosition() const {
    const std::size_t last = SlotCount() - 1;
    return JoinPath({last}, Slot(last)->LastPosition());
}

CursorPath GraphNode::PositionNearX(float x,
                                    VerticalEntry entry,
                                    const TextMeasure* measure) const {
    const std::size_t slot = entry == VerticalEntry::FROM_BELOW ? SlotCount() - 1 : TITLE_SLOT;
    return JoinPath({slot}, Slot(slot)->PositionNearX(x, entry, measure));
}

EditorNode* GraphNode::PathChild(PathView path,
                                 std::size_t& used) const {
    if (path.empty() || path[0] >= SlotCount()) {
        return nullptr;
    }
    used = 1;
    return Slot(path[0]);
}

std::vector<EditorNode::ChildPath> GraphNode::ChildPaths() const {
    std::vector<ChildPath> children;
    for (std::size_t slot = 0; slot < SlotCount(); ++slot) {
        children.push_back({{slot}, Slot(slot)});
    }
    return children;
}

void GraphNode::Normalize() {
    title_ = AsLine(std::move(title_));
    xLabel_ = AsLine(std::move(xLabel_));
    yLabel_ = AsLine(std::move(yLabel_));
    if (functions_.empty()) {
        functions_.push_back(Line());
    }
    for (auto& function : functions_) {
        function = AsLine(std::move(function));
    }
}

void GraphNode::SetSettings(GraphSettings settings) {
    settings_ = std::move(settings);
}

EditorNode* GraphNode::GetFunction(std::size_t index) const {
    return index < functions_.size() ? functions_[index].get() : nullptr;
}

void GraphNode::InsertFunction(std::size_t index) {
    index = std::min(index, functions_.size());
    functions_.insert(functions_.begin() + static_cast<std::ptrdiff_t>(index), Line());
}

bool GraphNode::RemoveFunction(std::size_t index) {
    if (functions_.size() <= 1 || index >= functions_.size()) {
        return false;
    }
    functions_.erase(functions_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}
