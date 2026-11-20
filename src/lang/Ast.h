#pragma once

// The syntax tree the parser builds, annotated with types by the checker.

#include "Source.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lang {

struct TypeInfo;

// Types of values. Error marks an expression that already has a reported
// problem, so one mistake doesn't cause a cascade of messages.
// Vector is a column of reals (ℝⁿ), Matrix a grid of reals (ℝ^{n×m}); their
// sizes are checked when the program runs. Sets hold whole numbers, reals or
// strings; EmptySet is ∅ (or {}) before it is known what it will hold.
// Struct, List and Map have details in `info` (see TypeInfo).
//
// == compares kinds only (`type == Type::Int`); SameType compares the
// details too.
struct Type {
    enum Kind {
        Error, Void, Int, Real, Bool, String, Vector, Matrix,
        SetInt, SetReal, SetString, EmptySet,
        Struct, List, Map,
    };
    Kind kind = Error;
    std::shared_ptr<const TypeInfo> info;

    Type(Kind kind = Error) : kind(kind) {}
    Type(Kind kind, std::shared_ptr<const TypeInfo> info) : kind(kind), info(std::move(info)) {}
    bool operator==(Kind other) const { return kind == other; }
    bool operator==(const Type& other) const { return kind == other.kind; }
};

// A @tag(…) on a struct field, such as @label("Name"): its arguments as text.
struct Tag {
    std::string name;
    std::vector<std::string> args;
    SourceRef ref;
};

struct Expr;

struct FieldInfo {
    std::string name;
    std::string ident;
    bool greek = false;
    Type type;
    std::vector<Tag> tags;
    // @default(…): the value of a field a struct value leaves out.
    std::shared_ptr<Expr> defaultValue;

    // The tag's first argument, or "" without it.
    std::string TagValue(const std::string& tag) const;
    bool HasTag(const std::string& tag) const;
};

struct TypeInfo {
    // Struct: its name ("Person", or ⟨name, age⟩ for one without), the C++
    // struct for it, and its fields.
    std::string name;
    std::string cppName;
    std::vector<FieldInfo> fields;
    // List: the elements' type. Map: the values' type, and the keys'.
    Type element;
    Type key;

    const FieldInfo* Field(const std::string& name) const;
};

// Types are the same, details included (structs by their fields).
bool SameType(const Type& a, const Type& b);

// How a type is written in messages: ℤ, ℝ, 𝔹, Σ*, ℝ*, Person.
std::string TypeName(const Type& type);

// A parameter of a function definition: f(x ∈ ℝ, n).
struct Param {
    std::string name;
    std::string ident;
    bool greek = false;
    SourceRef ref;
    // The declared set ("ℝ"), or empty to take the argument's type.
    std::string declaredType;
    SourceRef typeRef;
    // Sizes after the set: ℝ^n has one, ℝ^{n×m} two; null for *.
    std::vector<std::shared_ptr<Expr>> dims;
    // ℝ^{Σ*}: a map from the key set to the declared set.
    std::string mapKey;
    // T*: a list of the declared type.
    bool list = false;
    // {"a", "b"}: one of these values (the declared set is then empty).
    std::shared_ptr<Expr> choices;
    // Struct fields only: @tags after the type.
    std::vector<Tag> tags;
};

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
        Matrix,   // args: the cells row by row; rows, columns; text: the environment
        Index,    // args {base, index...}: x_i, A_{i,j}, v_{2..4}; see `op` below
        All,      // * in a subscript: A_{i,*} is row i
        Transpose,// args {base}: A^⊤
        Set,      // args: the elements, which may be ranges: {1, 3, 5..9}
        EmptySet, // ∅ or {}
        // The next three bind a variable (var…) to each element of a domain
        // (args[0], a set, range or vector) in turn, and use args[1]:
        Builder,  // {args[1] : var ∈ args[0], conditions[0]}; a filter
                  // {x ∈ S : P} has args[1] = x
        BigOp,    // text "sum", "prod", "bigcup", "bigcap": Σ_{var ∈ args[0]} args[1]
        Quantifier, // text "∀" or "∃": ∀ var ∈ args[0] : args[1]
        TypeName, // a built-in set used as a value: text "ℝ", "Σ*", ... (for T*)
        StructType, // ⟨name ∈ Σ*, age ∈ ℝ⟩: fields
        StructValue, // ⟨name = "Bob", age = 4⟩: ops are the field names, args the values
        ListOf,   // args {type}: T*, a new empty list (ℝ*, Person*, ⟨…⟩*)
        List,     // args: the elements: [1, 2, 3]
        Member,   // args {object}: object.text
        Method,   // args {object, arguments...}: object.text(arguments), e.g. L.push(x)
        Template, // "Hi ${name}!": ops are the text parts, args the values between them
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
    // Builder, BigOp, Quantifier: the bound variable.
    std::string var;
    std::string varIdent;
    bool varGreek = false;
    SourceRef varRef;
    // StructType: the fields, declared like parameters.
    std::vector<Param> fields;
    // Matrix: its size. A single column is a vector.
    std::size_t rows = 0;
    std::size_t columns = 0;

    // Set by the checker.
    Type type;
    // For names: the variable must stay ≥ 0 (declared in ℕ).
    bool natural = false;
    // For calls of functions defined in the document: the C++ function for
    // these argument types (see FunctionInstance).
    std::string function;
    // Set by the checker for Index: "family" (an element of a family, α_n),
    // "vector" or "matrix" (an entry or a slice). A label (v_{max}) is turned
    // into a Name instead, and I_n, 0_n, 1_n into calls.
    // For a Name: "family" when it is a whole family, used as a vector.
    std::string access;
};

struct Stmt;
using Block = std::vector<std::unique_ptr<Stmt>>;

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
    // inferred. Function: the declared result type (→ T). With dims for
    // vectors and matrices, as in Param.
    std::string declaredType;
    SourceRef typeRef;
    std::vector<std::shared_ptr<Expr>> dims;
    // As in Param: ℝ^{Σ*} maps, T* lists, {"a", "b"} choices.
    std::string mapKey;
    bool list = false;
    std::shared_ptr<Expr> choices;

    // Assign: a field of the variable (p.name ← …).
    std::string member;
    SourceRef memberRef;

    // Define, Assign: a subscript on the name (x_i := …, A_{i,j} ← …).
    // The checker decides what it is (`access`): "family", "vector",
    // "matrix", or a label, which it turns into part of the name.
    std::vector<std::unique_ptr<Expr>> indices;
    SourceRef indexRef;
    std::string access;

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

// A family of values named by an index, made by defining its elements:
// α_1 := 1, α_2 := 1. Families of numbers hold ℝ.
struct Family {
    std::string name;
    std::string ident;
    bool greek = false;
    Type element = Type::Error;
};

struct Program {
    Block statements;
    // Set by the checker; they exist for the whole program.
    std::vector<Family> families;
    // Every struct type, in an order where each comes after those it uses.
    std::vector<std::shared_ptr<const TypeInfo>> structs;
};

// Deep copies, without anything the checker set.
std::unique_ptr<Expr> Clone(const Expr& expr);
std::unique_ptr<Stmt> Clone(const Stmt& statement);
Block Clone(const Block& block);

}  // namespace lang
