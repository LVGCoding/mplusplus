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
    };

    Kind kind;
    SourceRef ref;
    std::string text;
    std::string ident;
    bool greek = false;
    std::string op;
    std::vector<std::string> ops;
    std::vector<std::unique_ptr<Expr>> args;
    // Where an operator itself is, e.g. the "/" in a / b.
    SourceRef opRef;

    // Set by the checker.
    Type type = Type::Error;
    // For names: the variable must stay ≥ 0 (declared in ℕ).
    bool natural = false;
};

struct Stmt {
    enum class Kind {
        Define,   // name := value, or name ∈ T := value, or name ∈ T
        Assign,   // name ← value
        Expr,     // value (a call such as print(...))
    };

    Kind kind;
    SourceRef ref;

    // Define and Assign: the variable.
    std::string name;
    std::string ident;
    bool greek = false;
    SourceRef nameRef;

    // Define: the declared type (∈ T), as written ("ℝ"), or empty if inferred.
    std::string declaredType;
    SourceRef typeRef;

    // Null for a declaration without a value (x ∈ ℝ).
    std::unique_ptr<Expr> value;

    // Set by the checker: the variable's type, and whether it is in ℕ.
    Type type = Type::Error;
    bool natural = false;
};

struct Program {
    std::vector<std::unique_ptr<Stmt>> statements;
};

}  // namespace lang
