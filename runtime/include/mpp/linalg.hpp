#pragma once

// Vectors (ℝⁿ), matrices (ℝ^{n×m}) and families (α_1, α_2, …). Sizes are
// checked here, when the program runs, with errors in the user's terms.
// Indices are 1-based.

#include "control.hpp"
#include "core.hpp"
#include "print.hpp"
#include "scalar.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace mpp {

using Int = std::int64_t;
using Vector = Eigen::VectorXd;
using Matrix = Eigen::MatrixXd;

// --- Making them ----------------------------------------------------------------------

inline Vector vector(std::initializer_list<double> values) {
    Vector v(static_cast<Eigen::Index>(values.size()));
    Eigen::Index i = 0;
    for (double value : values) v(i++) = value;
    return v;
}

// `values` row by row.
inline Matrix matrix(Int rows, Int columns, std::initializer_list<double> values) {
    Matrix m(rows, columns);
    auto value = values.begin();
    for (Int r = 0; r < rows; ++r)
        for (Int c = 0; c < columns; ++c) m(r, c) = *value++;
    return m;
}

inline Eigen::Index size_of(int at, Int n) {
    if (n < 0) fail_at(at, "a size can't be negative (" + std::to_string(n) + ")");
    if (n > 100000000) fail_at(at, "this size is too big (" + std::to_string(n) + ")");
    return static_cast<Eigen::Index>(n);
}

inline Matrix identity(int at, Int n) {
    const auto size = size_of(at, n);
    return Matrix::Identity(size, size);
}
inline Vector zeros(int at, Int n) { return Vector::Zero(size_of(at, n)); }
inline Matrix zeros(int at, Int rows, Int columns) {
    return Matrix::Zero(size_of(at, rows), size_of(at, columns));
}
inline Vector ones(int at, Int n) { return Vector::Ones(size_of(at, n)); }
inline Matrix ones(int at, Int rows, Int columns) {
    return Matrix::Ones(size_of(at, rows), size_of(at, columns));
}

// --- Sizes ------------------------------------------------------------------------------

inline std::string size_text(const Vector& v) { return std::to_string(v.size()) + " entries"; }
inline std::string size_text(const Matrix& m) {
    return std::to_string(m.rows()) + "×" + std::to_string(m.cols());
}

inline Int length(int, const Vector& v) { return v.size(); }
inline Int rows(int, const Matrix& m) { return m.rows(); }
inline Int columns(int, const Matrix& m) { return m.cols(); }

// A value stored in a variable declared with a size (v ∈ ℝ^3).
inline Vector sized(int at, Vector v, Int n, const char* name) {
    if (v.size() != n) {
        fail_at(at, std::string(name) + " has " + std::to_string(n) + " entries, but this has " +
                        std::to_string(v.size()));
    }
    return v;
}
inline Matrix sized(int at, Matrix m, Int rows, Int columns, const char* name) {
    if (m.rows() != rows || m.cols() != columns) {
        fail_at(at, std::string(name) + " is " + std::to_string(rows) + "×" +
                        std::to_string(columns) + ", but this is " + size_text(m));
    }
    return m;
}

inline void same_size(int at, const Vector& a, const Vector& b, const char* what) {
    if (a.size() != b.size()) {
        fail_at(at, std::string("can't ") + what + " vectors of different lengths (" +
                        std::to_string(a.size()) + " and " + std::to_string(b.size()) + ")");
    }
}
inline void same_size(int at, const Matrix& a, const Matrix& b, const char* what) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) {
        fail_at(at, std::string("can't ") + what + " a " + size_text(a) + " matrix and a " +
                        size_text(b) + " matrix");
    }
}
inline void square(int at, const Matrix& m, const char* what) {
    if (m.rows() != m.cols()) {
        fail_at(at, std::string(what) + " needs a square matrix, not " + size_text(m));
    }
}

// --- Arithmetic ---------------------------------------------------------------------------

