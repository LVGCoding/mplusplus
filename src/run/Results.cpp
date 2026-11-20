#include "Results.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <memory>

namespace run {

namespace {

// Just enough JSON for the results file: objects, arrays, strings, numbers,
// null, true and false.
struct Json {
    enum class Kind { Null, Bool, Number, String, Array, Object } kind = Kind::Null;
    double number = 0;
    bool boolean = false;
    std::string string;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;

    const Json* Get(const std::string& key) const {
        for (const auto& [name, value] : fields) {
            if (name == key) return &value;
        }
        return nullptr;
    }
};

class Reader {
public:
    explicit Reader(const std::string& text) : text_(text) {}

    std::optional<Json> Value() {
        Space();
        if (at_ >= text_.size()) return std::nullopt;
        const char c = text_[at_];
        Json value;
        if (c == '{') {
            ++at_;
            value.kind = Json::Kind::Object;
            Space();
            if (Take('}')) return value;
            do {
                Space();
                auto key = String();
                Space();
                if (!key || !Take(':')) return std::nullopt;
                auto item = Value();
                if (!item) return std::nullopt;
                value.fields.emplace_back(std::move(*key), std::move(*item));
                Space();
            } while (Take(','));
            return Take('}') ? std::optional<Json>(value) : std::nullopt;
        }
        if (c == '[') {
            ++at_;
            value.kind = Json::Kind::Array;
            Space();
            if (Take(']')) return value;
            do {
                auto item = Value();
                if (!item) return std::nullopt;
                value.items.push_back(std::move(*item));
                Space();
            } while (Take(','));
            return Take(']') ? std::optional<Json>(value) : std::nullopt;
        }
        if (c == '"') {
            auto string = String();
            if (!string) return std::nullopt;
            value.kind = Json::Kind::String;
            value.string = std::move(*string);
            return value;
        }
        if (Word("null")) return value;
        if (Word("true") || Word("false")) {
            value.kind = Json::Kind::Bool;
            value.boolean = text_[at_ - 1] == 'e' && text_[at_ - 2] == 'u';
            return value;
        }
        const char* start = text_.c_str() + at_;
        char* end = nullptr;
        value.number = std::strtod(start, &end);
        if (end == start) return std::nullopt;
        at_ += static_cast<std::size_t>(end - start);
        value.kind = Json::Kind::Number;
        return value;
    }

private:
    const std::string& text_;
    std::size_t at_ = 0;

    void Space() {
        while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\n' ||
                                      text_[at_] == '\r' || text_[at_] == '\t')) {
            ++at_;
        }
    }
    bool Take(char c) {
        if (at_ < text_.size() && text_[at_] == c) {
            ++at_;
            return true;
        }
        return false;
    }
    bool Word(const char* word) {
        const std::string w(word);
        if (text_.compare(at_, w.size(), w) == 0) {
            at_ += w.size();
            return true;
        }
        return false;
    }
    std::optional<std::string> String() {
        if (!Take('"')) return std::nullopt;
        std::string out;
        while (at_ < text_.size() && text_[at_] != '"') {
            char c = text_[at_++];
            if (c == '\\' && at_ < text_.size()) {
                const char e = text_[at_++];
                if (e == 'n') {
                    out += '\n';
                } else if (e == 'u' && at_ + 4 <= text_.size()) {
                    const unsigned code = static_cast<unsigned>(
                        std::strtoul(text_.substr(at_, 4).c_str(), nullptr, 16));
                    at_ += 4;
                    out += static_cast<char>(code < 0x80 ? code : '?');
                } else {
                    out += e;
                }
            } else {
                out += c;
            }
        }
        return Take('"') ? std::optional<std::string>(out) : std::nullopt;
    }
};

std::string Text(const Json& object, const char* key) {
    const Json* value = object.Get(key);
    return value && value->kind == Json::Kind::String ? value->string : std::string();
}

std::optional<double> Number(const Json& object, const char* key) {
    const Json* value = object.Get(key);
    if (value && value->kind == Json::Kind::Number) return value->number;
    return std::nullopt;
}

std::vector<double> Numbers(const Json& object, const char* key) {
    std::vector<double> numbers;
    if (const Json* value = object.Get(key); value && value->kind == Json::Kind::Array) {
        for (const Json& item : value->items) {
            numbers.push_back(item.kind == Json::Kind::Number
                                  ? item.number
                                  : std::numeric_limits<double>::quiet_NaN());
        }
    }
    return numbers;
}

}  // namespace

std::optional<PlotData> ParsePlot(const std::string& line) {
    auto json = Reader(line).Value();
    if (!json || json->kind != Json::Kind::Object || Text(*json, "kind") != "graph") {
        return std::nullopt;
    }
    PlotData plot;
    plot.id = Text(*json, "id");
    plot.title = Text(*json, "title");
    plot.xlabel = Text(*json, "xlabel");
    plot.ylabel = Text(*json, "ylabel");
    plot.xscale = Text(*json, "xscale") == "log" ? "log" : "linear";
    plot.yscale = Text(*json, "yscale") == "log" ? "log" : "linear";
    plot.xmin = Number(*json, "xmin");
    plot.xmax = Number(*json, "xmax");
    plot.ymin = Number(*json, "ymin");
    plot.ymax = Number(*json, "ymax");
    if (const Json* series = json->Get("series"); series && series->kind == Json::Kind::Array) {
        for (const Json& item : series->items) {
            if (item.kind != Json::Kind::Object) continue;
            PlotSeries s;
            s.type = Text(item, "type");
            s.x = Numbers(item, "x");
            s.y = Numbers(item, "y");
            s.width = Number(item, "width").value_or(0);
            s.legend = Text(item, "legend");
            s.color = Text(item, "color");
            s.shape = Text(item, "shape");
            if (const Json* labels = item.Get("labels");
                labels && labels->kind == Json::Kind::Array) {
                for (const Json& label : labels->items) s.labels.push_back(label.string);
            }
            plot.series.push_back(std::move(s));
        }
    }
    return plot;
}

std::vector<PlotData> LatestPlots(const std::vector<std::string>& lines) {
    std::vector<PlotData> plots;
    std::map<std::string, std::size_t> index;
    for (const std::string& line : lines) {
        auto plot = ParsePlot(line);
        if (!plot) continue;
        if (auto found = index.find(plot->id); found != index.end()) {
            plots[found->second] = std::move(*plot);
        } else {
            index[plot->id] = plots.size();
            plots.push_back(std::move(*plot));
        }
    }
    return plots;
}

}  // namespace run
