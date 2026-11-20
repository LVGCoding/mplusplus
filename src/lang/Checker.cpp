#include "Checker.h"

#include "Builtins.h"

#include <charconv>
#include <cstdint>
#include <map>
#include <string>

namespace lang {

const char* TypeName(Type type) {
    switch (type) {
        case Type::Int: return "ℤ";
        case Type::Real: return "ℝ";
        case Type::Bool: return "𝔹";
        case Type::String: return "Σ*";
        case Type::Void: return "nothing";
        case Type::Error: return "?";
    }
    return "?";
}

namespace {

bool IsNumeric(Type type) { return type == Type::Int || type == Type::Real; }

// The type of an arithmetic result: ℝ if either side is ℝ.
Type Join(Type a, Type b) { return a == Type::Real || b == Type::Real ? Type::Real : Type::Int; }

std::string Row(const SourceRef& ref) { return "row " + std::to_string(ref.Row() + 1); }

class Checker {
public:
    explicit Checker(std::vector<Diagnostic>& diagnostics) : diagnostics_(diagnostics) {}

    void CheckProgram(Program& program) {
        // Where each name is first defined, to explain "used before defined".
        for (const auto& statement : program.statements) {
            if (statement->kind == Stmt::Kind::Define) {
                laterDefinitions_.try_emplace(statement->name, statement->nameRef);
            }
        }
        for (auto& statement : program.statements) {
            CheckStatement(*statement);
        }
    }

private:
    struct Variable {
        Type type = Type::Error;
        bool natural = false;
        SourceRef ref;
    };

    std::vector<Diagnostic>& diagnostics_;
    std::map<std::string, Variable> variables_;
    std::map<std::string, SourceRef> laterDefinitions_;

    void Error(std::string message, const SourceRef& ref) {
        diagnostics_.push_back({std::move(message), ref});
    }

    // --- Statements -------------------------------------------------------------

    void CheckStatement(Stmt& statement) {
        switch (statement.kind) {
            case Stmt::Kind::Define: CheckDefine(statement); break;
            case Stmt::Kind::Assign: CheckAssign(statement); break;
            case Stmt::Kind::Expr: CheckExpressionStatement(statement); break;
        }
    }

    // Can a value of type `value` be stored in a variable of type `target`?
    // Reports why not at `ref`.
    bool CheckStore(Type target, Type value, const std::string& name, const char* targetName,
                    const SourceRef& ref) {
        if (target == Type::Error || value == Type::Error || target == value ||
            (target == Type::Real && value == Type::Int)) {
            return true;
        }
        if (value == Type::Void) {
            Error("This doesn't give a value to store in " + name + ".", ref);
        } else if (target == Type::Int && value == Type::Real) {
            Error(name + " is in " + targetName + ", but this is a real number (ℝ). Use "
                  "floor(…), ceil(…) or round(…) to make it whole.", ref);
        } else {
            Error(name + " is in " + targetName + ", but this is in " + TypeName(value) + ".",
                  ref);
        }
        return false;
    }

    void CheckDefine(Stmt& statement) {
        Type type = Type::Error;
        if (!statement.declaredType.empty()) {
            const std::string& set = statement.declaredType;
            if (set == "ℝ") {
                type = Type::Real;
            } else if (set == "ℤ") {
                type = Type::Int;
            } else if (set == "ℕ") {
                type = Type::Int;
                statement.natural = true;
            } else if (set == "𝔹") {
                type = Type::Bool;
            } else if (set == "Σ") {
                Error("For text, define the variable with a string: name := \"text\".",
                      statement.typeRef);
            } else {
                Error(set + " can't be used yet.", statement.typeRef);
            }
        }
        if (statement.value) {
            const Type value = CheckExpr(*statement.value);
            if (statement.declaredType.empty()) {
                type = value;
                if (value == Type::Void) {
                    Error("This doesn't give a value to store in " + statement.name + ".",
                          statement.value->ref);
                    type = Type::Error;
                }
            } else {
                CheckStore(type, value, statement.name, statement.declaredType.c_str(),
                           statement.value->ref);
            }
        }
        statement.type = type;

        if (FindBuiltinFunction(statement.name)) {
            Error(statement.name + " is a built-in function; choose another name.",
                  statement.nameRef);
            return;
        }
        if (auto existing = variables_.find(statement.name); existing != variables_.end()) {
            Error(statement.name + " is already defined (" + Row(existing->second.ref) +
                      "). To change it, write " + statement.name + " ← …",
                  statement.nameRef);
            return;
        }
        variables_[statement.name] = {type, statement.natural, statement.nameRef};
    }

    void CheckAssign(Stmt& statement) {
        const Type value = CheckExpr(*statement.value);
        const auto variable = variables_.find(statement.name);
        if (variable == variables_.end()) {
            if (statement.name == "π" || statement.name == "e") {
                Error(statement.name + " is a constant and can't be changed.", statement.nameRef);
            } else {
                Error(statement.name + " isn't defined yet. To define it, write " +
                          statement.name + " := …",
                      statement.nameRef);
            }
            return;
        }
        statement.type = variable->second.type;
        statement.natural = variable->second.natural;
        CheckStore(statement.type, value, statement.name,
                   statement.natural ? "ℕ" : TypeName(statement.type), statement.value->ref);
    }

