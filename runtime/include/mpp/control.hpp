#pragma once

// Loops and functions: ranges for `for`, and the error for a piecewise
// value none of whose cases applies.

#include "core.hpp"
#include "print.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

namespace mpp {

// first, first+step, ... up to and including last (when it is reached), for
//   for i ∈ a..b      (step 1)
//   for i ∈ a..s..b   (step s; negative counts down)
// The values are computed as first + k·step, so ℝ steps don't drift.
template <class T>
struct Range {
    T first;
    T step;
    std::int64_t count;

    struct iterator {
        const Range* range;
        std::int64_t index;
        T operator*() const { return range->first + static_cast<T>(index) * range->step; }
        iterator& operator++() {
            ++index;
            return *this;
        }
        bool operator!=(const iterator& other) const { return index != other.index; }
    };
    iterator begin() const { return {this, 0}; }
    iterator end() const { return {this, count}; }
};

inline Range<std::int64_t> range(int at, std::int64_t first, std::int64_t step,
                                 std::int64_t last) {
    if (step == 0) fail_at(at, "the step of a range can't be 0");
    const __int128 span = static_cast<__int128>(last) - first;
    if ((step > 0 && span < 0) || (step < 0 && span > 0)) return {first, step, 0};
    return {first, step, static_cast<std::int64_t>(span / step + 1)};
}
inline Range<std::int64_t> range(int at, std::int64_t first, std::int64_t last) {
    return range(at, first, 1, last);
}

inline Range<double> range(int at, double first, double step, double last) {
    if (step == 0) fail_at(at, "the step of a range can't be 0");
    if (!std::isfinite(first) || !std::isfinite(step) || !std::isfinite(last)) {
        fail_at(at, "a range needs finite numbers");
    }
    // A little tolerance so 0..0.1..1 includes 1 despite rounding.
    const double steps = (last - first) / step;
    if (steps < -1e-9) return {first, step, 0};
    if (steps > 1e15) fail_at(at, "this range has too many values");
    return {first, step, static_cast<std::int64_t>(std::floor(steps + 1e-9)) + 1};
}
inline Range<double> range(int at, double first, double last) {
    return range(at, first, 1.0, last);
}

// The value of a piecewise expression when none of its conditions holds.
template <class T>
[[noreturn]] T no_case(int at) {
    fail_at(at, "none of the cases applies");
}

}  // namespace mpp