template <class T>
T add(int at, const T& a, const T& b) {
    same_size(at, a, b, "add");
    return a + b;
}
template <class T>
T sub(int at, const T& a, const T& b) {
    same_size(at, a, b, "subtract");
    return a - b;
}
template <class T>
T hadamard(int at, const T& a, const T& b) {
    same_size(at, a, b, "multiply entry by entry (⊙)");
    return a.cwiseProduct(b);
}

inline double dot(int at, const Vector& a, const Vector& b) {
    same_size(at, a, b, "take the dot product of");
    return a.dot(b);
}
inline Vector cross(int at, const Vector& a, const Vector& b) {
    if (a.size() != 3 || b.size() != 3) {
        fail_at(at, "the cross product (×) needs two vectors with 3 entries");
    }
    return a.head<3>().cross(b.head<3>());
}
inline Matrix mul(int at, const Matrix& a, const Matrix& b) {
    if (a.cols() != b.rows()) {
        fail_at(at, "can't multiply a " + size_text(a) + " matrix by a " + size_text(b) +
                        " matrix: the first needs as many columns as the second has rows");
    }
    return a * b;
}
inline Vector mul(int at, const Matrix& a, const Vector& v) {
    if (a.cols() != v.size()) {
        fail_at(at, "can't multiply a " + size_text(a) + " matrix by a vector with " +
                        std::to_string(v.size()) + " entries");
    }
    return a * v;
}
template <class T>
T scale(double s, const T& a) {
    return s * a;
}
template <class T>
T divide(int at, const T& a, double s) {
    if (s == 0) fail_at(at, "division by zero");
    return a / s;
}

inline Matrix transpose(const Vector& v) { return v.transpose(); }
inline Matrix transpose(const Matrix& m) { return m.transpose(); }
inline Matrix transpose(int, const Vector& v) { return v.transpose(); }
inline Matrix transpose(int, const Matrix& m) { return m.transpose(); }

inline Matrix inverse(int at, const Matrix& m) {
    square(at, m, "the inverse");
    Eigen::FullPivLU<Matrix> lu(m);
    if (!lu.isInvertible()) fail_at(at, "this matrix has no inverse (its determinant is 0)");
    return lu.inverse();
}
inline Matrix pow(int at, const Matrix& m, Int n) {
    square(at, m, "a power of a matrix");
    Matrix base = n < 0 ? inverse(at, m) : m;
    unsigned long long k = n < 0 ? 0ULL - static_cast<unsigned long long>(n)
                                 : static_cast<unsigned long long>(n);
    Matrix result = Matrix::Identity(m.rows(), m.cols());
    while (k > 0) {
        if (k & 1) result = result * base;
        base = base * base;
        k >>= 1;
    }
    return result;
}

// A ∖ b: the x with A·x = b.
template <class T>
T solve(int at, const Matrix& a, const T& b) {
    if (a.rows() != b.rows()) {
        fail_at(at, "can't solve: the matrix has " + std::to_string(a.rows()) +
                        " rows but the right side has " + std::to_string(b.rows()));
    }
    if (a.rows() == a.cols()) {
        Eigen::FullPivLU<Matrix> lu(a);
        if (!lu.isInvertible()) {
            fail_at(at, "can't solve: the matrix is singular (its determinant is 0)");
        }
        return lu.solve(b);
    }
    // Not square: the least-squares solution.
    return a.colPivHouseholderQr().solve(b);
}

inline double det(int at, const Matrix& m) {
    square(at, m, "the determinant");
    return m.determinant();
}
inline double trace(int at, const Matrix& m) {
    square(at, m, "the trace");
    return m.trace();
}
inline Int rank(int, const Matrix& m) { return Eigen::FullPivLU<Matrix>(m).rank(); }
inline double norm(int, const Vector& v) { return v.norm(); }
inline double norm(int, const Matrix& m) { return m.norm(); }
inline double norm(int, double x) { return std::abs(x); }