    void CheckExpressionStatement(Stmt& statement) {
        Expr& value = *statement.value;
        const std::size_t errorsBefore = diagnostics_.size();
        const Type type = CheckExpr(value, /*statement=*/true);
        if (diagnostics_.size() != errorsBefore || type == Type::Void) {
            return;
        }
        if (value.kind == Expr::Kind::Compare && value.ops.size() == 1 && value.ops[0] == "=" &&
            value.args[0]->kind == Expr::Kind::Name) {
            const std::string& name = value.args[0]->text;
            Error("= compares two values. To define " + name + ", write " + name +
                      " := …, or to change it, " + name + " ← …",
                  value.opRef);
            return;
        }
        Error("This value isn't used. To show it, write print(…).", value.ref);
    }

    // --- Expressions --------------------------------------------------------------

    Type CheckExpr(Expr& expr, bool statement = false) {
        expr.type = Infer(expr, statement);
        return expr.type;
    }

    // Check that every argument is a number; returns false (after reporting
    // it) if one isn't. Arguments with errors already reported pass silently.
    bool RequireNumbers(Expr& expr, const std::string& what) {
        bool ok = true;
        for (auto& arg : expr.args) {
            if (arg->type == Type::Error) {
                ok = false;
            } else if (!IsNumeric(arg->type)) {
                Error(what + " needs numbers, but this is in " + TypeName(arg->type) + ".",
                      arg->ref);
                ok = false;
            }
        }
        return ok;
    }

    Type CheckArgs(Expr& expr) {
        bool error = false;
        for (auto& arg : expr.args) {
            const Type type = CheckExpr(*arg);
            error |= type == Type::Error;
            if (type == Type::Void) {
                Error("This doesn't give a value.", arg->ref);
                arg->type = Type::Error;
                error = true;
            }
        }
        return error ? Type::Error : Type::Void;
    }

    Type Infer(Expr& expr, bool statement) {
        switch (expr.kind) {
            case Expr::Kind::Number: return CheckNumber(expr);
            case Expr::Kind::Bool: return Type::Bool;
            case Expr::Kind::String: return Type::String;
            case Expr::Kind::Infinity:
            case Expr::Kind::Constant: return Type::Real;
            case Expr::Kind::Name: return CheckName(expr);
            case Expr::Kind::Call: return CheckCall(expr, statement);
            default: break;
        }

        if (CheckArgs(expr) == Type::Error) {
            return Type::Error;
        }
        switch (expr.kind) {
            case Expr::Kind::Unary:
                if (expr.op == "¬") {
                    return RequireBool(expr, "¬") ? Type::Bool : Type::Error;
                }
                return RequireNumbers(expr, expr.op) ? expr.args[0]->type : Type::Error;
            case Expr::Kind::Binary: return CheckBinary(expr);
            case Expr::Kind::Fraction:
                return RequireNumbers(expr, "A fraction") ? Type::Real : Type::Error;
            case Expr::Kind::Root:
                return RequireNumbers(expr, "A root") ? Type::Real : Type::Error;
            case Expr::Kind::Power: {
                if (!RequireNumbers(expr, "A power")) {
                    return Type::Error;
                }
                // ℤ to a ℤ power stays whole, unless the exponent is written
                // negative (2^{-1} is ½).
                const Expr& exponent = *expr.args[1];
                const bool negative = exponent.kind == Expr::Kind::Unary && exponent.op == "−";
                if (expr.args[0]->type == Type::Int && exponent.type == Type::Int && !negative) {
                    return Type::Int;
                }
                return Type::Real;
            }
            case Expr::Kind::Compare: return CheckCompare(expr);
            default: return Type::Error;
        }
    }

    bool RequireBool(Expr& expr, const std::string& op) {
        bool ok = true;
        for (auto& arg : expr.args) {
            if (arg->type != Type::Bool) {
                Error(op + " needs true/false values (𝔹), but this is in " +
                          TypeName(arg->type) + ".",
                      arg->ref);
                ok = false;
            }
        }
        return ok;
    }

    Type CheckNumber(Expr& expr) {
        const std::string& text = expr.text;
        if (text.find_first_of(".eE") != std::string::npos) {
            return Type::Real;
        }
        std::int64_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc() || end != text.data() + text.size()) {
            Error("This number is too big for ℤ. Write it with a decimal point (" + text +
                      ".0) to make it a real number.",
                  expr.ref);
            return Type::Error;
        }
        return Type::Int;
    }

    Type CheckName(Expr& expr) {
        if (const auto variable = variables_.find(expr.text); variable != variables_.end()) {
            expr.natural = variable->second.natural;
            return variable->second.type;
        }
        if (expr.text == "π" || expr.text == "e") {
            expr.kind = Expr::Kind::Constant;
            return Type::Real;
        }
        if (FindBuiltinFunction(expr.text)) {
            Error(expr.text + " is a function. Write " + expr.text + "(…) to use it.", expr.ref);
            return Type::Error;
        }
        if (const auto later = laterDefinitions_.find(expr.text); later != laterDefinitions_.end()) {
            Error(expr.text + " is used before it is defined (" + Row(later->second) + ").",
                  expr.ref);
            return Type::Error;
        }
        Error(expr.text + " isn't defined." + MultiplyHint(expr.text), expr.ref);
        return Type::Error;
    }

