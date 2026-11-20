#include "GraphSettings.h"

GraphSettings GraphSettings::FromOptions(const std::vector<options::Option>& items) {
    GraphSettings settings;
    for (const auto& [key, value] : items) {
        if (key == "xmin") {
            options::ParseNumber(value, settings.xMin);
        } else if (key == "xmax") {
            options::ParseNumber(value, settings.xMax);
        } else if (key == "ymin") {
            options::ParseNumber(value, settings.yMin);
        } else if (key == "ymax") {
            options::ParseNumber(value, settings.yMax);
        } else if ((key == "xscale" || key == "yscale") && (value == "linear" || value == "log")) {
            (key == "xscale" ? settings.xScale : settings.yScale) = value;
        } else {
            settings.unknown.emplace_back(key, value);
        }
    }
    return settings;
}

std::vector<std::string> GraphSettings::OptionItems() const {
    const GraphSettings defaults;
    std::vector<std::string> items;
    auto number = [&](const char* key, double value, double fallback) {
        if (value != fallback) {
            items.push_back(options::Item(key, options::FormatNumber(value)));
        }
    };
    number("xmin", xMin, defaults.xMin);
    number("xmax", xMax, defaults.xMax);
    number("ymin", yMin, defaults.yMin);
    number("ymax", yMax, defaults.yMax);
    if (xScale != defaults.xScale) {
        items.push_back(options::Item("xscale", xScale));
    }
    if (yScale != defaults.yScale) {
        items.push_back(options::Item("yscale", yScale));
    }
    for (const auto& [key, value] : unknown) {
        items.push_back(options::Item(key, value));
    }
    return items;
}
