#pragma once

// What a run sends back besides its output: plots, one JSON line each (see
// runtime/include/mpp/figure.hpp for the writer).

#include <optional>
#include <string>
#include <vector>

namespace run {

struct PlotSeries {
    std::string type = "line";  // "line", "scatter" or "bar"
    std::vector<double> x;      // NaN where a value is missing
    std::vector<double> y;
    std::vector<std::string> labels;
    double width = 0;
    std::string legend;
    std::string color;
    std::string shape;
};

struct PlotData {
    // "node:<GraphNode id>" for a graph in the document, "figure:<n>" for a Figure.
    std::string id;
    std::string title;
    std::string xlabel;
    std::string ylabel;
    std::string xscale = "linear";
    std::string yscale = "linear";
    std::optional<double> xmin, xmax, ymin, ymax;
    std::vector<PlotSeries> series;
};

// One line of the results file, or nullopt if it isn't a plot.
std::optional<PlotData> ParsePlot(const std::string& line);

// The plots in a run's results, the last version of each, in the order
// they first appeared.
std::vector<PlotData> LatestPlots(const std::vector<std::string>& lines);

}  // namespace run