    // For an unknown name made of known names, e.g. "ab" when a and b exist.
    std::string MultiplyHint(const std::string& name) const {
        if (name.size() < 2) {
            return "";
        }
        std::string product;
        for (char c : name) {
            if (!variables_.contains(std::string(1, c))) {
                return "";
            }
            if (!product.empty()) {
                product += "⋅";
            }
            product += c;
        }
        return " To multiply, write " + product + ".";
    }

    Type CheckBinary(Expr& expr) {
        const std::string& op = expr.op;
        Type left = expr.args[0]->type;
        Type right = expr.args[1]->type;
        if (op == "∧" || op == "∨" || op == "⊕") {
            return RequireBool(expr, op) ? Type::Bool : Type::Error;
        }
        if (op == "⊙") {
            Error("⊙ multiplies vectors and matrices, which are coming in a later version of "
                  "M++. Use ⋅ for numbers.",
                  expr.opRef);
            return Type::Error;
        }
        if (op == "+" && left == Type::String && right == Type::String) {
            Error("Strings can't be joined with + yet. To print several things, write "
                  "print(a, b).",
                  expr.opRef);
            return Type::Error;
        }
        const std::string what = op == "mod" ? "mod" : op;
        if (!RequireNumbers(expr, what)) {
            return Type::Error;
        }
        if (op == "/") {
            return Type::Real;
        }
        return Join(left, right);
    }

    Type CheckCompare(Expr& expr) {
        bool ok = true;
        for (std::size_t i = 0; i < expr.ops.size(); ++i) {
            const std::string& op = expr.ops[i];
            const Type a = expr.args[i]->type;
            const Type b = expr.args[i + 1]->type;
            if (op == "=" || op == "≠") {
                if (!(IsNumeric(a) && IsNumeric(b)) && a != b) {
                    Error("Can't compare " + std::string(TypeName(a)) + " with " +
                              TypeName(b) + ".",
                          Span(expr.args[i]->ref, expr.args[i + 1]->ref));
                    ok = false;
                }
            } else {
                for (const Expr* side : {expr.args[i].get(), expr.args[i + 1].get()}) {
                    if (!IsNumeric(side->type)) {
                        Error(op + " needs numbers, but this is in " +
                                  TypeName(side->type) + ".",
                              side->ref);
                        ok = false;
                    }
                }
            }
        }
        return ok ? Type::Bool : Type::Error;
    }

    Type CheckCall(Expr& expr, bool statement) {
        const BuiltinFunction* function = FindBuiltinFunction(expr.text);
        if (!function) {
            CheckArgs(expr);
            if (variables_.contains(expr.text)) {
                Error(expr.text + " isn't a function. To multiply, write " + expr.text +
                          "⋅(…).",
                      expr.opRef);
            } else {
                Error("There's no function called " + expr.text + ".", expr.opRef);
            }
            return Type::Error;
        }
        const int count = static_cast<int>(expr.args.size());
        if (count < function->minArgs || (function->maxArgs >= 0 && count > function->maxArgs)) {
            std::string expected;
            if (function->minArgs == function->maxArgs) {
                expected = std::to_string(function->minArgs);
            } else if (function->maxArgs < 0) {
                expected = "at least " + std::to_string(function->minArgs);
            } else {
                expected = std::to_string(function->minArgs) + " to " +
                           std::to_string(function->maxArgs);
            }
            Error(expr.text + " takes " + expected +
                      (expected == "1" ? " argument" : " arguments") + ", not " +
                      std::to_string(count) + ".",
                  expr.ref);
            CheckArgs(expr);
            return Type::Error;
        }
        if (CheckArgs(expr) == Type::Error) {
            return function->result == BuiltinFunction::Result::Void ? Type::Void : Type::Error;
        }
        switch (function->result) {
            case BuiltinFunction::Result::Void:
                if (!statement) {
                    Error(expr.text + "(…) doesn't give a value.", expr.ref);
                    return Type::Error;
                }
                return Type::Void;
            case BuiltinFunction::Result::Real:
                return RequireNumbers(expr, expr.text) ? Type::Real : Type::Error;
            case BuiltinFunction::Result::Int:
                return RequireNumbers(expr, expr.text) ? Type::Int : Type::Error;
            case BuiltinFunction::Result::Numeric: {
                if (!RequireNumbers(expr, expr.text)) {
                    return Type::Error;
                }
                Type type = Type::Int;
                for (const auto& arg : expr.args) {
                    type = Join(type, arg->type);
                }
                return type;
            }
        }
        return Type::Error;
    }
};

}  // namespace

void Check(Program& program, std::vector<Diagnostic>& diagnostics) {
    Checker(diagnostics).CheckProgram(program);
}

}  // namespace lang
