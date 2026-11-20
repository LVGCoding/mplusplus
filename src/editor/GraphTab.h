#pragma once

// A Graph tab: a plot a run sent back (a graph in the document, or a Figure
// made by the program). Re-running the document refreshes it in place.

#include "fileBase.h"
#include "run/Results.h"

#include <optional>
#include <string>

class GraphTab : public Tab {
public:
    // `source` is the document whose run made the plot.
    GraphTab(run::PlotData plot, const Tab* source);

    const std::string& PlotId() const {
        return plot_.id;
    }
    const Tab* Source() const {
        return source_;
    }
    const run::PlotData& Plot() const {
        return plot_;
    }

    // New data from another run; the axis scales the user chose are kept.
    void SetPlot(run::PlotData plot);

    const std::string& TabName() const override;
    bool HandleInput(const EditorView& view) override;
    void LayoutContent() const override;
    std::size_t LineCount() const override {
        return 0;
    }
    void DrawOverlay(const EditorView& view) const override;
    std::optional<Rectangle> CursorBounds(const EditorView& /*view*/) const override {
        return std::nullopt;
    }

    // Whether an axis is drawn with a log scale (the user can switch it).
    bool LogX() const {
        return logX_;
    }
    bool LogY() const {
        return logY_;
    }

private:
    run::PlotData plot_;
    const Tab* source_ = nullptr;
    // Distinguishes this tab's Clay elements from another graph's.
    std::uint32_t index_ = 0;
    bool logX_ = false;
    bool logY_ = false;
    // Set by the scale buttons while laying out; applied in HandleInput.
    mutable bool toggleX_ = false;
    mutable bool toggleY_ = false;
    mutable std::string xButton_;
    mutable std::string yButton_;
};
