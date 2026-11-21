#include "GraphTab.h"

#include "math/GraphNode.h"
#include "ui/font.h"
#include "ui/rich_text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace {

constexpr Clay_Color BUTTON_COLOR = {236, 236, 236, 255};
constexpr Clay_Color BUTTON_HOVER_COLOR = {215, 215, 215, 255};
constexpr Color AXIS_COLOR = {60, 60, 60, 255};
constexpr Color GRID_COLOR = {230, 230, 230, 255};
constexpr Color TEXT_COLOR = {30, 30, 30, 255};
constexpr Color MUTED_COLOR = {120, 120, 120, 255};

// Room around the plot for the title, tick labels and axis labels.
constexpr float MARGIN_LEFT = 70;
constexpr float MARGIN_RIGHT = 24;
constexpr float MARGIN_TOP = 44;
constexpr float MARGIN_BOTTOM = 56;

std::uint32_t nextIndex = 1;

Color ToColor(Clay_Color c) {
    return {static_cast<unsigned char>(c.r), static_cast<unsigned char>(c.g),
            static_cast<unsigned char>(c.b), static_cast<unsigned char>(c.a)};
}

// A series' colour: its own (a name or #rrggbb), or the next of the palette.
Color SeriesColor(const run::PlotSeries& series, std::size_t index) {
    static const struct {
        const char* name;
        Color color;
    } NAMED[] = {{"red", {214, 39, 40, 255}},     {"blue", {31, 119, 180, 255}},
                 {"green", {44, 160, 44, 255}},   {"orange", {255, 127, 14, 255}},
                 {"purple", {148, 103, 189, 255}}, {"black", {20, 20, 20, 255}},
                 {"gray", {127, 127, 127, 255}},  {"grey", {127, 127, 127, 255}},
                 {"brown", {140, 86, 75, 255}},   {"pink", {227, 119, 194, 255}}};
    for (const auto& named : NAMED) {
        if (series.color == named.name) return named.color;
    }
    unsigned r = 0, g = 0, b = 0;
    if (series.color.size() == 7 && series.color[0] == '#' &&
        std::sscanf(series.color.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
        return {static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                static_cast<unsigned char>(b), 255};
    }
    return ToColor(GraphNode::FunctionColor(index));
}

// Text in the body font, with symbols it lacks (π, ⋅, √) from the math font.
float DrawRich(Font* fonts, const std::string& text, Vector2 at, float size, Color color,
               bool draw = true) {
    float x = at.x;
    rich_text::ForEachRun(text, FONT_BODY, [&](std::string_view run, std::uint16_t font) {
        const std::string piece(run);
        const Vector2 measured = MeasureTextEx(fonts[font], piece.c_str(), size, 0);
        if (draw) DrawTextEx(fonts[font], piece.c_str(), {x, at.y}, size, 0, color);
        x += measured.x;
    });
    return x - at.x;
}

float TextWidth(Font* fonts, const std::string& text, float size) {
    return DrawRich(fonts, text, {0, 0}, size, BLACK, false);
}

std::string FormatTick(double value) {
    if (std::abs(value) < 1e-12) value = 0;
    char text[32];
    std::snprintf(text, sizeof text, "%g", value);
    return text;
}

// One axis: its range and how values map to it.
struct Axis {
    double min = 0;
    double max = 1;
    bool log = false;

    double Unit(double value) const {
        if (log) return (std::log10(value) - std::log10(min)) / (std::log10(max) - std::log10(min));
        return (value - min) / (max - min);
    }

    // Tick positions: 1-2-5 steps, or powers of ten on a log axis that spans them.
    std::vector<double> Ticks() const {
        std::vector<double> ticks;
        if (log && std::log10(max) - std::log10(min) >= 1) {
            for (double p = std::floor(std::log10(min)); p <= std::ceil(std::log10(max)); ++p) {
                const double v = std::pow(10.0, p);
                if (v >= min * (1 - 1e-9) && v <= max * (1 + 1e-9)) ticks.push_back(v);
            }
            return ticks;
        }
        const double span = max - min;
        const double raw = span / 6;
        const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
        double step = magnitude;
        for (double factor : {1.0, 2.0, 5.0, 10.0}) {
            step = factor * magnitude;
            if (raw <= step) break;
        }
        for (double v = std::ceil(min / step) * step; v <= max + step * 1e-9; v += step) {
            ticks.push_back(v);
        }
        return ticks;
    }
};

// The range of `values` that can be drawn (positive only on a log axis),
// or nullopt if there are none.
std::optional<std::pair<double, double>> Extent(const std::vector<double>& values, bool log) {
    double low = std::numeric_limits<double>::infinity();
    double high = -low;
    for (double v : values) {
        if (!std::isfinite(v) || (log && v <= 0)) continue;
        low = std::min(low, v);
        high = std::max(high, v);
    }
    if (low > high) return std::nullopt;
    return std::make_pair(low, high);
}

// An axis covering [low, high], widened a little (and made non-empty).
Axis MakeAxis(double low, double high, bool log, bool pad) {
    if (log) {
        if (low == high) {
            low /= 10;
            high *= 10;
        } else if (pad) {
            const double ratio = std::pow(high / low, 0.05);
            low /= ratio;
            high *= ratio;
        }
    } else {
        if (low == high) {
            low -= 1;
            high += 1;
        } else if (pad) {
            const double margin = (high - low) * 0.05;
            low -= margin;
            high += margin;
        }
    }
    return {low, high, log};
}

}  // namespace