// Real eigenvalues, smallest first, and their eigenvectors as columns.
inline Eigen::EigenSolver<Matrix> eigen(int at, const Matrix& m, bool vectors) {
    square(at, m, "eigenvalues");
    Eigen::EigenSolver<Matrix> solver(m, vectors);
    if (solver.info() != Eigen::Success) fail_at(at, "couldn't find the eigenvalues");
    const double scale = std::max(1.0, m.norm());
    for (Eigen::Index i = 0; i < m.rows(); ++i) {
        if (std::abs(solver.eigenvalues()(i).imag()) > 1e-9 * scale) {
            fail_at(at, "this matrix has complex eigenvalues, which M++ can't show yet");
        }
    }
    return solver;
}
inline std::vector<Eigen::Index> eigen_order(const Eigen::EigenSolver<Matrix>& solver) {
    std::vector<Eigen::Index> order(static_cast<std::size_t>(solver.eigenvalues().size()));
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<Eigen::Index>(i);
    std::sort(order.begin(), order.end(), [&](Eigen::Index a, Eigen::Index b) {
        return solver.eigenvalues()(a).real() < solver.eigenvalues()(b).real();
    });
    return order;
}
inline Vector eigenvalues(int at, const Matrix& m) {
    const auto solver = eigen(at, m, false);
    const auto order = eigen_order(solver);
    Vector values(m.rows());
    for (std::size_t i = 0; i < order.size(); ++i) {
        values(static_cast<Eigen::Index>(i)) = solver.eigenvalues()(order[i]).real();
    }
    return values;
}
inline Matrix eigenvectors(int at, const Matrix& m) {
    const auto solver = eigen(at, m, true);
    const auto order = eigen_order(solver);
    Matrix vectors(m.rows(), m.cols());
    for (std::size_t i = 0; i < order.size(); ++i) {
        Vector column = solver.eigenvectors().col(order[i]).real();
        // Unit length, first non-zero entry positive, so results are stable.
        for (Eigen::Index k = 0; k < column.size(); ++k) {
            if (std::abs(column(k)) > 1e-12) {
                if (column(k) < 0) column = -column;
                break;
            }
        }
        vectors.col(static_cast<Eigen::Index>(i)) = column.normalized();
    }
    return vectors;
}

// --- Comparing ------------------------------------------------------------------------------

template <class T>
bool equal(const T& a, const T& b) {
    return a.rows() == b.rows() && a.cols() == b.cols() && a == b;
}
inline bool approx(const Vector& a, const Vector& b) {
    if (a.size() != b.size()) return false;
    for (Eigen::Index i = 0; i < a.size(); ++i)
        if (!approx(a(i), b(i))) return false;
    return true;
}
inline bool approx(const Matrix& a, const Matrix& b) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) return false;
    for (Eigen::Index i = 0; i < a.size(); ++i)
        if (!approx(a(i), b(i))) return false;
    return true;
}

// --- Entries and slices -------------------------------------------------------------------

// * in a subscript.
struct All {};
inline constexpr All all{};

inline Eigen::Index position(int at, Int i, Eigen::Index count, const char* what) {
    if (i < 1 || i > count) {
        fail_at(at, std::string(what) + " " + std::to_string(i) + " is outside 1.." +
                        std::to_string(count));
    }
    return static_cast<Eigen::Index>(i - 1);
}

// The positions a selector picks: all of them, or a range.
inline std::vector<Eigen::Index> positions(int at, All, Eigen::Index count, const char*) {
    std::vector<Eigen::Index> picked;
    for (Eigen::Index i = 0; i < count; ++i) picked.push_back(i);
    return picked;
}
inline std::vector<Eigen::Index> positions(int at, const Range<Int>& range, Eigen::Index count,
                                           const char* what) {
    std::vector<Eigen::Index> picked;
    for (Int i : range) picked.push_back(position(at, i, count, what));
    return picked;
}

inline double& at(int loc, Vector& v, Int i) { return v(position(loc, i, v.size(), "index")); }
inline double at(int loc, const Vector& v, Int i) {
    return v(position(loc, i, v.size(), "index"));
}
inline double& at(int loc, Matrix& m, Int i, Int j) {
    return m(position(loc, i, m.rows(), "row"), position(loc, j, m.cols(), "column"));
}
inline double at(int loc, const Matrix& m, Int i, Int j) {
    return m(position(loc, i, m.rows(), "row"), position(loc, j, m.cols(), "column"));
}

