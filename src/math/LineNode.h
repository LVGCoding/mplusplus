#pragma once

#include "EditorNode.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// An ordered list of nodes. Used for document rows, the contents of command
// arguments (\frac{...}{...}, \sqrt{...}), matrix cells, and bare {...} groups.
//
// After Normalize(), children alternate between TextNodes and other nodes,
// starting and ending with a (possibly empty) TextNode, and no two TextNodes
// are adjacent. So every place the cursor can be is a TextNode offset, and each
// on-screen position has exactly one cursor path.
class LineNode : public EditorNode {
private:
    std::vector<std::unique_ptr<EditorNode>> children_;

    // True for a bare {...} group in the source, so ToText() restores the braces.
    bool braced_ = false;

    float spacing_ = 0.0f;

public:
    LineNode() = default;

    explicit LineNode(std::vector<std::unique_ptr<EditorNode>> children, bool braced = false);

    // `node` as a LineNode: itself if it already is one, otherwise wrapped in a
    // new LineNode (an empty one for null).
    static std::unique_ptr<EditorNode> EnsureLine(std::unique_ptr<EditorNode> node);

    void AddChild(std::unique_ptr<EditorNode> child);

    void InsertChild(std::size_t index, std::unique_ptr<EditorNode> child);

    void RemoveChild(std::size_t index);

    EditorNode* GetChild(std::size_t index) const;

    std::size_t ChildCount() const;

    bool IsBraced() const;

    // True if the line holds nothing but empty text.
    bool IsEmpty() const;

    // --- Editing (keep the Normalize() invariant) --------------------------------

    // Insert `node` at byte `offset` in the TextNode at `textIndex`, splitting
    // that text around it. Returns the new node's child index.
    std::size_t InsertAtText(std::size_t textIndex, std::size_t offset,
                             std::unique_ptr<EditorNode> node);

    // Remove the non-text child at `index` and join the texts on either side.
    // Returns the cursor position where it was, relative to this line.
    CursorPath RemoveAndJoin(std::size_t index);

    // Move everything after byte `offset` of the TextNode at `textIndex` into a
    // new line, which is returned.
    std::unique_ptr<LineNode> SplitAt(std::size_t textIndex, std::size_t offset);

    // Move all of `other`'s children onto the end of this line.
    void Append(LineNode& other);

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    // Render the children without this line's own element or placeholder.
    // Used for document rows, which the file wraps in its own row element.
    void RenderChildren(const RenderContext& context);

    // Forwards to the child under the cursor. When the cursor leaves a child
    // sideways it moves into the neighbouring structure (or steps over a symbol
    // like \alpha); at either end of the line the exit is passed up.
    // Backspace/Delete next to a non-text child ask for that child to be deleted.
    KeyResult KeyPress(const KeyEvent& event) override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float, float> GetNWCorner() override;

    std::pair<float, float> GetSize() override;

    bool HasChild() override;

    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x, VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path, std::size_t& used) const override;
    void Normalize() override;

    // --- Selection ------------------------------------------------------------------

    // Remove everything between two cursor positions in this line: byte
    // `startOffset` of the TextNode at `startIndex` up to byte `endOffset` of
    // the TextNode at `endIndex` (start must come first). The texts at either
    // end are joined. Returns the cursor position where the range was.
    CursorPath EraseRange(std::size_t startIndex, std::size_t startOffset, std::size_t endIndex,
                          std::size_t endOffset);

};
