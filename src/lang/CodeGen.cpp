#include "CodeGen.h"

#include "Builtins.h"

#include <cstdio>

namespace lang {

namespace {

const char* CppType(Type type) {
    switch (type) {
        case Type::Int: return "std::int64_t";
        case Type::Real: return "double";
        case Type::Bool: return "bool";
        case Type::String: return "std::string";
        default: return "auto";
    }
}

// A C++ string literal for `text`.
std::string Quote(const std::string& text) {
    std::string quoted = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            quoted += '\\';
            quoted += static_cast<char>(c);
        } else if (c == '\n') {
            quoted += "\\n";
        } else if (c < 0x20) {
            char escape[8];
            std::snprintf(escape, sizeof escape, "\\x%02x\"\"", c);
            quoted += escape;
        } else {
            quoted += static_cast<char>(c);
        }
    }
    return quoted + "\"";
}

std::string Variable(const std::string& ident, bool greek) {
    // Distinct prefixes keep α (g_alpha) apart from a name typed as "alpha"
    // (u_alpha), and keep every name clear of C++ keywords.
    return (greek ? "g_" : "u_") + ident;
}

class Generator {
public:
    GeneratedCode Run(const Program& program) {
        body_ = "";
        for (const auto& statement : program.statements) {
            Statement(*statement);
        }
        GeneratedCode code;
        code.cpp = "#include <mpp/mpp.hpp>\n"
                   "\n"
                   "int main() {\n"
                   "    return mpp::run([&] {\n" +
                   body_ +
                   "    });\n"
                   "}\n";
        code.locations = std::move(locations_);
        return code;
    }

private:
    std::string body_;
    std::vector<SourceRef> locations_;
    int temporaries_ = 0;

    std::string Loc(const SourceRef& ref) {
        locations_.push_back(ref);
        return std::to_string(locations_.size() - 1);
    }

    void Line(const std::string& code) { body_ += "        " + code + "\n"; }

    // --- Statements ---------------------------------------------------------------

    // The value to store in a variable of `type`, checked if it is in ℕ.
    std::string Stored(const Stmt& statement) {
        std::string value = statement.type == Type::Real ? Real(*statement.value)
                                                         : Expression(*statement.value);
        if (statement.natural) {
            value = "mpp::natural(" + Loc(statement.value->ref) + ", " + value + ", " +
                    Quote(statement.name) + ")";
        }
        return value;
    }

    void Statement(const Stmt& statement) {
        Line("mpp::where = " + Loc(statement.ref) + ";");
        const std::string name = Variable(statement.ident, statement.greek);
        switch (statement.kind) {
            case Stmt::Kind::Define:
                if (statement.value) {
                    Line(std::string(CppType(statement.type)) + " " + name + " = " +
                         Stored(statement) + ";");
                } else {
                    Line(std::string(CppType(statement.type)) + " " + name + "{};");
                }
                break;
            case Stmt::Kind::Assign:
                Line(name + " = " + Stored(statement) + ";");
                break;
            case Stmt::Kind::Expr:
                Line(Expression(*statement.value) + ";");
                break;
        }
    }

    // --- Expressions -----------------------------------------------------------------

    // `expr` as a double (ℤ values are converted).
    std::string Real(const Expr& expr) {
        const std::string code = Expression(expr);
        return expr.type == Type::Int ? "double(" + code + ")" : code;
    }

    // Arguments for a runtime function: as doubles if `real`, else as they are.
    std::string Args(const Expr& expr, bool real) {
        std::string code;
        for (const auto& arg : expr.args) {
            code += ", " + (real ? Real(*arg) : Expression(*arg));
        }
        return code;
    }

