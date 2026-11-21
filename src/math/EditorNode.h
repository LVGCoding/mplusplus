#pragma once

#include "clay.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class EditorNode;

// Position of the cursor in the node tree: indices from the document root down
// to a TextNode, ending with a byte offset on a UTF-8 character boundary. Each node
// type consumes its own indices:
//   LineNode   [child]           FracNode   [0 numerator | 1 denominator]
//   SqrtNode   [0 index | 1 argument]       MatrixNode [row, column]
//   TextNode   [byte offset]     GreekNode  (never holds the cursor)
using CursorPath = std::vector<std::size_t>;
using PathView = std::span<const std::size_t>;

// Width in pixels of `text` drawn at `fontSize`. Supplied by the editor so nodes
// can place the cursor without knowing about fonts.
using TextMeasure = std::function<float(std::string_view text, float fontSize)>;

// Which side the cursor comes from when it moves vertically into a node.
enum class VerticalEntry { FROM_ABOVE, FROM_BELOW };

// What is marked in red, waiting for a second Backspace to delete it.
struct DeleteHighlight {
    enum class Part { WHOLE, ROW, COLUMN };

    // The node itself (WHOLE), or the matrix whose row/column is marked.
    const EditorNode* node = nullptr;
    Part part = Part::WHOLE;
    std::size_t index = 0;  // row or column for ROW/COLUMN
};

struct RenderContext {
    float fontSize = 18.0f;

    // The innermost structure (fraction, root, matrix) that holds the cursor.
    // It outlines its slots so the user can see where they can navigate.
    const EditorNode* activeStructure = nullptr;

    DeleteHighlight deleteHighlight;

    // For a subscript or exponent: how tall the thing it attaches to (the
    // node before it in the line) was last frame, or 0 if unknown. A script
    // after a tall base, such as a fraction, stretches to reach its corner.
    float baseHeight = 0.0f;

    // For a TextNode: the offset where a comment (%) starts in it, set by
    // its line; npos when it has none. The comment part is drawn in grey.
    std::size_t commentFrom = static_cast<std::size_t>(-1);

    // Syntax colors of the document being drawn, or null (see Syntax.h).
    const struct SyntaxColors* syntax = nullptr;

    // The height of a script's box: as tall as its base, plus room to stick
    // out above or below it. Lines centre their children, so a script drawn
    // at the top (or bottom) of this box sits at the base's corner.
    float ScriptBoxHeight() const {
        return std::max(fontSize, baseHeight) + 0.45f * fontSize;
    }

    RenderContext WithFontSize(float size) const {
        RenderContext context = *this;
        context.fontSize = size;
        return context;
    }
};

struct KeyEvent {
    int key = 0;  // raylib KEY_*, or 0 for a typed character

    // The typed character when key == 0.
    char32_t codepoint = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;

    // Rest of the cursor path below the node receiving the event.
    PathView path;

    // Screen x the cursor should stay near when moving up or down.
    float cursorX = 0.0f;

    // Null when no fonts are available (e.g. in tests).
    const TextMeasure* measure = nullptr;

    // The same event for a child, with the first `count` indices consumed.
    KeyEvent Descend(std::size_t count) const {
        KeyEvent event = *this;
        event.path = count <= path.size() ? path.subspan(count) : PathView{};
        return event;
    }
};

// What a KeyEvent did, or what the parent should do about it.
//   MOVED         - `path` is the new cursor position, relative to the node that
//                  returned it. `changed` is set if the content was edited.
//   EXIT_*        - the cursor would leave this node in that direction; the
//                  parent decides where it goes.
//   DELETE_LEFT   - Backspace at the very start of this node.
//   DELETE_RIGHT  - Delete at the very end of this node.
//   DELETE_SELF   - this node asks its parent to delete it (e.g. Backspace at the
//                  start of a fraction's numerator).
//   DELETE_NODE   - `path` leads to a node that should be deleted.
//   MATRIX_LINE   - `path` leads to a matrix cell [.., row, column]; Backspace at
//                  its start asks to delete that row (or column).
//   MATRIX_INSERT - `path` leads to a matrix cell; Enter asks to insert a row or
//                  column next to it.
//   IGNORED       - the key does nothing here.
struct KeyResult {
    enum class Kind {
        IGNORED,
        MOVED,
        EXIT_LEFT,
        EXIT_RIGHT,
        EXIT_UP,
        EXIT_DOWN,
        DELETE_LEFT,
        DELETE_RIGHT,
        DELETE_SELF,
        DELETE_NODE,
        MATRIX_LINE,
        MATRIX_INSERT,
    };

    Kind kind = Kind::IGNORED;
    CursorPath path;
    bool changed = false;

    static KeyResult Moved(CursorPath path,
                           bool changed = false) {
        return {Kind::MOVED, std::move(path), changed};
    }
    static KeyResult Exit(Kind kind) {
        return {kind, {}, false};
    }
    static KeyResult WithPath(Kind kind,
                              CursorPath path) {
        return {kind, std::move(path), false};
    }

