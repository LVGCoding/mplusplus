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
        indent_ = 2;
        Statements(program.statements);
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
    int indent_ = 2;
    std::vector<SourceRef> locations_;
    int temporaries_ = 0;
    // The function instance being generated, for its return statements.
    const FunctionInstance* instance_ = nullptr;
    std::string functionName_;

    std::string Loc(const SourceRef& ref) {
        locations_.push_back(ref);
        return std::to_string(locations_.size() - 1);
    }

    void Line(const std::string& code) {
        body_ += std::string(static_cast<std::size_t>(indent_) * 4, ' ') + code + "\n";
    }

    // --- Statements ---------------------------------------------------------------

    void Statements(const Block& block) {
        for (const auto& statement : block) {
            Statement(*statement);
        }
    }

    // `code` { block }
    void Braced(const std::string& code, const Block& block) {
        Line(code + " {");
        ++indent_;
        Statements(block);
        --indent_;
    }

    // `value` converted to `type` (ℤ to ℝ) and checked if it must be in ℕ.
    std::string Converted(const Expr& value, Type type, bool natural, const std::string& name) {
        std::string code = type == Type::Real ? Real(value) : Expression(value);
        if (natural) {
            code = "mpp::natural(" + Loc(value.ref) + ", " + code + ", " + Quote(name) + ")";
        }
        return code;
    }

    void Statement(const Stmt& statement) {
        if (statement.kind == Stmt::Kind::Function) {
            Function(statement);
            return;
        }
        Line("mpp::where = " + Loc(statement.ref) + ";");
        const std::string name = Variable(statement.ident, statement.greek);
        switch (statement.kind) {
            case Stmt::Kind::Define:
                if (statement.value) {
                    Line(std::string(CppType(statement.type)) + " " + name + " = " +
                         Converted(*statement.value, statement.type, statement.natural,
                                   statement.name) +
                         ";");
                } else {
                    Line(std::string(CppType(statement.type)) + " " + name + "{};");
                }
                break;
            case Stmt::Kind::Assign:
                Line(name + " = " +
                     Converted(*statement.value, statement.type, statement.natural,
                               statement.name) +
                     ";");
                break;
            case Stmt::Kind::Expr:
                Line("(void)" + Expression(*statement.value) + ";");
                break;
            case Stmt::Kind::If: If(statement); break;
            case Stmt::Kind::While:
                Braced("while (" + Expression(*statement.value) + ")", statement.body);
                Line("}");
                break;
            case Stmt::Kind::For: {
                const Expr& range = *statement.value;
                const bool real = range.type == Type::Real;
                std::string code = "mpp::range(" + Loc(range.ref);
                for (const auto& bound : range.args) {
                    code += ", " + (real ? Real(*bound) : Expression(*bound));
                }
                Braced("for (const " + std::string(CppType(statement.type)) + " " + name + " : " +
                           code + "))",
                       statement.body);
                Line("}");
                break;
            }
            case Stmt::Kind::Break: Line("break;"); break;
            case Stmt::Kind::Continue: Line("continue;"); break;
            case Stmt::Kind::Return:
                if (!statement.value) {
                    Line("return;");
                } else {
                    Line("return " +
                         Converted(*statement.value, instance_->result, instance_->naturalResult,
                                   functionName_) +
                         ";");
                }
                break;
            case Stmt::Kind::Function: break;
        }
    }

    // if (…) { … } else if (…) { … } else { … }
    void If(const Stmt& statement) {
        Braced("if (" + Expression(*statement.value) + ")", statement.body);
        const Stmt* current = &statement;
        while (current->hasElse) {
            if (current->elseBody.size() == 1 && current->elseBody[0]->kind == Stmt::Kind::If) {
                const Stmt& next = *current->elseBody[0];
                Braced("} else if (" + Expression(*next.value) + ")", next.body);
                current = &next;
            } else {
                Braced("} else", current->elseBody);
                break;
            }
        }
        Line("}");
    }

    // Each instance becomes a std::function, all declared before any is
    // defined so they can call each other and themselves.
    void Function(const Stmt& definition) {
        for (const auto& instance : definition.instances) {
            std::string signature = std::string(ResultType(instance->result)) + "(";
            for (std::size_t i = 0; i < instance->params.size(); ++i) {
                signature += std::string(i > 0 ? ", " : "") + CppType(instance->params[i]);
            }
            Line("std::function<" + signature + ")> " + instance->cppName + ";");
        }
        for (const auto& instance : definition.instances) {
            std::string parameters;
            for (std::size_t i = 0; i < instance->params.size(); ++i) {
                const Param& param = definition.params[i];
                parameters += std::string(i > 0 ? ", " : "") + CppType(instance->params[i]) +
                              " " + Variable(param.ident, param.greek);
            }
            Line(instance->cppName + " = [&](" + parameters + ") -> " +
                 ResultType(instance->result) + " {");
            ++indent_;
            const FunctionInstance* saved = instance_;
            instance_ = instance.get();
            functionName_ = definition.name;
            Line("const mpp::Call call(" + Loc(definition.nameRef) + ", " +
                 Quote(definition.name) + ");");
            for (std::size_t i = 0; i < instance->params.size(); ++i) {
                if (instance->naturalParams[i]) {
                    const Param& param = definition.params[i];
                    const std::string name = Variable(param.ident, param.greek);
                    Line(name + " = mpp::natural(" + Loc(param.ref) + ", " + name + ", " +
                         Quote(param.name) + ");");
                }
            }
            if (instance->value) {
                Line("mpp::where = " + Loc(instance->value->ref) + ";");
                Line("return " +
                     Converted(*instance->value, instance->result, instance->naturalResult,
                               definition.name) +
                     ";");
            } else {
                Statements(instance->body);
                if (instance->result != Type::Void) {
                    Line("mpp::fail_at(" + Loc(definition.nameRef) + ", " +
                         Quote(definition.name + " ended without giving a value: it needs a "
                                                 "return at the end.") +
                         ");");
                }
            }
            instance_ = saved;
            --indent_;
            Line("};");
        }
    }

    static const char* ResultType(Type type) {
        return type == Type::Void ? "void" : CppType(type);
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
            case Expr::Kind::Cases: return Cases(expr);
            case Expr::Kind::Range: return "";
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

    // c₁ ? v₁ : c₂ ? v₂ : (no case applies)
    std::string Cases(const Expr& expr) {
        const std::string type = CppType(expr.type);
        std::string code = "(";
        bool otherwise = false;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            const std::string value = type + "(" + Expression(*expr.args[i]) + ")";
            if (!expr.conditions[i]) {
                code += value;
                otherwise = true;
                break;
            }
            code += Expression(*expr.conditions[i]) + " ? " + value + " : ";
        }
        if (!otherwise) {
            code += "mpp::no_case<" + type + ">(" + Loc(expr.ref) + ")";
        }
        return code + ")";
    }

    std::string Call(const Expr& expr) {
        if (!expr.function.empty()) {
            std::string code = expr.function + "(";
            for (std::size_t i = 0; i < expr.args.size(); ++i) {
                code += std::string(i > 0 ? ", " : "") + Expression(*expr.args[i]);
            }
            return code + ")";
        }
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
