#pragma once

#include "EditorNode.h"
#include "GraphSettings.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// A graph of one or more functions:
//   \begin{graph}[title=Growth, xlabel=t, xmax=5] y=x^2 \\ y=2x \end{graph}
// Everything you type is edited in place, like matrix cells: the title, the
// two axis labels, and the functions (Enter adds a function below, Backspace
// in an empty one removes it). The axis ranges and scales are in the settings
// menu. The plot itself is drawn in a Graph tab when the document runs
// (src/editor/GraphTab.h); here the plot area is an empty frame.
//
// Cursor path inside the graph: [slot, ...path in that slot's LineNode], with
// slots in Up/Down order: TITLE_SLOT, Y_LABEL_SLOT, X_LABEL_SLOT, then the
// functions from FIRST_FUNCTION_SLOT.
class GraphNode : public EditorNode {
public:
    static constexpr const char* ENVIRONMENT = "graph";
    static constexpr std::size_t TITLE_SLOT = 0;
    static constexpr std::size_t Y_LABEL_SLOT = 1;
    static constexpr std::size_t X_LABEL_SLOT = 2;
    static constexpr std::size_t FIRST_FUNCTION_SLOT = 3;

    // Size of the plot area in the document, in pixels.
    static constexpr float PLOT_WIDTH = 420;
    static constexpr float PLOT_HEIGHT = 240;

    // Default ranges, no title, axis labels "x" and "y", one empty function.
    GraphNode();

    // Null title/labels become empty; no functions becomes one empty function.
    GraphNode(GraphSettings settings,
              std::unique_ptr<EditorNode> title,
              std::unique_ptr<EditorNode> xLabel,
              std::unique_ptr<EditorNode> yLabel,
              std::vector<std::unique_ptr<EditorNode>> functions);

    ~GraphNode() override = default;

    void Render(const RenderContext& context) override;
    std::unique_ptr<EditorNode> Clone() const override;

    void MouseClick() override;

    std::string ToText() const override;

    std::pair<float,
              float>
    GetNWCorner() override;

    std::pair<float,
              float>
    GetSize() override;

    bool HasChild() override;

    // Up/Down and Left/Right at a slot's edge move to the neighbouring slot;
    // past the first or last the exit is passed up. Enter in a function adds
    // one below. Backspace at the start of an empty function removes it; at
    // the start of the title it asks for the whole graph to be deleted.
    KeyResult KeyPress(const KeyEvent& event) override;
    CursorPath FirstPosition() const override;
    CursorPath LastPosition() const override;
    CursorPath PositionNearX(float x,
                             VerticalEntry entry,
                             const TextMeasure* measure) const override;
    EditorNode* PathChild(PathView path,
                          std::size_t& used) const override;
    std::vector<ChildPath> ChildPaths() const override;
    bool OutlinesSlots() const override {
        return true;
    }
    void Normalize() override;

    const GraphSettings& Settings() const {
        return settings_;
    }
    void SetSettings(GraphSettings settings);

    EditorNode* Title() const {
        return title_.get();
    }
    EditorNode* XLabel() const {
        return xLabel_.get();
    }
    EditorNode* YLabel() const {
        return yLabel_.get();
    }

    std::size_t FunctionCount() const {
        return functions_.size();
    }
    EditorNode* GetFunction(std::size_t index) const;

    // Insert an empty function before `index` (0..FunctionCount()).
    void InsertFunction(std::size_t index);
    // Remove a function. Refused (returns false) for the last remaining one.
    bool RemoveFunction(std::size_t index);

    // Draw as a small icon instead of the full card (the math menu preview).
    void SetCompact(bool compact) {
        compact_ = compact;
    }

    // Colour of a function's line, used for its swatch (and later its plot).
    static Clay_Color FunctionColor(std::size_t index);

private:
    GraphSettings settings_;
    std::unique_ptr<EditorNode> title_;
    std::unique_ptr<EditorNode> xLabel_;
    std::unique_ptr<EditorNode> yLabel_;
    std::vector<std::unique_ptr<EditorNode>> functions_;
    bool compact_ = false;

    std::size_t SlotCount() const {
        return FIRST_FUNCTION_SLOT + functions_.size();
    }
    EditorNode* Slot(std::size_t slot) const;
    bool IsEmptySlot(std::size_t slot) const;
    void RenderCompact(const RenderContext& context);
};
