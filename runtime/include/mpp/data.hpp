#pragma once

// Data: lists (T*), maps (V^K), string operations, and tables. A list of
// structs prints as a table, its columns set by the fields' @tags.

#include "core.hpp"
#include "print.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mpp {

// --- Strings --------------------------------------------------------------------------

// Length in characters (UTF-8 code points), not bytes.
inline std::int64_t length(const std::string& text) {
    std::int64_t count = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80) ++count;
    return count;
}
inline std::int64_t size(const std::string& text) { return length(text); }

inline std::string repeat(int at, const std::string& text, std::int64_t times) {
    if (times < 0) fail_at(at, "a string can't be repeated a negative number of times");
    if (times > 0 && text.size() * static_cast<std::size_t>(times) > 100000000) {
        fail_at(at, "this string would be too long");
    }
    std::string result;
    for (std::int64_t i = 0; i < times; ++i) result += text;
    return result;
}

// needle in haystack: is it part of it?
inline bool contains_text(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// The value must be one of the declared choices (scale ∈ {"linear", "log"}).
template <class T, class Choices>
T one_of(int at, T value, const Choices& choices, const char* name) {
    for (const auto& choice : choices)
        if (choice == value) return value;
    std::string listed;
    for (const auto& choice : choices) listed += (listed.empty() ? "" : ", ") + to_text(choice);
    fail_at(at, std::string(name) + " must be one of " + listed + ", not " + to_text(value));
}

// --- Lists ----------------------------------------------------------------------------------

template <class T>
struct List {
    std::vector<T> items;

    List() = default;
    explicit List(std::vector<T> values) : items(std::move(values)) {}

    auto begin() const { return items.begin(); }
    auto end() const { return items.end(); }
    void push(T value) { items.push_back(std::move(value)); }
    bool operator==(const List&) const = default;

    T& at(int loc, std::int64_t i) { return items[position(loc, i)]; }
    const T& at(int loc, std::int64_t i) const { return items[position(loc, i)]; }

    std::size_t position(int loc, std::int64_t i) const {
        if (i < 1 || i > static_cast<std::int64_t>(items.size())) {
            fail_at(loc, "index " + std::to_string(i) + " is outside 1.." +
                             std::to_string(items.size()));
        }
        return static_cast<std::size_t>(i - 1);
    }
};

template <class T>
std::int64_t size(const List<T>& list) {
    return static_cast<std::int64_t>(list.items.size());
}
template <class T, class U>
bool contains(const List<T>& list, const U& value) {
    return std::find(list.items.begin(), list.items.end(), value) != list.items.end();
}

// --- Maps -------------------------------------------------------------------------------------

template <class K, class V>
struct Map {
    std::map<K, V> entries;

    bool operator==(const Map&) const = default;

    const V& get(int at, const K& key) const {
        const auto found = entries.find(key);
        if (found == entries.end()) fail_at(at, "there is no entry for " + to_text(key));
        return found->second;
    }
    void set(K key, V value) { entries.insert_or_assign(std::move(key), std::move(value)); }
    bool has(const K& key) const { return entries.contains(key); }
    std::vector<K> keys() const {
        std::vector<K> result;
        for (const auto& [key, value] : entries) result.push_back(key);
        return result;
    }
};

template <class K, class V>
std::int64_t size(const Map<K, V>& map) {
    return static_cast<std::int64_t>(map.entries.size());
}

// {a: 1, b: 2}
template <class K, class V>
std::string to_text(const Map<K, V>& map) {
    std::string text = "{";
    bool first = true;
    for (const auto& [key, value] : map.entries) {
        text += (first ? "" : ", ") + to_text(key) + ": " + to_text(value);
        first = false;
    }
    return text + "}";
}

// --- Tables -----------------------------------------------------------------------------------

// A column of a table: a struct field and its @tags.
struct Column {
    std::string label;    // @label, else the field's name
    std::string align;    // @align: "left", "right" or "center"
    std::string filler;   // @filler: what pads the column (one character)
    std::string format;   // @format: as in std::format, e.g. ".2f"
    std::string prefix;   // @prefix, @postfix: around each value
    std::string postfix;
    std::string unit;     // @unit: after the label, in brackets
    bool hidden = false;  // @hidden
};

// One value of a table, written as its column says.
template <class T>
std::string cell(const T& value, const Column& column) {
    std::string text;
    if (column.format.empty()) {
        text = to_text(value);
    } else {
        try {
            if constexpr (std::is_arithmetic_v<T>) {
                // ".2f" on a whole number formats it as a real.
                const bool real = column.format.find_first_of("fFeEgG%") != std::string::npos;
                text = real ? std::vformat("{:" + column.format + "}",
                                           std::make_format_args(static_cast<const double&>(
                                               static_cast<double>(value))))
                            : std::vformat("{:" + column.format + "}", std::make_format_args(value));
            } else if constexpr (std::is_same_v<T, std::string>) {
                text = std::vformat("{:" + column.format + "}", std::make_format_args(value));
            } else {
                text = to_text(value);
            }
        } catch (const std::format_error&) {
            fail("@format(\"" + column.format + "\") doesn't fit the value " + to_text(value));
        }
    }
    return column.prefix + text + column.postfix;
}

// `text` padded to `width` characters as the column aligns it.
inline std::string pad(const std::string& text, std::size_t width, const Column& column) {
    const std::size_t have = static_cast<std::size_t>(length(text));
    if (have >= width) return text;
    const std::size_t missing = width - have;
    const std::string fill = column.filler.empty() ? " " : column.filler;
    auto repeated = [&](std::size_t n) {
        std::string out;
        for (std::size_t i = 0; i < n; ++i) out += fill;
        return out;
    };
    if (column.align == "right") return repeated(missing) + text;
    if (column.align == "center") {
        return repeated(missing / 2) + text + repeated(missing - missing / 2);
    }
    return text + repeated(missing);
}

// A header row and the rows, lined up in columns two spaces apart.
inline std::string table_text(const std::vector<Column>& columns,
                              const std::vector<std::vector<std::string>>& rows) {
    std::vector<std::string> headers;
    for (const Column& column : columns) {
        headers.push_back(column.unit.empty() ? column.label
                                              : column.label + " (" + column.unit + ")");
    }
    std::vector<std::size_t> widths(columns.size(), 0);
    for (std::size_t c = 0; c < columns.size(); ++c) {
        widths[c] = static_cast<std::size_t>(length(headers[c]));
        for (const auto& row : rows) {
            widths[c] = std::max(widths[c], static_cast<std::size_t>(length(row[c])));
        }
    }
    auto line = [&](const std::vector<std::string>& cells) {
        std::string text;
        bool first = true;
        for (std::size_t c = 0; c < columns.size(); ++c) {
            if (columns[c].hidden) continue;
            text += (first ? "" : "  ") + pad(cells[c], widths[c], columns[c]);
            first = false;
        }
        while (!text.empty() && text.back() == ' ') text.pop_back();
        return text;
    };
    std::string text = line(headers);
    for (const auto& row : rows) text += "\n" + line(row);
    return text;
}

// A struct written by M++ has columns() and cells(), so a list of them is a table.
template <class T>
concept Tabular = requires(const T& row) {
    { T::columns() } -> std::convertible_to<std::vector<Column>>;
    { row.cells() } -> std::convertible_to<std::vector<std::string>>;
};

// [1, 2, 3], or a table for a list of structs.
template <class T>
std::string to_text(const List<T>& list) {
    if constexpr (Tabular<T>) {
        std::vector<std::vector<std::string>> rows;
        for (const T& row : list.items) rows.push_back(row.cells());
        return table_text(T::columns(), rows);
    } else {
        std::string text = "[";
        bool first = true;
        for (const T& value : list.items) {
            text += (first ? "" : ", ") + to_text(value);
            first = false;
        }
        return text + "]";
    }
}

template <class T>
struct starts_own_line<List<T>> : std::bool_constant<Tabular<T>> {};

}  // namespace mpp
