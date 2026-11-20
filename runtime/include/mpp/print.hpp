#pragma once

// print(...) and how values are written as text.

#include <Eigen/Dense>

#include <algorithm>
#include <cstdint>
#include <format>
#include <vector>
#include <iostream>
#include <string>
#include <string_view>

namespace mpp {

inline std::string to_text(std::string_view text) { return std::string(text); }
inline std::string to_text(const char* text) { return text; }
inline std::string to_text(bool value) { return value ? "true" : "false"; }
inline std::string to_text(std::int64_t value) { return std::to_string(value); }
inline std::string to_text(int value) { return std::to_string(value); }
// At most 12 significant digits, so rounding noise doesn't show:
// 0.1 + 0.2 prints as 0.3, not 0.30000000000000004.
inline std::string to_text(double value) {
    if (value == 0) return "0";  // also for −0
    std::string text = std::format("{:.12g}", value);
    return text;
}

// A vector as (1, 2, 3).
inline std::string to_text(const Eigen::VectorXd& v) {
    std::string text = "(";
    for (Eigen::Index i = 0; i < v.size(); ++i) {
        text += (i ? ", " : "") + to_text(v(i));
    }
    return text + ")";
}

// A matrix as rows in brackets, its columns lined up on the right:
//   [1  -2]
//   [3   4]
inline std::string to_text(const Eigen::MatrixXd& m) {
    std::vector<std::size_t> widths(static_cast<std::size_t>(m.cols()), 0);
    for (Eigen::Index c = 0; c < m.cols(); ++c)
        for (Eigen::Index r = 0; r < m.rows(); ++r)
            widths[c] = std::max(widths[c], to_text(m(r, c)).size());
    std::string text;
    for (Eigen::Index r = 0; r < m.rows(); ++r) {
        text += r ? "\n[" : "[";
        for (Eigen::Index c = 0; c < m.cols(); ++c) {
            const std::string entry = to_text(m(r, c));
            text += std::string(c ? 2 : 0, ' ') + std::string(widths[c] - entry.size(), ' ') + entry;
        }
        text += "]";
    }
    return m.rows() == 0 ? "[]" : text;
}

// Adds one argument to the line being printed. A matrix starts on a row of
// its own, so its columns line up.
template <class T>
void append(std::string& line, const T& value) {
    if constexpr (std::is_same_v<T, Eigen::MatrixXd>) {
        if (!line.empty()) {
            while (!line.empty() && line.back() == ' ') line.pop_back();
            line += '\n';
        }
    }
    line += to_text(value);
}

// Writes the arguments with nothing between them, then a line break.
template <class... Args>
void print(const Args&... args) {
    std::string line;
    (append(line, args), ...);
    line += '\n';
    std::cout << line;
}

}  // namespace mpp