GraphTab::GraphTab(run::PlotData plot, const Tab* source)
    : plot_(std::move(plot)),
      source_(source),
      index_(nextIndex++),
      logX_(plot_.xscale == "log"),
      logY_(plot_.yscale == "log") {}

void GraphTab::SetPlot(run::PlotData plot) {
    plot_ = std::move(plot);
}

const std::string& GraphTab::TabName() const {
    tabName_ = plot_.title.empty() ? "Graph" : "Graph: " + plot_.title;
    return tabName_;
}

bool GraphTab::HandleInput(const EditorView& /*view*/) {
    const bool changed = toggleX_ || toggleY_;
    logX_ ^= toggleX_;
    logY_ ^= toggleY_;
    toggleX_ = toggleY_ = false;
    return changed;
}

void GraphTab::LayoutContent() const {
    xButton_ = std::string("x axis: ") + (logX_ ? "log" : "linear");
    yButton_ = std::string("y axis: ") + (logY_ ? "log" : "linear");
    // Fill the editor area as it was last frame (it shrinks when the terminal
    // grows), down to a smallest size that still reads; below that it scrolls.
    float width = 600;
    float height = 360;
    const auto workspace = Clay_GetElementData(CLAY_ID("Workspace"));
    const auto editor = Clay_GetElementData(CLAY_ID("Editor"));
    if (workspace.found && editor.found) {
        // Editor padding 6 and this element's 8, on both sides; the buttons row.
        constexpr float PADDING = 2 * (6 + 8);
        constexpr float BUTTONS = 26 + 8;
        width = workspace.boundingBox.x + workspace.boundingBox.width - editor.boundingBox.x -
                PADDING;
        height = workspace.boundingBox.height - PADDING - BUTTONS;
    }
    width = std::max(320.0f, std::floor(width));
    height = std::max(220.0f, std::floor(height));
    auto button = [&](const std::string& label, std::uint32_t id, bool& toggle) {
        CLAY(CLAY_IDI("GraphScaleButton", index_ * 2 + id),
             {.layout = {.padding = {10, 10, 4, 4}},
              .backgroundColor = Clay_Hovered() ? BUTTON_HOVER_COLOR : BUTTON_COLOR,
              .cornerRadius = CLAY_CORNER_RADIUS(4)}) {
            if (Clay_Hovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                toggle = true;
            }
            const Clay_String text = {.isStaticallyAllocated = false,
                                      .length = static_cast<int32_t>(label.size()),
                                      .chars = label.data()};
            CLAY_TEXT(text, CLAY_TEXT_CONFIG({.textColor = {30, 30, 30, 255},
                                              .fontId = FONT_BODY,
                                              .fontSize = 15}));
        }
    };
    CLAY(CLAY_IDI("GraphTab", index_),
         {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .padding = {8, 8, 8, 8},
                     .childGap = 8,
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        CLAY_AUTO_ID({.layout = {.childGap = 8}}) {
            button(xButton_, 0, toggleX_);
            button(yButton_, 1, toggleY_);
        }
        CLAY(CLAY_IDI("GraphPlotArea", index_),
             {.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height)}},
              .backgroundColor = {255, 255, 255, 255},
              .border = {.color = {210, 210, 210, 255}, .width = CLAY_BORDER_OUTSIDE(1)}}) {}
    }
}

