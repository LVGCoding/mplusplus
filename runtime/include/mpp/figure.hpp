#pragma once

// Graphs: what a program draws is sent to the editor, which shows it in a
// Graph tab. Each plot is written as one line of JSON to the file named by
// the environment variable MPP_RESULTS (see docs/language-plan.md):
//   {"kind":"graph","id":"node:12","title":"…","series":[{"type":"line","x":[…],"y":[…]}]}
// A plot is written again whenever it changes; the editor keeps the last.

#include "core.hpp"
#include "data.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace mpp {

struct Series {
    std::string type = "line";  // "line", "scatter" or "bar"
    std::vector<double> x;
    std::vector<double> y;
    std::vector<std::string> labels;  // bar: a name for each bar
    double width = 0;                 // bar: the width of each bar
    std::string legend;
    std::string color;
    std::string shape;  // scatter: "dot", "square", "diamond", …
};

struct Plot {
    std::string id;
    std::string title;
    std::string xlabel;
    std::string ylabel;
    std::string xscale = "linear";
    std::string yscale = "linear";
    std::optional<double> xmin, xmax, ymin, ymax;
    std::vector<Series> series;
};

inline std::string json_string(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c < 0x20) {
            char escape[8];
            std::snprintf(escape, sizeof escape, "\\u%04x", c);
            out += escape;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

inline std::string json_number(double value) {
    if (!std::isfinite(value)) return "null";
    char text[32];
    std::snprintf(text, sizeof text, "%.17g", value);
    return text;
}

inline std::string json_numbers(const std::vector<double>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) out += (i ? "," : "") + json_number(values[i]);
    return out + "]";
}

inline std::string json(const Plot& plot) {
    std::string out = "{\"kind\":\"graph\",\"id\":" + json_string(plot.id) +
                      ",\"title\":" + json_string(plot.title) +
                      ",\"xlabel\":" + json_string(plot.xlabel) +
                      ",\"ylabel\":" + json_string(plot.ylabel) +
                      ",\"xscale\":" + json_string(plot.xscale) +
                      ",\"yscale\":" + json_string(plot.yscale);
    auto range = [&](const char* key, const std::optional<double>& value) {
        if (value) out += std::string(",\"") + key + "\":" + json_number(*value);
    };
    range("xmin", plot.xmin);
    range("xmax", plot.xmax);
    range("ymin", plot.ymin);
    range("ymax", plot.ymax);
    out += ",\"series\":[";
    for (std::size_t i = 0; i < plot.series.size(); ++i) {
        const Series& s = plot.series[i];
        out += std::string(i ? "," : "") + "{\"type\":" + json_string(s.type) +
               ",\"x\":" + json_numbers(s.x) + ",\"y\":" + json_numbers(s.y) +
               ",\"legend\":" + json_string(s.legend) + ",\"color\":" + json_string(s.color) +
               ",\"shape\":" + json_string(s.shape) + ",\"width\":" + json_number(s.width);
        if (!s.labels.empty()) {
            out += ",\"labels\":[";
            for (std::size_t k = 0; k < s.labels.size(); ++k) {
                out += (k ? "," : "") + json_string(s.labels[k]);
            }
            out += "]";
        }
        out += "}";
    }
    return out + "]}";
}

// Sends a plot to the editor. Without MPP_RESULTS (run outside the editor)
// there is nowhere to send it, and it is dropped.
inline void publish(const Plot& plot) {
    static std::FILE* file = [] {
        const char* path = std::getenv("MPP_RESULTS");
        return path ? std::fopen(path, "a") : nullptr;
    }();
    if (!file) return;
    const std::string line = json(plot) + "\n";
    std::fwrite(line.data(), 1, line.size(), file);
    std::fflush(file);
}

// --- Functions -------------------------------------------------------------------------

// f at 400 points across [from, to] (evenly spaced, or by ratio on a log
// axis). Where f fails (√ of a negative, …) the curve has a gap.
template <class F>
Series sample(F f, double from, double to, bool logarithmic, std::string legend) {
    Series series;
    series.legend = std::move(legend);
    constexpr int POINTS = 400;
    if (logarithmic && (from <= 0 || to <= 0)) logarithmic = false;
    for (int i = 0; i < POINTS; ++i) {
        const double t = static_cast<double>(i) / (POINTS - 1);
        const double x = logarithmic ? from * std::pow(to / from, t) : from + (to - from) * t;
        double y = std::numeric_limits<double>::quiet_NaN();
        const int saved = where;
        try {
            y = f(x);
        } catch (const Error&) {
        }
        where = saved;
        series.x.push_back(x);
        series.y.push_back(y);
    }
    return series;
}

// --- Data ---------------------------------------------------------------------------------

