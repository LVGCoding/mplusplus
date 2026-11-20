#pragma once

// Finite sets: {1, 2, 3}, ranges used as sets, set-builder results. They are
// kept sorted, so they print and are gone through in order.

#include "control.hpp"
#include "core.hpp"
#include "print.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace mpp {

template <class T>
struct Set {
    std::set<T> items;

    Set() = default;
    explicit Set(std::set<T> values) : items(std::move(values)) {}
    // A set of ℤ used as a set of ℝ.
    template <class U>
        requires(!std::is_same_v<U, T> && std::is_convertible_v<U, T>)
    Set(const Set<U>& other) {
        for (const U& value : other.items) items.insert(static_cast<T>(value));
    }

    auto begin() const { return items.begin(); }
    auto end() const { return items.end(); }
    std::size_t size() const { return items.size(); }
    void insert(T value) { items.insert(std::move(value)); }
    template <class Values>
    void insert_all(const Values& values) {
        for (const auto& value : values) items.insert(static_cast<T>(value));
    }
    bool operator==(const Set&) const = default;
};

// ∅ before it is known what it will hold: becomes an empty set of anything.
struct EmptySet {
    template <class T>
    operator Set<T>() const {
        return {};
    }
};

inline std::string to_text(EmptySet) { return "∅"; }

template <class T>
Set<T> to_set(const Range<T>& range) {
    Set<T> set;
    set.insert_all(range);
    return set;
}

template <class T>
Set<T> set_union(const Set<T>& a, const Set<T>& b) {
    Set<T> result = a;
    result.items.insert(b.items.begin(), b.items.end());
    return result;
}
template <class T>
Set<T> set_intersection(const Set<T>& a, const Set<T>& b) {
    Set<T> result;
    for (const T& value : a.items)
        if (b.items.contains(value)) result.items.insert(value);
    return result;
}
template <class T>
Set<T> set_difference(const Set<T>& a, const Set<T>& b) {
    Set<T> result;
    for (const T& value : a.items)
        if (!b.items.contains(value)) result.items.insert(value);
    return result;
}
template <class T>
Set<T> set_symmetric(const Set<T>& a, const Set<T>& b) {
    return set_union(set_difference(a, b), set_difference(b, a));
}
template <class T>
bool subset(const Set<T>& a, const Set<T>& b) {
    for (const T& value : a.items)
        if (!b.items.contains(value)) return false;
    return true;
}
template <class T>
bool proper_subset(const Set<T>& a, const Set<T>& b) {
    return a.size() < b.size() && subset(a, b);
}

template <class T, class U>
bool contains(const Set<T>& set, const U& value) {
    return set.items.contains(static_cast<T>(value));
}
inline bool contains(const Eigen::VectorXd& v, double value) {
    for (Eigen::Index i = 0; i < v.size(); ++i)
        if (v(i) == value) return true;
    return false;
}

// x ∈ a..b and x ∈ a..s..b, without making the set.
inline bool in_range(std::int64_t x, std::int64_t first, std::int64_t last) {
    return first <= x && x <= last;
}
inline bool in_range(std::int64_t x, std::int64_t first, std::int64_t step, std::int64_t last) {
    if (step == 0) return false;
    const bool inside = step > 0 ? first <= x && x <= last : last <= x && x <= first;
    return inside && (static_cast<__int128>(x) - first) % step == 0;
}
inline bool in_range(double x, double first, double last) { return first <= x && x <= last; }
inline bool in_range(double x, double first, double step, double last) {
    if (step == 0) return false;
    const double steps = (x - first) / step;
    return steps >= -1e-9 && std::abs(steps - std::round(steps)) < 1e-9 &&
           (step > 0 ? x <= last + 1e-9 * std::abs(step) : x >= last - 1e-9 * std::abs(step));
}

template <class T>
std::int64_t size(const Set<T>& set) {
    return static_cast<std::int64_t>(set.size());
}

// {1, 2, 3}; the empty set is ∅.
template <class T>
std::string to_text(const Set<T>& set) {
    if (set.items.empty()) return "∅";
    std::string text = "{";
    bool first = true;
    for (const T& value : set.items) {
        text += (first ? "" : ", ") + to_text(value);
        first = false;
    }
    return text + "}";
}

}  // namespace mpp