void GraphTab::DrawOverlay(const EditorView& view) const {
    const auto area = LastLayoutBounds(CLAY_IDI("GraphPlotArea", index_));
    if (!area || !view.fonts) {
        return;
    }
    Font* fonts = view.fonts;
    BeginEditorClip(view);
    const Rectangle plot = {area->x + MARGIN_LEFT, area->y + MARGIN_TOP,
                            area->width - MARGIN_LEFT - MARGIN_RIGHT,
                            area->height - MARGIN_TOP - MARGIN_BOTTOM};

    // Title and axis labels.
    if (!plot_.title.empty()) {
        const float size = 20;
        DrawRich(fonts, plot_.title,
                 {area->x + (area->width - TextWidth(fonts, plot_.title, size)) / 2, area->y + 12},
                 size, TEXT_COLOR);
    }
    bool barLabels = false;
    for (const auto& series : plot_.series) barLabels |= !series.labels.empty();
    if (!plot_.xlabel.empty()) {
        // Below the bars' names, if there are any.
        DrawRich(fonts, plot_.xlabel,
                 {plot.x + (plot.width - TextWidth(fonts, plot_.xlabel, 16)) / 2,
                  plot.y + plot.height + (barLabels ? 38 : 26)},
                 16, TEXT_COLOR);
    }
    if (!plot_.ylabel.empty()) {
        DrawRich(fonts, plot_.ylabel, {area->x + 8, plot.y - 26}, 16, TEXT_COLOR);
    }

    // Ranges: the plot's own, or the data's.
    std::vector<double> xs, ys;
    bool bars = false;
    for (const auto& series : plot_.series) {
        xs.insert(xs.end(), series.x.begin(), series.x.end());
        ys.insert(ys.end(), series.y.begin(), series.y.end());
        if (series.type == "bar") {
            bars = true;
            for (double x : series.x) {
                xs.push_back(x - series.width / 2);
                xs.push_back(x + series.width / 2);
            }
        }
    }
    if (bars && !logY_) ys.push_back(0);  // bars stand on 0
    auto range = [&](std::optional<double> min, std::optional<double> max,
                     const std::vector<double>& values, bool log) -> std::optional<Axis> {
        if (min && max && *min < *max && (!log || *min > 0)) {
            return Axis{*min, *max, log};
        }
        const auto extent = Extent(values, log);
        if (!extent) return std::nullopt;
        return MakeAxis(extent->first, extent->second, log, true);
    };
    // A graph in the document gives its y range; with a log axis it may not
    // fit, so the data decides then.
    const auto xAxis = range(plot_.xmin, plot_.xmax, xs, logX_);
    const auto yAxis = range(plot_.ymin, plot_.ymax, ys, logY_);
    if (!xAxis || !yAxis) {
        const std::string message = plot_.series.empty() ? "Nothing to draw yet."
                                    : (logX_ || logY_)   ? "A log axis needs positive values."
                                                         : "There's nothing to draw: no values.";
        DrawRich(fonts, message, {plot.x + 10, plot.y + 10}, 16, MUTED_COLOR);
        EndScissorMode();
        return;
    }
    auto toScreen = [&](double x, double y) -> Vector2 {
        return {static_cast<float>(plot.x + xAxis->Unit(x) * plot.width),
                static_cast<float>(plot.y + plot.height - yAxis->Unit(y) * plot.height)};
    };

    // Grid and ticks.
    for (double x : xAxis->Ticks()) {
        const float sx = toScreen(x, yAxis->min).x;
        DrawLineEx({sx, plot.y}, {sx, plot.y + plot.height}, 1, GRID_COLOR);
        const std::string label = FormatTick(x);
        DrawRich(fonts, label, {sx - TextWidth(fonts, label, 14) / 2, plot.y + plot.height + 6},
                 14, MUTED_COLOR);
    }
    for (double y : yAxis->Ticks()) {
        const float sy = toScreen(xAxis->min, y).y;
        DrawLineEx({plot.x, sy}, {plot.x + plot.width, sy}, 1, GRID_COLOR);
        const std::string label = FormatTick(y);
        DrawRich(fonts, label, {plot.x - 8 - TextWidth(fonts, label, 14), sy - 8}, 14,
                 MUTED_COLOR);
    }
    // The axes through 0 where they are in view, else the plot's edges.
    const bool zeroX = !xAxis->log && xAxis->min < 0 && xAxis->max > 0;
    const bool zeroY = !yAxis->log && yAxis->min < 0 && yAxis->max > 0;
    const float axisX = zeroX ? toScreen(0, yAxis->min).x : plot.x;
    const float axisY = zeroY ? toScreen(xAxis->min, 0).y : plot.y + plot.height;
    DrawLineEx({axisX, plot.y}, {axisX, plot.y + plot.height}, 1.5f, AXIS_COLOR);
    DrawLineEx({plot.x, axisY}, {plot.x + plot.width, axisY}, 1.5f, AXIS_COLOR);

    // The series, clipped to the plot.
    // Scissor regions don't nest, so this one is cut to the editor's visible
    // area too; otherwise the series would draw over the terminal and menus.
    {
        const float left = std::max(plot.x, view.viewport.x);
        const float top = std::max(plot.y, view.viewport.y);
        const float right =
            std::min(plot.x + plot.width + 1, view.viewport.x + view.viewport.width);
        const float bottom =
            std::min(plot.y + plot.height + 1, view.viewport.y + view.viewport.height);
        BeginScissorMode(static_cast<int>(std::ceil(left)), static_cast<int>(std::ceil(top)),
                         static_cast<int>(std::max(0.0f, std::floor(right - left))),
                         static_cast<int>(std::max(0.0f, std::floor(bottom - top))));
    }
    auto drawable = [&](double x, double y) {
        return std::isfinite(x) && std::isfinite(y) && (!xAxis->log || x > 0) &&
               (!yAxis->log || y > 0);
    };
    for (std::size_t s = 0; s < plot_.series.size(); ++s) {
        const run::PlotSeries& series = plot_.series[s];
        const Color color = SeriesColor(series, s);
        const std::size_t count = std::min(series.x.size(), series.y.size());
        if (series.type == "bar") {
            const double base = yAxis->log ? yAxis->min : 0;
            for (std::size_t i = 0; i < count; ++i) {
                if (!drawable(series.x[i], series.y[i])) continue;
                const Vector2 a = toScreen(series.x[i] - series.width / 2, series.y[i]);
                const Vector2 b = toScreen(series.x[i] + series.width / 2, base);
                const Rectangle bar = {std::min(a.x, b.x), std::min(a.y, b.y),
                                       std::abs(b.x - a.x), std::abs(b.y - a.y)};
                DrawRectangleRec(bar, Fade(color, 0.75f));
                DrawRectangleLinesEx(bar, 1, color);
            }
        } else if (series.type == "scatter") {
            for (std::size_t i = 0; i < count; ++i) {
                if (!drawable(series.x[i], series.y[i])) continue;
                const Vector2 p = toScreen(series.x[i], series.y[i]);
                if (series.shape == "square" || series.shape == "fsquare") {
                    DrawRectangleV({p.x - 3.5f, p.y - 3.5f}, {7, 7}, color);
                } else if (series.shape == "diamond" || series.shape == "fdiamond") {
                    DrawPoly(p, 4, 5, 0, color);
                } else {
                    DrawCircleV(p, 3.5f, color);
                }
            }
        } else {
            // A line through the points, broken where a value is missing or
            // where it leaps across the whole plot (an asymptote, as in tan x).
            for (std::size_t i = 1; i < count; ++i) {
                if (!drawable(series.x[i - 1], series.y[i - 1]) ||
                    !drawable(series.x[i], series.y[i])) {
                    continue;
                }
                const Vector2 a = toScreen(series.x[i - 1], series.y[i - 1]);
                const Vector2 b = toScreen(series.x[i], series.y[i]);
                if (std::abs(b.y - a.y) > plot.height * 3) continue;
                DrawLineEx(a, b, 2, color);
            }
        }
    }
    EndScissorMode();
    BeginEditorClip(view);

    // Bar names under their bars.
    for (const auto& series : plot_.series) {
        for (std::size_t i = 0; i < series.labels.size() && i < series.x.size(); ++i) {
            const float sx = toScreen(series.x[i], yAxis->min).x;
            const std::string& label = series.labels[i];
            DrawRich(fonts, label, {sx - TextWidth(fonts, label, 14) / 2, plot.y + plot.height + 20},
                     14, TEXT_COLOR);
        }
    }

    // Legend, top right, for the series that have one.
    float legendWidth = 0;
    int entries = 0;
    for (const auto& series : plot_.series) {
        if (series.legend.empty()) continue;
        legendWidth = std::max(legendWidth, TextWidth(fonts, series.legend, 15));
        ++entries;
    }
    if (entries > 0) {
        const float rowHeight = 22;
        const Rectangle box = {plot.x + plot.width - legendWidth - 52, plot.y + 8,
                               legendWidth + 44, rowHeight * static_cast<float>(entries) + 8};
        DrawRectangleRec(box, Fade(WHITE, 0.9f));
        DrawRectangleLinesEx(box, 1, GRID_COLOR);
        float y = box.y + 4;
        for (std::size_t s = 0; s < plot_.series.size(); ++s) {
            const run::PlotSeries& series = plot_.series[s];
            if (series.legend.empty()) continue;
            const Color color = SeriesColor(series, s);
            if (series.type == "line") {
                DrawLineEx({box.x + 8, y + 10}, {box.x + 28, y + 10}, 2.5f, color);
            } else {
                DrawRectangleV({box.x + 12, y + 5}, {10, 10}, color);
            }
            DrawRich(fonts, series.legend, {box.x + 34, y + 2}, 15, TEXT_COLOR);
            y += rowHeight;
        }
    }
    EndScissorMode();
}