inline std::vector<double> data_of(const Eigen::VectorXd& v) {
    return std::vector<double>(v.data(), v.data() + v.size());
}
inline std::vector<double> data_of(const List<double>& list) { return list.items; }
inline std::vector<double> data_of(const List<std::int64_t>& list) {
    return std::vector<double>(list.items.begin(), list.items.end());
}

inline void same_count(int at, const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size()) {
        fail_at(at, "the data has " + std::to_string(x.size()) + " x values but " +
                        std::to_string(y.size()) + " y values");
    }
}

// (xs, ys) in a graph: points.
inline Series points(int at, std::vector<double> x, std::vector<double> y, std::string legend) {
    same_count(at, x, y);
    Series series;
    series.type = "scatter";
    series.x = std::move(x);
    series.y = std::move(y);
    series.legend = std::move(legend);
    return series;
}

// --- Figures ----------------------------------------------------------------------------------

// fig := Figure⟨title = "…", scaleY = "log"⟩, then fig.Line⟨x = xs, y = ys⟩ …
// Each change sends the whole figure again.
struct Figure {
    Plot plot;

    Figure() = default;
    Figure(int at, std::string title, std::string xlabel, std::string ylabel, std::string scaleX,
           std::string scaleY) {
        static int count = 0;
        plot.id = "figure:" + std::to_string(++count);
        plot.title = std::move(title);
        plot.xlabel = std::move(xlabel);
        plot.ylabel = std::move(ylabel);
        plot.xscale = check_scale(at, scaleX, "scaleX");
        plot.yscale = check_scale(at, scaleY, "scaleY");
        publish(plot);
    }

    static std::string check_scale(int at, const std::string& scale, const char* name) {
        if (scale.empty()) return "linear";
        if (scale != "linear" && scale != "log") {
            fail_at(at, std::string(name) + " is \"linear\" or \"log\", not \"" + scale + "\"");
        }
        return scale;
    }

    void add(Series series) {
        plot.series.push_back(std::move(series));
        publish(plot);
    }

    void line(int at, std::vector<double> x, std::vector<double> y, std::string legend,
              std::string color) {
        same_count(at, x, y);
        add(Series{.type = "line", .x = std::move(x), .y = std::move(y),
                   .legend = std::move(legend), .color = std::move(color)});
    }
    void scatter(int at, std::vector<double> x, std::vector<double> y, std::string legend,
                 std::string color, std::string shape) {
        same_count(at, x, y);
        add(Series{.type = "scatter", .x = std::move(x), .y = std::move(y),
                   .legend = std::move(legend), .color = std::move(color),
                   .shape = std::move(shape)});
    }
    // Counts of the values in `bins` equal buckets from the smallest to the largest.
    void histogram(int at, std::vector<double> values, std::int64_t bins, std::string legend,
                   std::string color) {
        if (bins < 1) fail_at(at, "a histogram needs at least 1 bin");
        if (values.empty()) fail_at(at, "a histogram needs some values");
        double low = values[0], high = values[0];
        for (double v : values) {
            low = std::min(low, v);
            high = std::max(high, v);
        }
        if (high == low) high = low + 1;
        const double width = (high - low) / static_cast<double>(bins);
        Series series{.type = "bar", .width = width, .legend = std::move(legend),
                      .color = std::move(color)};
        std::vector<double> counts(static_cast<std::size_t>(bins), 0);
        for (double v : values) {
            auto bin = static_cast<std::size_t>((v - low) / width);
            if (bin >= counts.size()) bin = counts.size() - 1;
            counts[bin] += 1;
        }
        for (std::size_t i = 0; i < counts.size(); ++i) {
            series.x.push_back(low + width * (static_cast<double>(i) + 0.5));
            series.y.push_back(counts[i]);
        }
        add(std::move(series));
    }
    // One bar per value, at 1, 2, 3, …, named by `labels` if given.
    void bar(int at, std::vector<std::string> labels, std::vector<double> values,
             std::string legend, std::string color) {
        if (!labels.empty() && labels.size() != values.size()) {
            fail_at(at, "there are " + std::to_string(labels.size()) + " labels but " +
                            std::to_string(values.size()) + " values");
        }
        Series series{.type = "bar", .labels = std::move(labels), .width = 0.8,
                      .legend = std::move(legend), .color = std::move(color)};
        for (std::size_t i = 0; i < values.size(); ++i) {
            series.x.push_back(static_cast<double>(i + 1));
            series.y.push_back(values[i]);
        }
        add(std::move(series));
    }

    std::string text() const { return "Figure " + json_string(plot.title); }
};

inline std::vector<std::string> labels_of(const List<std::string>& list) { return list.items; }

}  // namespace mpp
