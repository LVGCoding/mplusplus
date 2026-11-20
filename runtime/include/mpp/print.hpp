#pragma once

// print(...) and how values are written as text.

#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <string_view>

namespace mpp {

inline std::string to_text(std::string_view text) { return std::string(text); }
inline std::string to_text(const char* text) { return text; }
inline std::string to_text(bool value) { return value ? "true" : "false"; }
inline std::string to_text(std::int64_t value) { return std::to_string(value); }
inline std::string to_text(int value) { return std::to_string(value); }
// Shortest text that reads back as the same number: 0.1 prints as "0.1".
inline std::string to_text(double value) { return std::format("{}", value); }

// Writes the arguments with nothing between them, then a line break.
template <class... Args>
void print(const Args&... args) {
    std::string line;
    ((line += to_text(args)), ...);
    line += '\n';
    std::cout << line;
}

}  // namespace mpp
