#pragma once

// Numbers: checked ℤ arithmetic, and ℝ operations and functions that stop
// with an error instead of quietly giving NaN. Each takes the location to
// blame first (see core.hpp).

#include "core.hpp"
#include "print.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string>

namespace mpp {

using Int = std::int64_t;

inline constexpr double infinity = std::numeric_limits<double>::infinity();

// --- ℤ arithmetic, checked for overflow ------------------------------------------

[[noreturn]] inline void overflow(int at) {
    fail_at(at, "the result is too big for ℤ (more than 9223372036854775807)");
}

inline Int add(int at, Int a, Int b) {
    Int result;
    if (__builtin_add_overflow(a, b, &result)) overflow(at);
    return result;
}
inline Int sub(int at, Int a, Int b) {
    Int result;
    if (__builtin_sub_overflow(a, b, &result)) overflow(at);
    return result;
}
inline Int mul(int at, Int a, Int b) {
    Int result;
    if (__builtin_mul_overflow(a, b, &result)) overflow(at);
    return result;
}
inline Int neg(int at, Int a) { return sub(at, 0, a); }

// --- Division and remainders --------------------------------------------------------

inline double div(int at, double a, double b) {
    if (b == 0) fail_at(at, "division by zero");
    return a / b;
}

// a mod b has the sign of b, as in mathematics: −7 mod 3 = 2.
inline Int mod(int at, Int a, Int b) {
    if (b == 0) fail_at(at, "mod 0 is undefined");
    Int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
inline double mod(int at, double a, double b) {
    if (b == 0) fail_at(at, "mod 0 is undefined");
    double r = std::fmod(a, b);
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

// --- Powers and roots ------------------------------------------------------------------

inline Int pow(int at, Int base, Int exponent) {
    if (exponent < 0) {
        fail_at(at, "a negative power of a whole number isn't whole: " + to_text(base) + "^" +
                        to_text(exponent) + ". Write the base as a real number (" +
                        to_text(base) + ".0) to get a fraction");
    }
    Int result = 1;
    while (exponent > 0) {
        if (exponent & 1) result = mul(at, result, base);
        exponent >>= 1;
        if (exponent > 0) base = mul(at, base, base);
    }
    return result;
}

inline double pow(int at, double base, double exponent) {
    if (base == 0 && exponent < 0) fail_at(at, "0 to a negative power is undefined");
    const double result = std::pow(base, exponent);
    if (std::isnan(result) && !std::isnan(base) && !std::isnan(exponent)) {
        fail_at(at, to_text(base) + "^" + to_text(exponent) + " isn't a real number");
    }
    return result;
}

inline double sqrt(int at, double x) {
    if (x < 0) fail_at(at, "square root of a negative number (" + to_text(x) + ")");
    return std::sqrt(x);
}

// The n-th root. Odd roots of negative numbers are real: ∛−8 = −2.
inline double root(int at, double x, double n) {
    if (n == 0) fail_at(at, "the 0th root is undefined");
    if (x < 0) {
        const bool oddWhole = n == std::trunc(n) && std::fmod(std::abs(n), 2.0) == 1.0;
        if (!oddWhole) {
            fail_at(at, "root of a negative number (" + to_text(x) + ")");
        }
        return -std::pow(-x, 1.0 / n);
    }
    return std::pow(x, 1.0 / n);
}

// A value stored in a variable declared in ℕ.
inline Int natural(int at, Int value, const char* name) {
    if (value < 0) {
        fail_at(at, std::string(name) + " is in ℕ, so it can't be " + to_text(value));
    }
    return value;
}

// Equal up to rounding: within a relative 1e-9 (or 1e-12 absolute near 0).
inline bool approx(double a, double b) {
    if (a == b) return true;
    const double scale = std::max(std::abs(a), std::abs(b));
    return std::abs(a - b) <= std::max(1e-9 * scale, 1e-12);
}

// --- Functions -------------------------------------------------------------------------------

inline double sin(int, double x) { return std::sin(x); }
inline double cos(int, double x) { return std::cos(x); }
inline double tan(int, double x) { return std::tan(x); }
inline double arcsin(int at, double x) {
    if (x < -1 || x > 1) fail_at(at, "arcsin needs a number from −1 to 1, not " + to_text(x));
    return std::asin(x);
}
inline double arccos(int at, double x) {
    if (x < -1 || x > 1) fail_at(at, "arccos needs a number from −1 to 1, not " + to_text(x));
    return std::acos(x);
}
inline double arctan(int, double x) { return std::atan(x); }
inline double sinh(int, double x) { return std::sinh(x); }
inline double cosh(int, double x) { return std::cosh(x); }
inline double tanh(int, double x) { return std::tanh(x); }
inline double exp(int, double x) { return std::exp(x); }

inline double logarithm(int at, double x, const char* name, double (*log)(double)) {
    if (x <= 0) fail_at(at, std::string(name) + " needs a positive number, not " + to_text(x));
    return log(x);
}
inline double ln(int at, double x) { return logarithm(at, x, "ln", std::log); }
inline double log10(int at, double x) { return logarithm(at, x, "log10", std::log10); }
inline double log2(int at, double x) { return logarithm(at, x, "log2", std::log2); }

// ℝ -> ℤ, failing if the value doesn't fit.
inline Int whole(int at, double x) {
    if (!(x >= -9.2233720368547758e18 && x < 9.2233720368547758e18)) {
        fail_at(at, to_text(x) + " is too big for ℤ");
    }
    return static_cast<Int>(x);
}
inline Int floor(int at, double x) { return whole(at, std::floor(x)); }
inline Int ceil(int at, double x) { return whole(at, std::ceil(x)); }
inline Int round(int at, double x) { return whole(at, std::round(x)); }
inline Int sign(int, double x) { return (x > 0) - (x < 0); }

inline Int abs(int at, Int x) { return x < 0 ? neg(at, x) : x; }
inline double abs(int, double x) { return std::abs(x); }

template <class T, class... Rest>
T min(int at, T first, Rest... rest) {
    if constexpr (sizeof...(rest) == 0) {
        return first;
    } else {
        const T other = min(at, rest...);
        return other < first ? other : first;
    }
}
template <class T, class... Rest>
T max(int at, T first, Rest... rest) {
    if constexpr (sizeof...(rest) == 0) {
        return first;
    } else {
        const T other = max(at, rest...);
        return other > first ? other : first;
    }
}

}  // namespace mpp