    // Prefix this node's own indices onto a child's result path.
    KeyResult Prepend(std::initializer_list<std::size_t> indices) && {
        if (kind == Kind::MOVED || kind == Kind::DELETE_NODE || kind == Kind::MATRIX_LINE ||
            kind == Kind::MATRIX_INSERT) {
            path.insert(path.begin(), indices);
        }
        return std::move(*this);
    }
};

// `prefix` followed by `rest`; an empty `rest` (no valid position) stays empty.
inline CursorPath JoinPath(std::initializer_list<std::size_t> prefix,
                           CursorPath rest) {
    if (rest.empty()) {
        return {};
    }
    rest.insert(rest.begin(), prefix);
    return rest;
}

// Bounding box of an element from the most recent Clay layout, if there is one.
inline std::optional<Clay_BoundingBox> LastLayoutBounds(Clay_ElementId id) {
    if (!Clay_GetCurrentContext()) {
        return std::nullopt;
    }
    const Clay_ElementData data = Clay_GetElementData(id);
    if (!data.found) {
        return std::nullopt;
    }
    return data.boundingBox;
}

class EditorNode {
public:
    EditorNode() : id_(GenerateId()) {}
    virtual ~EditorNode() = default;

    virtual void Render(const RenderContext& context) = 0;

    // A deep copy that keeps every node's id, so it lays out where the
    // original did (Clay looks up last frame's boxes by id). Used by undo.
    virtual std::unique_ptr<EditorNode> Clone() const = 0;

    // Handle a key with the cursor inside this node. `event.path` is the cursor
    // path relative to this node; containers forward it to the child it points
    // into and then interpret the child's result.
    virtual KeyResult KeyPress(const KeyEvent& /*event*/) {
        return {};
    }

    virtual void MouseClick() = 0;

    // Serialize this node (and its children) back to M++/LaTeX source.
    virtual std::string ToText() const = 0;

    // Legacy geometry hooks; current cursor placement uses LastLayoutBounds() instead.
    virtual std::pair<float,
                      float>
    GetNWCorner() = 0;
    virtual std::pair<float,
                      float>
    GetSize() = 0;
    virtual bool HasChild() = 0;

    // --- Cursor ---------------------------------------------------------------

    // Cursor positions at the start and end of this node, relative to it.
    // Empty for nodes the cursor can't enter (e.g. GreekNode): it steps over them.
    virtual CursorPath FirstPosition() const {
        return {};
    }
    virtual CursorPath LastPosition() const {
        return {};
    }

    // The position closest to screen x when the cursor moves into this node
    // vertically, using the last frame's layout. Falls back to FirstPosition().
    virtual CursorPath PositionNearX(float /*x*/,
                                     VerticalEntry /*entry*/,
                                     const TextMeasure* /*measure*/) const {
        return FirstPosition();
    }

    // The child that `path` points into and how many indices that consumed, or
    // nullptr if the path ends here or is invalid.
    virtual EditorNode* PathChild(PathView /*path*/,
                                  std::size_t& /*used*/) const {
        return nullptr;
    }

    // Every child with the path indices that lead to it from this node (e.g.
    // {row, column} for a matrix cell). Used to search the tree, such as
    // finding a node by id or the slot nearest a mouse click.
    struct ChildPath {
        CursorPath path;
        EditorNode* node = nullptr;
    };
    virtual std::vector<ChildPath> ChildPaths() const {
        return {};
    }

    // Run a button drawn inside this node (e.g. a table's "+" for a new row).
    // node_style::ActionButton records the click while rendering; the UI then
    // runs it here through FileMpp::EditNode, as one undo step. Returns true
    // if the node changed.
    virtual bool RunAction(int /*action*/) {
        return false;
    }

    // True for structures whose slots are outlined while the cursor is in them.
    virtual bool OutlinesSlots() const {
        return false;
    }

    // Restore the tree invariants the cursor relies on (see LineNode::Normalize).
    virtual void Normalize() {}

    // --- Identity ---------------------------------------------------------------

    // Random id assigned at construction; stable for the node's lifetime.
    std::uint32_t GetId() const {
        return id_;
    }

    // Clay element id for this node, consistent across frames so Clay can
    // track the element (hover, scroll, element data) between layouts.
    Clay_ElementId GetClayId() const {
        return CLAY_IDI("EditorNode", id_);
    }

protected:
    void CopyIdFrom(const EditorNode& other) {
        id_ = other.id_;
    }

    // Clone() of a child that may be null.
    static std::unique_ptr<EditorNode> CloneOf(const std::unique_ptr<EditorNode>& node) {
        return node ? node->Clone() : nullptr;
    }

private:
    static std::uint32_t GenerateId() {
        static std::mt19937 randomGenerator{std::random_device{}()};
        return randomGenerator();
    }

    std::uint32_t id_;
};
