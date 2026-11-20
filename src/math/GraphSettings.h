#pragma once

// A graph's axis ranges, changed from the graph settings menu. The title, axis
// labels and functions are editable math in the graph itself (GraphNode).
//
// Stored with the labels as the graph environment's options, one source line:
//   \begin{graph}[title=Growth, xlabel=t, xmin=0, xmax=5] y=x^2 \\ y=2x \end{graph}
// Only values that differ from the defaults are written (format: Options.h).
// Options this version doesn't know are kept and written back unchanged, so
// files from newer (or older) versions don't lose settings when saved.

#include "Options.h"

#include <string>
#include <utility>
#include <vector>

struct GraphSettings {
    double xMin = -10;
    double xMax = 10;
    double yMin = -10;
    double yMax = 10;
    // "linear" or "log".
    std::string xScale = "linear";
    std::string yScale = "linear";
    // key=value options this version doesn't understand, in their original order.
    std::vector<options::Option> unknown;

    // Read the range options; anything else goes to `unknown`. Known keys with
    // unreadable values keep their defaults.
    static GraphSettings FromOptions(const std::vector<options::Option>& items);

    // The options to store: non-default ranges, then the unknown ones.
    std::vector<std::string> OptionItems() const;
    std::string ToOptions() const {
        return options::Join(OptionItems());
    }

    bool operator==(const GraphSettings&) const = default;
};
