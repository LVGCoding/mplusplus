#include "Builtins.h"

#include <array>

namespace lang {

namespace {
using R = BuiltinFunction::Result;

// The runtime (runtime/include/mpp/scalar.hpp) has a function of the same
// name in namespace mpp for each of these, except print.
constexpr std::array<BuiltinFunction, 37> FUNCTIONS = {{
    {"print", 0, -1, R::Void},
    {"sin"}, {"cos"}, {"tan"},
    {"arcsin"}, {"arccos"}, {"arctan"},
    {"sinh"}, {"cosh"}, {"tanh"},
    {"exp"}, {"ln"}, {"log10"}, {"log2"},
    {"sqrt"},
    {"abs", 1, 1, R::Numeric},
    {"floor", 1, 1, R::Int},
    {"ceil", 1, 1, R::Int},
    {"round", 1, 1, R::Int},
    {"sign", 1, 1, R::Int},
    {"min", 2, -1, R::Numeric},
    {"max", 2, -1, R::Numeric},
    // Vectors and matrices.
    {"det", 1, 1, R::Special},
    {"trace", 1, 1, R::Special},
    {"rank", 1, 1, R::Special},
    {"inverse", 1, 1, R::Special},
    {"transpose", 1, 1, R::Special},
    {"solve", 2, 2, R::Special},
    {"eigenvalues", 1, 1, R::Special},
    {"eigenvectors", 1, 1, R::Special},
    {"norm", 1, 1, R::Special},
    {"length", 1, 1, R::Special},
    {"rows", 1, 1, R::Special},
    {"columns", 1, 1, R::Special},
    {"identity", 1, 1, R::Special},
    {"zeros", 1, 2, R::Special},
    {"ones", 1, 2, R::Special},
}};
}  // namespace

std::span<const BuiltinFunction> BuiltinFunctions() {
    return FUNCTIONS;
}

const BuiltinFunction* FindBuiltinFunction(std::string_view name) {
    for (const BuiltinFunction& function : FUNCTIONS) {
        if (function.name == name) {
            return &function;
        }
    }
    return nullptr;
}

}  // namespace lang
