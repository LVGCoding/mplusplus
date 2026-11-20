#pragma once

// Built-in functions and constants.

#include <optional>
#include <span>
#include <string_view>

namespace lang {

struct BuiltinFunction {
    enum class Result {
        Void,     // print: a statement, not a value
        Real,     // sin, ln, ...
        Int,      // floor, ceil, round
        Numeric,  // abs, min, max: ℤ if every argument is ℤ, else ℝ
    };

    std::string_view name;
    int minArgs = 1;
    int maxArgs = 1;  // -1: any number
    Result result = Result::Real;
};

// Every built-in function, e.g. print, sin, ln, floor, min.
std::span<const BuiltinFunction> BuiltinFunctions();
const BuiltinFunction* FindBuiltinFunction(std::string_view name);

}  // namespace lang
