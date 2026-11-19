#include "Options.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace options {
namespace {
std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

// Split at commas that are not inside braces.
std::vector<std::string_view> SplitItems(std::string_view text) {
    std::vector<std::string_view> items;
    int depth = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}') {
            depth = std::max(0, depth - 1);
        } else if (text[i] == ',' && depth == 0) {
            items.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    items.push_back(text.substr(start));
    return items;
}

std::string Unwrap(std::string_view value) {
    value = Trim(value);
    if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
        value = value.substr(1, value.size() - 2);
    }
    return std::string(value);
}
}  // namespace

std::vector<Option> Parse(std::string_view text) {
    std::vector<Option> result;
    for (std::string_view item : SplitItems(text)) {
        item = Trim(item);
        if (item.empty()) {
            continue;
        }
        const auto equals = item.find('=');
        std::string key(Trim(item.substr(0, equals)));
        std::string value =
            equals == std::string_view::npos ? std::string() : Unwrap(item.substr(equals + 1));
        result.emplace_back(std::move(key), std::move(value));
    }
    return result;
}

std::string Wrap(const std::string& value) {
    const bool needsBraces = value.empty() || value.find_first_of(",=[]\\") != std::string::npos ||
                             value.front() == ' ' || value.back() == ' ' || value.front() == '{';
    return needsBraces ? "{" + value + "}" : value;
}

std::string Item(const std::string& key,
                 const std::string& value) {
    return value.empty() ? key : key + "=" + Wrap(value);
}

std::string Join(const std::vector<std::string>& items) {
    std::string text;
    for (std::size_t i = 0; i < items.size(); ++i) {
        text += (i > 0 ? ", " : "") + items[i];
    }
    return text;
}

std::string FormatNumber(double value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

bool ParseNumber(std::string_view text,
                 double& value) {
    text = Trim(text);
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
    }
    double parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (text.empty() || result.ec != std::errc() || result.ptr != text.data() + text.size() ||
        !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

bool ParseBool(std::string_view text,
               bool& value) {
    text = Trim(text);
    if (text == "true") {
        value = true;
        return true;
    }
    if (text == "false") {
        value = false;
        return true;
    }
    return false;
}

std::string Environment(const std::string& environment,
                        const std::string& options,
                        const std::string& body) {
    std::string text = "\\begin{" + environment + "}";
    if (!options.empty() || (!body.empty() && body.front() == '[')) {
        text += "[" + options + "]";
    }
    return text + body + "\\end{" + environment + "}";
}

}  // namespace options