template <class Selector>
Vector at(int loc, const Vector& v, const Selector& selector) {
    const auto picked = positions(loc, selector, v.size(), "index");
    Vector result(static_cast<Eigen::Index>(picked.size()));
    for (std::size_t k = 0; k < picked.size(); ++k) result(static_cast<Eigen::Index>(k)) = v(picked[k]);
    return result;
}
// Row i (as a vector), or part of it.
template <class Selector>
Vector at(int loc, const Matrix& m, Int i, const Selector& columns) {
    const Eigen::Index row = position(loc, i, m.rows(), "row");
    const auto picked = positions(loc, columns, m.cols(), "column");
    Vector result(static_cast<Eigen::Index>(picked.size()));
    for (std::size_t k = 0; k < picked.size(); ++k) result(static_cast<Eigen::Index>(k)) = m(row, picked[k]);
    return result;
}
// Column j, or part of it.
template <class Selector>
Vector at(int loc, const Matrix& m, const Selector& rows, Int j) {
    const Eigen::Index column = position(loc, j, m.cols(), "column");
    const auto picked = positions(loc, rows, m.rows(), "row");
    Vector result(static_cast<Eigen::Index>(picked.size()));
    for (std::size_t k = 0; k < picked.size(); ++k) result(static_cast<Eigen::Index>(k)) = m(picked[k], column);
    return result;
}
template <class Rows, class Columns>
Matrix at(int loc, const Matrix& m, const Rows& rows, const Columns& columns) {
    const auto pickedRows = positions(loc, rows, m.rows(), "row");
    const auto pickedColumns = positions(loc, columns, m.cols(), "column");
    Matrix result(static_cast<Eigen::Index>(pickedRows.size()),
                  static_cast<Eigen::Index>(pickedColumns.size()));
    for (std::size_t r = 0; r < pickedRows.size(); ++r)
        for (std::size_t c = 0; c < pickedColumns.size(); ++c)
            result(static_cast<Eigen::Index>(r), static_cast<Eigen::Index>(c)) =
                m(pickedRows[r], pickedColumns[c]);
    return result;
}

// --- Families -------------------------------------------------------------------------------

// α_1 := 1 defines an element; reading one that has no value is an error.
template <class T>
struct Family {
    std::map<Int, T> values;

    std::string element(const char* name, Int i) const {
        return std::string(name) + "_" + std::to_string(i);
    }
    const T& get(int at, Int i, const char* name) const {
        const auto found = values.find(i);
        if (found == values.end()) fail_at(at, element(name, i) + " hasn't been given a value");
        return found->second;
    }
    void define(int at, Int i, T value, const char* name) {
        if (!values.emplace(i, std::move(value)).second) {
            fail_at(at, element(name, i) + " already has a value; to change it, write " +
                            element(name, i) + " ← …");
        }
    }
    void set(int at, Int i, T value, const char* name) {
        const auto found = values.find(i);
        if (found == values.end()) {
            fail_at(at, element(name, i) + " hasn't been defined yet; write " + element(name, i) +
                            " := …");
        }
        found->second = std::move(value);
    }
    // The whole family as a vector, if it is numbered 1, 2, …, n.
    Vector vector(int at, const char* name) const {
        Vector v(static_cast<Eigen::Index>(values.size()));
        Int expected = 1;
        for (const auto& [index, value] : values) {
            if (index != expected) {
                fail_at(at, std::string(name) + " isn't numbered 1, 2, …, " +
                                std::to_string(values.size()) +
                                ", so it can't be used as a vector (it has no " + element(name, expected) +
                                ")");
            }
            v(static_cast<Eigen::Index>(expected - 1)) = static_cast<double>(value);
            ++expected;
        }
        return v;
    }
};

}  // namespace mpp