    std::string Expression(const Expr& expr) {
        switch (expr.kind) {
            case Expr::Kind::Number:
                if (expr.type == Type::Int) {
                    return "std::int64_t(" + expr.text + ")";
                }
                return expr.text.front() == '.' ? "0" + expr.text : expr.text;
            case Expr::Kind::Bool: return expr.text;
            case Expr::Kind::String: return "std::string(" + Quote(expr.text) + ")";
            case Expr::Kind::Infinity: return "mpp::infinity";
            case Expr::Kind::Constant:
                return expr.text == "π" ? "std::numbers::pi" : "std::numbers::e";
            case Expr::Kind::Name: return Variable(expr.ident, expr.greek);
            case Expr::Kind::Unary: {
                const Expr& operand = *expr.args[0];
                if (expr.op == "¬") return "(!" + Expression(operand) + ")";
                if (expr.op == "+") return Expression(operand);
                if (expr.type == Type::Int) {
                    return "mpp::neg(" + Loc(expr.ref) + ", " + Expression(operand) + ")";
                }
                return "(-" + Expression(operand) + ")";
            }
            case Expr::Kind::Binary: return Binary(expr);
            case Expr::Kind::Fraction:
                return "mpp::div(" + Loc(expr.args[1]->ref) + Args(expr, true) + ")";
            case Expr::Kind::Power:
                return "mpp::pow(" + Loc(expr.ref) + Args(expr, expr.type == Type::Real) + ")";
            case Expr::Kind::Root:
                if (expr.args.size() == 1) {
                    return "mpp::sqrt(" + Loc(expr.ref) + Args(expr, true) + ")";
                }
                return "mpp::root(" + Loc(expr.ref) + Args(expr, true) + ")";
            case Expr::Kind::Compare: return Compare(expr);
            case Expr::Kind::Call: return Call(expr);
        }
        return "";
    }

    std::string Binary(const Expr& expr) {
        const std::string& op = expr.op;
        if (op == "∧" || op == "∨" || op == "⊕") {
            const char* cpp = op == "∧" ? " && " : op == "∨" ? " || " : " != ";
            return "(" + Expression(*expr.args[0]) + cpp + Expression(*expr.args[1]) + ")";
        }
        const bool real = expr.type == Type::Real;
        auto side = [&](const Expr& e) { return real ? Real(e) : Expression(e); };
        const std::string a = side(*expr.args[0]);
        const std::string b = side(*expr.args[1]);
        if (op == "/") {
            return "mpp::div(" + Loc(expr.args[1]->ref) + ", " + a + ", " + b + ")";
        }
        if (op == "mod") {
            return "mpp::mod(" + Loc(expr.args[1]->ref) + ", " + a + ", " + b + ")";
        }
        const char* cpp = op == "+" ? "+" : op == "−" ? "-" : "*";
        if (real) {
            return "(" + a + " " + cpp + " " + b + ")";
        }
        // ℤ arithmetic is checked for overflow.
        const char* function = op == "+" ? "add" : op == "−" ? "sub" : "mul";
        return std::string("mpp::") + function + "(" + Loc(expr.ref) + ", " + a + ", " + b + ")";
    }

    static const char* CppComparison(const std::string& op) {
        if (op == "=") return "==";
        if (op == "≠") return "!=";
        if (op == "≤") return "<=";
        if (op == "≥") return ">=";
        return op == "<" ? "<" : ">";
    }

    std::string Comparison(const std::string& op, const std::string& a, const std::string& b) {
        if (op == "≈") {
            return "mpp::approx(" + a + ", " + b + ")";
        }
        return "(" + a + " " + CppComparison(op) + " " + b + ")";
    }

    std::string Compare(const Expr& expr) {
        bool anyReal = false;
        for (const auto& arg : expr.args) {
            anyReal |= arg->type == Type::Real;
        }
        std::vector<std::string> values;
        for (const auto& arg : expr.args) {
            values.push_back(anyReal ? Real(*arg) : Expression(*arg));
        }
        if (expr.ops.size() == 1) {
            return Comparison(expr.ops[0], values[0], values[1]);
        }
        // a < b ≤ c: each value is computed once, and only as far as needed.
        std::string code = "[&] {";
        std::vector<std::string> names;
        for (std::size_t i = 0; i < values.size(); ++i) {
            names.push_back("t" + std::to_string(temporaries_++));
            code += " const auto " + names[i] + " = " + values[i] + ";";
            if (i > 0) {
                const std::string test = Comparison(expr.ops[i - 1], names[i - 1], names[i]);
                if (i + 1 < values.size()) {
                    code += " if (!" + test + ") return false;";
                } else {
                    code += " return " + test + ";";
                }
            }
        }
        return code + " }()";
    }

    std::string Call(const Expr& expr) {
        if (expr.text == "print") {
            std::string code = "mpp::print(";
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                if (i > 0) {
                    code += ", \" \", ";
                }
                code += Expression(*expr.args[i]);
            }
            return code + ")";
        }
        const BuiltinFunction* function = FindBuiltinFunction(expr.text);
        const bool real = function->result != BuiltinFunction::Result::Numeric ||
                          expr.type == Type::Real;
        return "mpp::" + expr.text + "(" + Loc(expr.ref) + Args(expr, real) + ")";
    }
};

}  // namespace

GeneratedCode Generate(const Program& program) {
    return Generator().Run(program);
}

}  // namespace lang
