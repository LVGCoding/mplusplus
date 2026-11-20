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

// The type a set stands for when declaring (x ∈ ℝ, f(n ∈ ℕ) → ℤ), or Error.
// `natural` is set for ℕ.
Type SetType(const std::string& set, bool& natural) {
    natural = set == "ℕ";
    if (set == "ℝ") return Type::Real;
    if (set == "ℤ" || set == "ℕ") return Type::Int;
    if (set == "𝔹") return Type::Bool;
    return Type::Error;
}

// The type of a value that can be either of two types, e.g. the cases of a
// piecewise function. Sets `ok` to false if they don't fit together.
Type Unify(Type a, Type b, bool& ok) {
    ok = true;
    if (a == Type::Error || b == Type::Error) return Type::Error;
    if (IsNumeric(a) && IsNumeric(b)) return Join(a, b);
    if (a == b) return a;
    ok = false;
    return Type::Error;
}

class Checker {
public:
    explicit Checker(std::vector<Diagnostic>& diagnostics) : diagnostics_(diagnostics) {}

    void CheckProgram(Program& program) {
        // Where each name is first defined, to explain "used before defined".
        for (const auto& statement : program.statements) {
            if (statement->kind == Stmt::Kind::Define ||
                statement->kind == Stmt::Kind::Function) {
                laterDefinitions_.try_emplace(statement->name, statement->nameRef);
            }
        }
        scopes_.emplace_back();
        for (auto& statement : program.statements) {
            CheckStatement(*statement);
        }
        // A function nobody calls is still checked, with ℝ for parameters
        // without a type, so its mistakes show up.
        for (auto& [name, function] : functions_) {
            if (function.definition->instances.empty()) {
                std::vector<Type> types;
                for (const Param& param : function.definition->params) {
                    bool natural = false;
                    const Type declared = SetType(param.declaredType, natural);
                    types.push_back(param.declaredType.empty() ? Type::Real : declared);
                }
                Instantiate(function, types);
            }
        }
    }

private:
    struct Variable {
        Type type = Type::Error;
        bool natural = false;
        // A for loop's variable, which the loop body can't change.
        bool loop = false;
        SourceRef ref;
    };
    using Scope = std::map<std::string, Variable>;

    struct Function {
        Stmt* definition = nullptr;
        // The global variables defined before the function: all it can see
        // besides its parameters.
        Scope globals;
        // Functions are numbered in order; one sees only those before it.
        std::size_t order = 0;
    };

    // The function instance being checked.
    struct FunctionContext {
        FunctionInstance* instance = nullptr;
        const Function* function = nullptr;
        // The result type is declared (→ T), or known so far from the returns
        // (or cases) checked, for calls of the function inside itself.
        bool declared = false;
        Type soFar = Type::Error;
        bool usedSoFar = false;
        std::vector<std::pair<Type, SourceRef>> returns;
        bool bareReturn = false;
    };

    std::vector<Diagnostic>& diagnostics_;
    std::vector<Scope> scopes_;
    std::map<std::string, Function> functions_;
    std::map<std::string, SourceRef> laterDefinitions_;
    // Names whose block has ended, and where they were defined.
    std::map<std::string, SourceRef> endedScopes_;
    int loopDepth_ = 0;
    FunctionContext* context_ = nullptr;
    std::map<const FunctionInstance*, FunctionContext*> active_;

    void Error(std::string message, const SourceRef& ref) {
        diagnostics_.push_back({std::move(message), ref});
    }

