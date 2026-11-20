#include "CodeGen.h"

#include "Builtins.h"

#include <cstdio>

namespace lang {

namespace {

std::string CppType(const Type& type) {
    switch (type.kind) {
        case Type::Struct: return type.info->cppName;
        case Type::List: return "mpp::List<" + CppType(type.info->element) + ">";
        case Type::Map:
            return "mpp::Map<" + CppType(type.info->key) + ", " + CppType(type.info->element) +
                   ">";
        case Type::Int: return "std::int64_t";
        case Type::Real: return "double";
        case Type::Bool: return "bool";
        case Type::String: return "std::string";
        case Type::Vector: return "mpp::Vector";
        case Type::Matrix: return "mpp::Matrix";
        case Type::SetInt:
        case Type::EmptySet: return "mpp::Set<std::int64_t>";
        case Type::SetReal: return "mpp::Set<double>";
        case Type::SetString: return "mpp::Set<std::string>";
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
        for (const auto& info : program.structs) {
            StructDefinition(*info);
        }
        // Families exist for the whole program.
        for (const Family& family : program.families) {
            Line("mpp::Family<" + std::string(CppType(family.element)) + "> " +
                 Variable(family.ident, family.greek) + ";");
        }
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
        std::string code = As(value, type);
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
                if (statement.access == "type") {
                    break;  // a struct type, written at the start
                }
                if (statement.access == "map" || statement.access == "list") {
                    ElementAssign(statement, name);
                    break;
                }
                if (statement.choices && !statement.value) {
                    // x ∈ {"a", "b"} starts as the first choice.
                    Line(CppType(statement.type) + " " + name + " = " +
                         As(*statement.choices->args[0], statement.type) + ";");
                    break;
                }
                if (statement.access == "family") {
                    Line(name + ".define(" + Loc(statement.indexRef) + ", " +
                         Expression(*statement.indices[0]) + ", " +
                         Converted(*statement.value, statement.type, false, statement.name) +
                         ", " + Quote(statement.name) + ");");
                } else if (statement.value) {
                    Line(std::string(CppType(statement.type)) + " " + name + " = " +
                         Chosen(statement,
                                Sized(statement,
                                      Converted(*statement.value, statement.type,
                                                statement.natural, statement.name))) +
                         ";");
                } else if (!statement.dims.empty() && statement.dims[0]) {
                    // ℝ^n starts as zeros.
                    std::string code = "mpp::zeros(" + Loc(statement.typeRef);
                    for (const auto& size : statement.dims) {
                        code += ", " + Expression(*size);
                    }
                    Line(std::string(CppType(statement.type)) + " " + name + " = " + code + ");");
                } else {
                    Line(std::string(CppType(statement.type)) + " " + name + "{};");
                }
                break;
            case Stmt::Kind::Assign: {
                if (!statement.member.empty()) {
                    Line(name + "." + MemberName(statement) + " = " +
                         As(*statement.value, statement.type) + ";");
                    break;
                }
                if (statement.access == "map" || statement.access == "list") {
                    ElementAssign(statement, name);
                    break;
                }
                const std::string value = Chosen(
                    statement, Converted(*statement.value, statement.type, statement.natural,
                                         statement.name));
                if (statement.access == "family") {
                    Line(name + ".set(" + Loc(statement.indexRef) + ", " +
                         Expression(*statement.indices[0]) + ", " + value + ", " +
                         Quote(statement.name) + ");");
                } else if (!statement.access.empty()) {
                    std::string code = "mpp::at(" + Loc(statement.indexRef) + ", " + name;
                    for (const auto& index : statement.indices) {
                        code += ", " + Expression(*index);
                    }
                    Line(code + ") = " + value + ";");
                } else {
                    Line(name + " = " + value + ";");
                }
                break;
            }
            case Stmt::Kind::Expr:
                Line("(void)" + Expression(*statement.value) + ";");
                break;
            case Stmt::Kind::If: If(statement); break;
            case Stmt::Kind::While:
                Braced("while (" + Expression(*statement.value) + ")", statement.body);
                Line("}");
                break;
            case Stmt::Kind::For: {
                Braced("for (const " + std::string(CppType(statement.type)) + " " + name + " : " +
                           Domain(*statement.value) + ")",
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

    // A struct type: its fields, how it prints (text()), and for a list of
    // them printed as a table, its columns (from the @tags) and cells.
    void StructDefinition(const TypeInfo& info) {
        Line("struct " + info.cppName + " {");
        ++indent_;
        std::string text = "std::string(\"⟨\")";
        std::string columns;
        std::string cells;
        for (std::size_t i = 0; i < info.fields.size(); ++i) {
            const FieldInfo& field = info.fields[i];
            const std::string member = Variable(field.ident, field.greek);
            Line(CppType(field.type) + " " + member + "{};");
            text += " + " + Quote((i ? ", " : "") + field.name + " = ") + " + mpp::to_text(" +
                    member + ")";
            const std::string label =
                field.HasTag("label") ? field.TagValue("label") : field.name;
            columns += std::string(i ? ", " : "") + "mpp::Column{.label = " + Quote(label) +
                       ", .align = " + Quote(field.TagValue("align")) +
                       ", .filler = " + Quote(field.TagValue("filler")) +
                       ", .format = " + Quote(field.TagValue("format")) +
                       ", .prefix = " + Quote(field.TagValue("prefix")) +
                       ", .postfix = " + Quote(field.TagValue("postfix")) +
                       ", .unit = " + Quote(field.TagValue("unit")) +
                       ", .hidden = " + (field.HasTag("hidden") ? "true" : "false") + "}";
            cells += std::string(i ? ", " : "") + "mpp::cell(" + member + ", c[" +
                     std::to_string(i) + "])";
        }
        Line("std::string text() const { return " + text + " + \"⟩\"; }");
        Line("bool operator==(const " + info.cppName + "&) const = default;");
        Line("static std::vector<mpp::Column> columns() { return {" + columns + "}; }");
        Line("std::vector<std::string> cells() const { const auto c = columns(); return {" +
             cells + "}; }");
        --indent_;
        Line("};");
    }

    // A field's @default(…) as C++ (the checker made sure it fits the type).
    static std::string DefaultValue(const FieldInfo& field) {
        const std::string value = field.TagValue("default");
        if (field.type == Type::String) return "std::string(" + Quote(value) + ")";
        if (field.type == Type::Int) return "std::int64_t(" + value + ")";
        if (field.type == Type::Real) return "double(" + value + ")";
        return value;
    }

    // m_{key} ← v, L_i ← v
    void ElementAssign(const Stmt& statement, const std::string& name) {
        const std::string value = As(*statement.value, statement.type);
        const std::string& index = Expression(*statement.indices[0]);
        if (statement.access == "map") {
            Line(name + ".set(" + index + ", " + value + ");");
        } else {
            Line(name + ".at(" + Loc(statement.indexRef) + ", " + index + ") = " + value + ";");
        }
    }

    // A value for a variable with choices (x ∈ {"a", "b"}), checked to be one.
    std::string Chosen(const Stmt& statement, std::string value) {
        if (!statement.choices) {
            return value;
        }
        return "mpp::one_of(" + Loc(statement.value->ref) + ", " + value + ", " +
               Expression(*statement.choices) + ", " + Quote(statement.name) + ")";
    }

    static std::string MemberName(const Stmt& statement) {
        // The field's ident is its name (fields are plain words).
        return Variable(statement.member, false);
    }

    // A value for a variable declared with sizes (v ∈ ℝ^3), checked to fit.
    std::string Sized(const Stmt& statement, std::string value) {
        if (statement.dims.empty() || !statement.dims[0]) {
            return value;
        }
        std::string code = "mpp::sized(" + Loc(statement.value->ref) + ", " + value;
        for (const auto& size : statement.dims) {
            code += ", " + Expression(*size);
        }
        return code + ", " + Quote(statement.name) + ")";
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

    static std::string ResultType(const Type& type) {
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
            case Expr::Kind::Name:
                if (expr.access == "family") {
                    return Variable(expr.ident, expr.greek) + ".vector(" + Loc(expr.ref) + ", " +
                           Quote(expr.text) + ")";
                }
                return Variable(expr.ident, expr.greek);
            case Expr::Kind::Unary: {
                const Expr& operand = *expr.args[0];
                if (expr.op == "¬") return "(!" + Expression(operand) + ")";
                if (expr.op == "+") return Expression(operand);
                if (expr.type == Type::Vector || expr.type == Type::Matrix) {
                    return std::string(CppType(expr.type)) + "(-" + Expression(operand) + ")";
                }
                if (expr.type == Type::Int) {
                    return "mpp::neg(" + Loc(expr.ref) + ", " + Expression(operand) + ")";
                }
                return "(-" + Expression(operand) + ")";
            }
            case Expr::Kind::Binary: return Binary(expr);
            case Expr::Kind::Fraction:
                if (expr.type == Type::Vector || expr.type == Type::Matrix) {
                    return "mpp::divide(" + Loc(expr.args[1]->ref) + ", " +
                           Expression(*expr.args[0]) + ", " + Real(*expr.args[1]) + ")";
                }
                return "mpp::div(" + Loc(expr.args[1]->ref) + Args(expr, true) + ")";
            case Expr::Kind::Power:
                if (expr.type == Type::Matrix) {
                    return "mpp::pow(" + Loc(expr.ref) + Args(expr, false) + ")";
                }
                return "mpp::pow(" + Loc(expr.ref) + Args(expr, expr.type == Type::Real) + ")";
            case Expr::Kind::Matrix: return MatrixLiteral(expr);
            case Expr::Kind::Index: return Index(expr);
            case Expr::Kind::Transpose:
                return "mpp::transpose(" + Expression(*expr.args[0]) + ")";
            case Expr::Kind::All: return "mpp::all";
            case Expr::Kind::Root:
                if (expr.args.size() == 1) {
                    return "mpp::sqrt(" + Loc(expr.ref) + Args(expr, true) + ")";
                }
                return "mpp::root(" + Loc(expr.ref) + Args(expr, true) + ")";
            case Expr::Kind::Compare: return Compare(expr);
            case Expr::Kind::Call: return Call(expr);
            case Expr::Kind::Cases: return Cases(expr);
            case Expr::Kind::Range: return "mpp::to_set(" + Domain(expr) + ")";
            case Expr::Kind::EmptySet: return "mpp::EmptySet{}";
            case Expr::Kind::Set: return SetLiteral(expr);
            case Expr::Kind::Builder: return Builder(expr);
            case Expr::Kind::BigOp: return BigOp(expr);
            case Expr::Kind::Quantifier: return Quantifier(expr);
            case Expr::Kind::TypeName:
            case Expr::Kind::StructType: return "";
            case Expr::Kind::StructValue: return StructValue(expr);
            case Expr::Kind::ListOf: return CppType(expr.type) + "{}";
            case Expr::Kind::List: {
                const Type& element = expr.type.info->element;
                std::string code = CppType(expr.type) + "(std::vector<" + CppType(element) + ">{";
                for (std::size_t i = 0; i < expr.args.size(); ++i) {
                    code += (i ? ", " : "") + As(*expr.args[i], element);
                }
                return code + "})";
            }
            case Expr::Kind::Member:
                return "(" + Expression(*expr.args[0]) + ")." + Variable(expr.text, false);
            case Expr::Kind::Method: {
                const Expr& list = *expr.args[0];
                return Expression(list) + ".push(" +
                       As(*expr.args[1], list.type.info->element) + ")";
            }
            case Expr::Kind::Template: {
                std::string code = "(std::string(" + Quote(expr.ops[0]) + ")";
                for (std::size_t i = 0; i < expr.args.size(); ++i) {
                    code += " + mpp::to_text(" + Expression(*expr.args[i]) + ")";
                    if (i + 1 < expr.ops.size() && !expr.ops[i + 1].empty()) {
                        code += " + " + Quote(expr.ops[i + 1]);
                    }
                }
                return code + ")";
            }
        }
        return "";
    }

    // `expr` as a value of `type`: ℤ as ℝ, a set of ℤ as a set of ℝ, ∅ as a
    // set of anything.
    std::string As(const Expr& expr, Type type) {
        if (type == Type::Real) {
            return Real(expr);
        }
        if (expr.type == Type::EmptySet && type != Type::EmptySet) {
            return std::string(CppType(type)) + "{}";
        }
        if (type == Type::Struct && expr.type == Type::Struct && expr.type.info != type.info) {
            // The same fields in another struct type: copy them across.
            std::string code = "[&](const auto& s) { return " + type.info->cppName + "{";
            for (std::size_t i = 0; i < type.info->fields.size(); ++i) {
                const FieldInfo& field = type.info->fields[i];
                code += std::string(i ? ", " : "") + "s." + Variable(field.ident, field.greek);
            }
            return code + "}; }(" + Expression(expr) + ")";
        }
        const std::string code = Expression(expr);
        if (expr.type != type &&
            (type == Type::SetInt || type == Type::SetReal || type == Type::SetString)) {
            return std::string(CppType(type)) + "(" + code + ")";
        }
        return code;
    }

    // What a bound variable goes through: a range, without making a set,
    // or a set or vector.
    std::string Domain(const Expr& domain) {
        if (domain.kind == Expr::Kind::EmptySet) {
            return "mpp::Set<std::int64_t>{}";
        }
        if (domain.kind != Expr::Kind::Range) {
            return domain.type == Type::Map ? Expression(domain) + ".keys()" : Expression(domain);
        }
        const bool real = domain.type == Type::SetReal;
        std::string code = "mpp::range(" + Loc(domain.ref);
        for (const auto& bound : domain.args) {
            code += ", " + (real ? Real(*bound) : Expression(*bound));
        }
        return code + ")";
    }

    // The element type of a set type.
    static Type Element(Type set) {
        return set == Type::SetReal ? Type::Real : set == Type::SetString ? Type::String
                                                                          : Type::Int;
    }

    // for (const auto x : domain) — the start of a loop over a bound variable.
    std::string ForBound(const Expr& expr) {
        return "for (const auto " + Variable(expr.varIdent, expr.varGreek) + " : " +
               Domain(*expr.args[0]) + ")";
    }

    // ⟨name = "Bob", age = 4⟩ as the struct type the checker gave it, its
    // fields in the type's order (left out ones take their @default).
    std::string StructValue(const Expr& expr) {
        const TypeInfo& info = *expr.type.info;
        std::string code = info.cppName + "{";
        for (std::size_t i = 0; i < info.fields.size(); ++i) {
            const FieldInfo& field = info.fields[i];
            std::string value;
            for (std::size_t k = 0; k < expr.ops.size(); ++k) {
                if (expr.ops[k] == field.name) {
                    value = As(*expr.args[k], field.type);
                }
            }
            code += (i ? ", " : "") + (value.empty() ? DefaultValue(field) : value);
        }
        return code + "}";
    }

    std::string SetLiteral(const Expr& expr) {
        const std::string type = CppType(expr.type);
        const Type element = Element(expr.type);
        std::string code = "[&] { " + type + " s;";
        for (const auto& item : expr.args) {
            if (item->kind == Expr::Kind::Range) {
                code += " s.insert_all(" + Domain(*item) + ");";
            } else {
                code += " s.insert(" + As(*item, element) + ");";
            }
        }
        return code + " return s; }()";
    }

    std::string Builder(const Expr& expr) {
        const std::string type = CppType(expr.type);
        std::string code = "[&] { " + type + " s; " + ForBound(expr) + " {";
        const std::string insert =
            " s.insert(" + As(*expr.args[1], Element(expr.type)) + ");";
        if (expr.conditions[0]) {
            code += " if (" + Expression(*expr.conditions[0]) + ")" + insert;
        } else {
            code += insert;
        }
        return code + " } return s; }()";
    }

    std::string BigOp(const Expr& expr) {
        const std::string type = CppType(expr.type);
        const std::string at = Loc(expr.opRef);
        const Expr& body = *expr.args[1];
        if (expr.text == "sum" || expr.text == "prod") {
            const bool sum = expr.text == "sum";
            const std::string term = As(body, expr.type);
            std::string step;
            if (expr.type == Type::Int) {
                step = std::string("mpp::") + (sum ? "add" : "mul") + "(" + at + ", t, " + term +
                       ")";
            } else {
                step = std::string("t ") + (sum ? "+ " : "* ") + term;
            }
            return "[&] { " + type + " t = " + (sum ? "0" : "1") + "; " + ForBound(expr) +
                   " { t = " + step + "; } return t; }()";
        }
        const std::string term = As(body, expr.type);
        if (expr.text == "bigcup") {
            return "[&] { " + type + " t; " + ForBound(expr) + " { t = mpp::set_union(t, " +
                   term + "); } return t; }()";
        }
        // ⋂ of no sets at all has no answer.
        return "[&] { std::optional<" + type + "> t; " + ForBound(expr) +
               " { t = t ? mpp::set_intersection(*t, " + term + ") : " + term +
               "; } if (!t) mpp::fail_at(" + at +
               ", \"⋂ of no sets: there is nothing to intersect\"); return *t; }()";
    }

    std::string Quantifier(const Expr& expr) {
        const bool all = expr.text == "∀";
        const std::string test = Expression(*expr.args[1]);
        return "[&] { " + ForBound(expr) + " { if (" + (all ? "!" : "") + "(" + test +
               ")) return " + (all ? "false" : "true") + "; } return " +
               (all ? "true" : "false") + "; }()";
    }

    // x ∈ S, x ∉ S, A ⊂ B, A ⊆ B, and = ≠ between sets.
    std::string SetComparison(const Expr& expr) {
        const std::string& op = expr.ops[0];
        const Expr& left = *expr.args[0];
        const Expr& right = *expr.args[1];
        std::string test;
        if (op == "∈" || op == "∉") {
            if (right.kind == Expr::Kind::Range) {
                const bool real = right.type == Type::SetReal || left.type == Type::Real;
                test = "mpp::in_range(" + (real ? Real(left) : Expression(left));
                for (const auto& bound : right.args) {
                    test += ", " + (real ? Real(*bound) : Expression(*bound));
                }
                test += ")";
            } else if (right.type == Type::Vector) {
                test = "mpp::contains(" + Expression(right) + ", " + Real(left) + ")";
            } else if (right.type == Type::Map) {
                test = Expression(right) + ".has(" + As(left, right.type.info->key) + ")";
            } else if (right.type == Type::List) {
                test = "mpp::contains(" + Expression(right) + ", " +
                       As(left, right.type.info->element) + ")";
            } else {
                test = "mpp::contains(" + Expression(right) + ", " + Expression(left) + ")";
            }
            return op == "∉" ? "(!" + test + ")" : test;
        }
        // Compare as sets of one type.
        const Type common = left.type == right.type ? left.type
                          : left.type == Type::EmptySet ? right.type
                          : right.type == Type::EmptySet ? left.type
                                                         : Type::SetReal;
        const Type type = common == Type::EmptySet ? Type::SetInt : common;
        const std::string a = As(left, type);
        const std::string b = As(right, type);
        if (op == "⊂") return "mpp::proper_subset(" + a + ", " + b + ")";
        if (op == "⊆") return "mpp::subset(" + a + ", " + b + ")";
        return std::string(op == "≠" ? "(!" : "(") + "(" + a + " == " + b + "))";
    }

    std::string MatrixLiteral(const Expr& expr) {
        std::string values;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            values += (i ? ", " : "") + Real(*expr.args[i]);
        }
        if (expr.type == Type::Vector) {
            return "mpp::vector({" + values + "})";
        }
        const std::string matrix = "mpp::matrix(" + std::to_string(expr.rows) + ", " +
                                   std::to_string(expr.columns) + ", {" + values + "})";
        if (expr.text == "vmatrix") {
            return "mpp::det(" + Loc(expr.ref) + ", " + matrix + ")";
        }
        return matrix;
    }

    std::string Index(const Expr& expr) {
        const Expr& base = *expr.args[0];
        if (expr.access == "family") {
            return Variable(base.ident, base.greek) + ".get(" + Loc(expr.ref) + ", " +
                   Expression(*expr.args[1]) + ", " + Quote(base.text) + ")";
        }
        if (expr.access == "map") {
            return "(" + Expression(base) + ").get(" + Loc(expr.ref) + ", " +
                   As(*expr.args[1], base.type.info->key) + ")";
        }
        if (expr.access == "list") {
            return "(" + Expression(base) + ").at(" + Loc(expr.ref) + ", " +
                   Expression(*expr.args[1]) + ")";
        }
        std::string code = "mpp::at(" + Loc(expr.opRef) + ", " + Expression(base);
        for (std::size_t i = 1; i < expr.args.size(); ++i) {
            const Expr& item = *expr.args[i];
            if (item.kind == Expr::Kind::Range) {
                code += ", mpp::range(" + Loc(item.ref);
                for (const auto& bound : item.args) {
                    code += ", " + Expression(*bound);
                }
                code += ")";
            } else {
                code += ", " + Expression(item);
            }
        }
        return code + ")";
    }

    // An operator on vectors or matrices (the checker set expr.access).
    std::string ArrayBinary(const Expr& expr) {
        const std::string& how = expr.access;
        const Expr& left = *expr.args[0];
        const Expr& right = *expr.args[1];
        const std::string at = Loc(expr.opRef.Empty() ? expr.ref : expr.opRef);
        if (how == "scale") {
            return "mpp::scale(" + Real(left) + ", " + Expression(right) + ")";
        }
        if (how == "scale_right") {
            return "mpp::scale(" + Real(right) + ", " + Expression(left) + ")";
        }
        if (how == "divide") {
            return "mpp::divide(" + Loc(right.ref) + ", " + Expression(left) + ", " +
                   Real(right) + ")";
        }
        if (how == "dot_transposed") {
            return "mpp::dot(" + at + ", " + Expression(*left.args[0]) + ", " +
                   Expression(right) + ")";
        }
        const char* function = how == "matmul" ? "mul" : how.c_str();
        return std::string("mpp::") + function + "(" + at + ", " + Expression(left) + ", " +
               Expression(right) + ")";
    }

    std::string Binary(const Expr& expr) {
        if (expr.access.starts_with("set_")) {
            return "mpp::" + expr.access + "(" + As(*expr.args[0], expr.type) + ", " +
                   As(*expr.args[1], expr.type) + ")";
        }
        if (expr.access == "repeat") {
            const bool textFirst = expr.args[0]->type == Type::String;
            const Expr& text = *expr.args[textFirst ? 0 : 1];
            const Expr& times = *expr.args[textFirst ? 1 : 0];
            return "mpp::repeat(" + Loc(expr.ref) + ", " + Expression(text) + ", " +
                   Expression(times) + ")";
        }
        if (!expr.access.empty()) {
            return ArrayBinary(expr);
        }
        if (expr.type == Type::String) {
            return "(" + Expression(*expr.args[0]) + " + " + Expression(*expr.args[1]) + ")";
        }
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

    static bool IsSetType(Type type) {
        return type == Type::SetInt || type == Type::SetReal || type == Type::SetString ||
               type == Type::EmptySet;
    }

    std::string Compare(const Expr& expr) {
        const Type first = expr.args[0]->type;
        if (expr.ops.size() == 1 && expr.ops[0] == "in") {
            return "mpp::contains_text(" + Expression(*expr.args[1]) + ", " +
                   Expression(*expr.args[0]) + ")";
        }
        if (expr.ops.size() == 1 && first == Type::Struct) {
            return std::string(expr.ops[0] == "≠" ? "!" : "") + "(" +
                   Expression(*expr.args[0]) + " == " + As(*expr.args[1], first) + ")";
        }
        if (expr.ops.size() == 1) {
            const std::string& op = expr.ops[0];
            if (op == "∈" || op == "∉" || op == "⊂" || op == "⊆" || IsSetType(first) ||
                IsSetType(expr.args[1]->type)) {
                return SetComparison(expr);
            }
        }
        if (expr.ops.size() == 1 && (first == Type::Vector || first == Type::Matrix)) {
            const std::string a = Expression(*expr.args[0]);
            const std::string b = Expression(*expr.args[1]);
            const std::string& op = expr.ops[0];
            if (op == "≈") return "mpp::approx(" + a + ", " + b + ")";
            return std::string(op == "≠" ? "(!" : "(") + "mpp::equal(" + a + ", " + b + "))";
        }
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
        if (expr.access == "size") {
            return "mpp::size(" + Expression(*expr.args[0]) + ")";
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
        if (function->result == BuiltinFunction::Result::Special) {
            // Sizes stay whole; a number given to norm becomes ℝ.
            std::string code = "mpp::" + expr.text + "(" + Loc(expr.ref);
            for (const auto& arg : expr.args) {
                code += ", " + (expr.text == "norm" ? Real(*arg) : Expression(*arg));
            }
            return code + ")";
        }
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
