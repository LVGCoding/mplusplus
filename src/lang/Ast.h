#pragma once

// The syntax tree the parser builds, annotated with types by the checker.

#include "Source.h"

#include <memory>
#include <string>
#include <vector>

namespace lang {

// Types of values. Error marks an expression that already has a reported
// problem, so one mistake doesn't cause a cascade of messages.
enum class Type { Error, Void, Int, Real, Bool, String };

// How a type is written in messages: ℤ, ℝ, 𝔹, Σ*.
const char* TypeName(Type type);

struct Expr {
    enum class Kind {
        Number,   // text
        Bool,     // text: "true" / "false"
        String,   // text
        Infinity, // ∞
        Constant, // a built-in constant: text "π" or "e"
        Name,     // text as written ("α"), ident for C++ ("alpha")
        Unary,    // op ("−", "+", "¬"), args {operand}
        Binary,   // op ("+", "−", "⋅", "×", "/", "mod", "∧", "∨", "⊕"), args {left, right}
        Fraction, // args {numerator, denominator}
        Power,    // args {base, exponent}
        Root,     // args {argument} or {argument, degree}
        Compare,  // args {a, b, c, ...}, ops between them: a < b ≤ c
        Call,     // text: function name, args
        Range,    // args {first, last} or {first, step, last}: a..b, a..s..b
        Cases,    // args: the values; conditions: one per value, null for "otherwise"
    };

    Kind kind;
    SourceRef ref;
    std::string text;
    std::string ident;
    bool greek = false;
    std::string op;
    std::vector<std::string> ops;
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::unique_ptr<Expr>> conditions;
    // Where an operator itself is, e.g. the "/" in a / b.
    SourceRef opRef;

    // Set by the checker.
    Type type = Type::Error;
    // For names: the variable must stay ≥ 0 (declared in ℕ).
    bool natural = false;
    // For calls of functions defined in the document: the C++ function for
    // these argument types (see FunctionInstance).
    std::string function;
};

struct Stmt;
using Block = std::vector<std::unique_ptr<Stmt>>;

// A parameter of a function definition: f(x ∈ ℝ, n).
struct Param {
    std::string name;
    std::string ident;
    bool greek = false;
    SourceRef ref;
    // The declared set ("ℝ"), or empty to take the argument's type.
    std::string declaredType;
    SourceRef typeRef;
};

// A function is checked once for each combination of argument types it is
// called with, so f(x) := x² gives ℤ for f(2) and ℝ for f(2.5). Each
// combination gets its own copy of the body and its own C++ function.
struct FunctionInstance {
    std::vector<Type> params;
    std::vector<bool> naturalParams;
    Type result = Type::Error;
    bool naturalResult = false;
    std::string cppName;
    // A copy of the definition's value or body, checked for these types.
    std::unique_ptr<Expr> value;
    Block body;
};

struct Stmt {
    enum class Kind {
        Define,    // name := value, or name ∈ T := value, or name ∈ T
        Assign,    // name ← value
        Expr,      // value (a call such as print(...))
        If,        // if value { body } else { elseBody }; else-if is an If in elseBody
        While,     // while value { body }
        For,       // for name ∈ value { body }; value is a Range
        Break,
        Continue,
        Return,    // return [value]
        Function,  // name(params) [→ declaredType] := value, or name(params) { body }
    };

    Kind kind;
    SourceRef ref;

    // Define, Assign, For, Function: the variable or function name.
    std::string name;
    std::string ident;
    bool greek = false;
    SourceRef nameRef;

    // Define: the declared type (∈ T), as written ("ℝ"), or empty if
    // inferred. Function: the declared result type (→ T).
    std::string declaredType;
    SourceRef typeRef;

    // Define: null for a declaration without a value (x ∈ ℝ). If/While: the
    // condition. For: the range. Return: null for a bare return.
    std::unique_ptr<Expr> value;

    Block body;
    Block elseBody;
    bool hasElse = false;

    // Function.
    std::vector<Param> params;
    std::vector<std::unique_ptr<FunctionInstance>> instances;

    // Set by the checker: the variable's type, and whether it is in ℕ.
    Type type = Type::Error;
    bool natural = false;
};

struct Program {
    Block statements;
};

// Deep copies, without anything the checker set.
std::unique_ptr<Expr> Clone(const Expr& expr);
std::unique_ptr<Stmt> Clone(const Stmt& statement);
Block Clone(const Block& block);

}  // namespace lang