    Variable* Lookup(const std::string& name) {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            if (auto found = scope->find(name); found != scope->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    // A function defined in the document that can be called from here.
    Function* LookupFunction(const std::string& name) {
        auto found = functions_.find(name);
        if (found == functions_.end()) {
            return nullptr;
        }
        if (context_ && found->second.order > context_->function->order) {
            return nullptr;
        }
        return &found->second;
    }

    // --- Statements -------------------------------------------------------------

    void CheckStatement(Stmt& statement) {
        switch (statement.kind) {
            case Stmt::Kind::Define: CheckDefine(statement); break;
            case Stmt::Kind::Assign: CheckAssign(statement); break;
            case Stmt::Kind::Expr: CheckExpressionStatement(statement); break;
            case Stmt::Kind::If:
                RequireCondition(*statement.value, "if");
                CheckBlock(statement.body);
                CheckBlock(statement.elseBody);
                break;
            case Stmt::Kind::While:
                RequireCondition(*statement.value, "while");
                ++loopDepth_;
                CheckBlock(statement.body);
                --loopDepth_;
                break;
            case Stmt::Kind::For: CheckFor(statement); break;
            case Stmt::Kind::Break:
            case Stmt::Kind::Continue:
                if (loopDepth_ == 0) {
                    Error(std::string(statement.kind == Stmt::Kind::Break ? "break" : "continue") +
                              " can only be used inside a loop (while or for).",
                          statement.ref);
                }
                break;
            case Stmt::Kind::Return: CheckReturn(statement); break;
            case Stmt::Kind::Function: CheckFunction(statement); break;
        }
    }

    void CheckBlock(Block& block) {
        scopes_.emplace_back();
        for (auto& statement : block) {
            CheckStatement(*statement);
        }
        EndScope();
    }

    void EndScope() {
        for (const auto& [name, variable] : scopes_.back()) {
            endedScopes_.insert_or_assign(name, variable.ref);
        }
        scopes_.pop_back();
    }

    void RequireCondition(Expr& condition, const char* keyword) {
        const Type type = CheckExpr(condition);
        if (type != Type::Bool && type != Type::Error) {
            Error(std::string("The condition after ") + keyword +
                      " must be true or false (𝔹), but this is in " + TypeName(type) + ".",
                  condition.ref);
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

    // Reports a declared set that can't be used (yet); returns its type.
    Type DeclaredType(const std::string& set, const SourceRef& ref, bool& natural) {
        const Type type = SetType(set, natural);
        if (type == Type::Error) {
            if (set == "Σ") {
                Error("For text, use a string: name := \"text\".", ref);
            } else {
                Error(set + " can't be used yet.", ref);
            }
        }
        return type;
    }

    // Can `name` be defined here? Reports why not at `ref`.
    bool CheckNewName(const std::string& name, const SourceRef& ref) {
        if (FindBuiltinFunction(name)) {
            Error(name + " is a built-in function; choose another name.", ref);
            return false;
        }
        if (auto function = functions_.find(name); function != functions_.end()) {
            Error(name + " is already a function (" +
                      Row(function->second.definition->nameRef) + "); choose another name.",
                  ref);
            return false;
        }
        if (const Variable* existing = Lookup(name)) {
            Error(name + " is already defined (" + Row(existing->ref) +
                      "). To change it, write " + name + " ← …",
                  ref);
            return false;
        }
        return true;
    }

    void CheckDefine(Stmt& statement) {
        Type type = Type::Error;
        if (!statement.declaredType.empty()) {
            type = DeclaredType(statement.declaredType, statement.typeRef, statement.natural);
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
        if (CheckNewName(statement.name, statement.nameRef)) {
            scopes_.back()[statement.name] = {type, statement.natural, false, statement.nameRef};
        }
    }

    void CheckAssign(Stmt& statement) {
        const Type value = CheckExpr(*statement.value);
        const Variable* variable = Lookup(statement.name);
        if (!variable) {
            if (statement.name == "π" || statement.name == "e") {
                Error(statement.name + " is a constant and can't be changed.", statement.nameRef);
            } else if (functions_.contains(statement.name)) {
                Error(statement.name + " is a function and can't be changed.", statement.nameRef);
            } else {
                Error(statement.name + " isn't defined yet. To define it, write " +
                          statement.name + " := …",
                      statement.nameRef);
            }
            return;
        }
        if (variable->loop) {
            Error(statement.name + " counts the loop, so it can't be changed inside it.",
                  statement.nameRef);
            return;
        }
        statement.type = variable->type;
        statement.natural = variable->natural;
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
        // A function of the document may be called just for what it does.
        if (value.kind == Expr::Kind::Call && !value.function.empty()) {
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

    void CheckFor(Stmt& statement) {
        Expr& range = *statement.value;
        Type type = Type::Int;
        bool ok = true;
        for (auto& bound : range.args) {
            const Type bound_type = CheckExpr(*bound);
            if (bound_type == Type::Error) {
                ok = false;
            } else if (!IsNumeric(bound_type)) {
                Error("A range needs numbers, but this is in " +
                          std::string(TypeName(bound_type)) + ".",
                      bound->ref);
                ok = false;
            } else {
                type = Join(type, bound_type);
            }
        }
        range.type = ok ? type : Type::Error;
        statement.type = range.type;
        scopes_.emplace_back();
        if (CheckNewName(statement.name, statement.nameRef)) {
            scopes_.back()[statement.name] = {statement.type, false, true, statement.nameRef};
        }
        ++loopDepth_;
        for (auto& inner : statement.body) {
            CheckStatement(*inner);
        }
        --loopDepth_;
        EndScope();
    }

    void CheckReturn(Stmt& statement) {
        if (!context_) {
            Error("return can only be used inside a function.", statement.ref);
            if (statement.value) {
                CheckExpr(*statement.value);
            }
            return;
        }
        FunctionContext& context = *context_;
        const std::string& name = context.function->definition->name;
        if (!statement.value) {
            context.bareReturn = true;
            if (context.declared) {
                Error(name + " gives a value in " + context.function->definition->declaredType +
                          ": write return followed by it.",
                      statement.ref);
            }
            return;
        }
        const Type type = CheckExpr(*statement.value);
        if (type == Type::Void) {
            Error("This doesn't give a value to return.", statement.value->ref);
            return;
        }
        if (context.declared) {
            CheckStore(context.instance->result, type, "The result of " + name,
                       context.function->definition->declaredType.c_str(), statement.value->ref);
            return;
        }
        if (type == Type::Error) {
            return;
        }
        if (!context.returns.empty()) {
            bool ok = true;
            const Type joined = Unify(context.soFar, type, ok);
            if (!ok && context.soFar != Type::Error) {
                Error("This returns " + std::string(TypeName(type)) + ", but " + name +
                          " returns " + TypeName(context.soFar) + " before (" +
                          Row(context.returns.front().second) + ").",
                      statement.value->ref);
                return;
            }
            context.soFar = joined;
        } else {
            context.soFar = type;
        }
        context.returns.push_back({type, statement.value->ref});
    }

    void CheckFunction(Stmt& statement) {
        if (context_ || scopes_.size() > 1) {
            Error("Functions are defined outside of blocks and other functions.",
                  statement.nameRef);
            return;
        }
        bool natural = false;
        bool ok = true;
        for (const Param& param : statement.params) {
            if (!param.declaredType.empty()) {
                ok &= DeclaredType(param.declaredType, param.typeRef, natural) != Type::Error;
            }
            if (FindBuiltinFunction(param.name)) {
                Error(param.name + " is a built-in function; choose another name.", param.ref);
                ok = false;
            }
            for (const Param& other : statement.params) {
                if (&other == &param) break;
                if (other.name == param.name) {
                    Error("There are two parameters called " + param.name + ".", param.ref);
                    ok = false;
                }
            }
        }
        if (!statement.declaredType.empty()) {
            ok &= DeclaredType(statement.declaredType, statement.typeRef, natural) != Type::Error;
        }
        if (!CheckNewName(statement.name, statement.nameRef) || !ok) {
            return;
        }
        Function& function = functions_[statement.name];
        function.definition = &statement;
        function.globals = scopes_.front();
        function.order = functions_.size();
        // With every parameter's type written, it can be checked right away.
        bool allDeclared = true;
        std::vector<Type> types;
        for (const Param& param : statement.params) {
            allDeclared &= !param.declaredType.empty();
            types.push_back(SetType(param.declaredType, natural));
        }
        if (allDeclared) {
            Instantiate(function, types);
        }
    }

    // The instance of `function` for these parameter types, checked the
    // first time it is needed. It may still be being checked (active_) when
    // the function uses itself.
    FunctionInstance& Instantiate(Function& function, const std::vector<Type>& types) {
        Stmt& definition = *function.definition;
        for (auto& existing : definition.instances) {
            if (existing->params == types) {
                return *existing;
            }
        }
        auto created = std::make_unique<FunctionInstance>();
        FunctionInstance& instance = *created;
        instance.params = types;
        instance.cppName = "f_" + std::string(definition.greek ? "g_" : "u_") + definition.ident +
                           "_" + std::to_string(definition.instances.size());
        Scope parameters;
        for (std::size_t i = 0; i < definition.params.size(); ++i) {
            const Param& param = definition.params[i];
            bool natural = false;
            SetType(param.declaredType, natural);
            instance.naturalParams.push_back(natural);
            parameters[param.name] = {types[i], natural, false, param.ref};
        }
        FunctionContext context{.instance = &instance, .function = &function};
        if (!definition.declaredType.empty()) {
            context.declared = true;
            instance.result = SetType(definition.declaredType, instance.naturalResult);
        }
        if (definition.value) {
            instance.value = Clone(*definition.value);
        } else {
            instance.body = Clone(definition.body);
        }
        definition.instances.push_back(std::move(created));

        // Check the copy with only the globals and parameters in scope.
        std::vector<Scope> savedScopes = std::move(scopes_);
        const int savedLoops = loopDepth_;
        FunctionContext* savedContext = context_;
        scopes_ = {function.globals, std::move(parameters)};
        loopDepth_ = 0;
        context_ = &context;
        active_[&instance] = &context;

        const std::string& name = definition.name;
        if (instance.value) {
            const Type type = CheckExpr(*instance.value);
            if (context.declared) {
                CheckStore(instance.result, type, "The result of " + name,
                           definition.declaredType.c_str(), instance.value->ref);
            } else {
                if (type == Type::Void) {
                    Error("This doesn't give a value for " + name + " to give.",
                          instance.value->ref);
                }
                instance.result = type == Type::Void ? Type::Error : type;
            }
        } else {
            scopes_.emplace_back();
            for (auto& statement : instance.body) {
                CheckStatement(*statement);
            }
            scopes_.pop_back();
            if (!context.declared) {
                instance.result = context.returns.empty() ? Type::Void : context.soFar;
                if (!context.returns.empty() && context.bareReturn) {
                    Error(name + " returns a value in some places but not others.",
                          definition.nameRef);
                }
            }
        }
        if (context.usedSoFar && instance.result != context.soFar) {
            Error(name + " uses itself, but its results aren't all in " +
                      TypeName(context.soFar) + ". Write the type of its result, as in " + name +
                      "(…) → " + TypeName(instance.result) + " := …",
                  definition.nameRef);
        }

        active_.erase(&instance);
        context_ = savedContext;
        loopDepth_ = savedLoops;
        scopes_ = std::move(savedScopes);
        return instance;
    }

    // The type of calling the function instance being checked now, from
    // inside itself.
    Type RecursiveResult(FunctionContext& context, const Expr& call) {
        if (context.declared) {
            return context.instance->result;
        }
        if (context.soFar != Type::Error) {
            context.usedSoFar = true;
            return context.soFar;
        }
        const std::string& name = context.function->definition->name;
        Error(name + " uses itself before it gives a value, so write the type of its result, "
                     "as in " + name + "(…) → ℤ := …",
              call.ref);
        return Type::Error;
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
            case Expr::Kind::Cases: return CheckCases(expr);
            case Expr::Kind::Range:
                CheckArgs(expr);
                Error("A range (a..b) can only be used in a for loop, for now.", expr.ref);
                return Type::Error;
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
        if (const Variable* variable = Lookup(expr.text)) {
            expr.natural = variable->natural;
            return variable->type;
        }
        if (expr.text == "π" || expr.text == "e") {
            expr.kind = Expr::Kind::Constant;
            return Type::Real;
        }
        if (FindBuiltinFunction(expr.text) || functions_.contains(expr.text)) {
            Error(expr.text + " is a function. Write " + expr.text + "(…) to use it.", expr.ref);
            return Type::Error;
        }
        if (const auto ended = endedScopes_.find(expr.text); ended != endedScopes_.end()) {
            Error(expr.text + " isn't defined here: it is defined inside a block (" +
                      Row(ended->second) + ") and only exists there.",
                  expr.ref);
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
            if (!const_cast<Checker*>(this)->Lookup(std::string(1, c))) {
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

    Type CheckUserCall(Function& function, Expr& expr, bool statement) {
        const Stmt& definition = *function.definition;
        const std::string& name = definition.name;
        if (CheckArgs(expr) == Type::Error) {
            return Type::Error;
        }
        if (expr.args.size() != definition.params.size()) {
            const std::size_t count = definition.params.size();
            Error(name + " takes " + std::to_string(count) +
                      (count == 1 ? " argument" : " arguments") + ", not " +
                      std::to_string(expr.args.size()) + ".",
                  expr.ref);
            return Type::Error;
        }
        std::vector<Type> types;
        bool ok = true;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            const Param& param = definition.params[i];
            const Type argument = expr.args[i]->type;
            if (param.declaredType.empty()) {
                types.push_back(argument);
                continue;
            }
            bool natural = false;
            const Type declared = SetType(param.declaredType, natural);
            ok &= CheckStore(declared, argument, param.name, param.declaredType.c_str(),
                             expr.args[i]->ref);
            types.push_back(declared);
        }
        if (!ok) {
            return Type::Error;
        }
        const std::size_t instancesBefore = function.definition->instances.size();
        const std::size_t errorsBefore = diagnostics_.size();
        FunctionInstance& instance = Instantiate(function, types);
        expr.function = instance.cppName;
        if (function.definition->instances.size() != instancesBefore &&
            diagnostics_.size() != errorsBefore) {
            // The problem is in the function's body; say which call led there.
            std::string described;
            for (std::size_t i = 0; i < types.size(); ++i) {
                described += std::string(i ? ", " : "") + TypeName(types[i]);
            }
            Error(name + " doesn't work with " + (types.size() == 1 ? "a value" : "values") +
                      " in " + described + ": see " + Row(diagnostics_[errorsBefore].ref) + ".",
                  expr.ref);
            return Type::Error;
        }
        Type result = instance.result;
        if (auto active = active_.find(&instance); active != active_.end()) {
            result = RecursiveResult(*active->second, expr);
        }
        if (result == Type::Void && !statement) {
            Error(name + "(…) doesn't give a value.", expr.ref);
            return Type::Error;
        }
        return result;
    }

    Type CheckCases(Expr& expr) {
        Type type = Type::Error;
        bool first = true;
        bool failed = false;
        // A piecewise function that uses itself: what its first cases give
        // is the type of the calls in later ones.
        FunctionContext* context =
            context_ && context_->instance->value.get() == &expr && !context_->declared
                ? context_
                : nullptr;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            Expr* condition = expr.conditions[i].get();
            if (condition) {
                RequireCondition(*condition, "a case");
            } else if (i + 1 < expr.args.size()) {
                Error("'otherwise' must be the last case.", expr.ref);
                failed = true;
            }
            Expr& value = *expr.args[i];
            const Type value_type = CheckExpr(value);
            if (value_type == Type::Void) {
                Error("This doesn't give a value.", value.ref);
                failed = true;
                continue;
            }
            if (value_type == Type::Error) {
                failed = true;
                continue;
            }
            if (first) {
                type = value_type;
                first = false;
            } else {
                bool ok = true;
                const Type joined = Unify(type, value_type, ok);
                if (!ok) {
                    Error("This case gives " + std::string(TypeName(value_type)) +
                              ", but the ones before give " + TypeName(type) + ".",
                          value.ref);
                    failed = true;
                    continue;
                }
                type = joined;
            }
            if (context) {
                context->soFar = type;
            }
        }
        return failed ? Type::Error : type;
    }

    Type CheckCall(Expr& expr, bool statement) {
        if (Function* user = LookupFunction(expr.text)) {
            return CheckUserCall(*user, expr, statement);
        }
        const BuiltinFunction* function = FindBuiltinFunction(expr.text);
        if (!function) {
            CheckArgs(expr);
            if (functions_.contains(expr.text)) {
                Error(expr.text + " is defined after the function using it. Move its "
                          "definition above.",
                      expr.opRef);
            } else if (Lookup(expr.text)) {
                Error(expr.text + " isn't a function. To multiply, write " + expr.text +
                          "⋅(…).",
                      expr.opRef);
            } else if (const auto later = laterDefinitions_.find(expr.text);
                       later != laterDefinitions_.end()) {
                Error(expr.text + " is used before it is defined (" + Row(later->second) + ").",
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
