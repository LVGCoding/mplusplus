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
        case Type::Vector: return "ℝ^n";
        case Type::Matrix: return "ℝ^(n×m)";
        case Type::Void: return "nothing";
        case Type::Error: return "?";
    }
    return "?";
}

namespace {

bool IsNumeric(Type type) { return type == Type::Int || type == Type::Real; }
bool IsArray(Type type) { return type == Type::Vector || type == Type::Matrix; }

// The type of a declaration with sizes: ℝ^n is a vector, ℝ^{n×m} a matrix.
Type WithDims(Type element, std::size_t dims) {
    if (dims == 0 || element == Type::Error) return element;
    return dims == 1 ? Type::Vector : Type::Matrix;
}

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
            // (A subscripted definition is a label or a family element; which
            // isn't known yet.)
            if ((statement->kind == Stmt::Kind::Define && statement->indices.empty()) ||
                statement->kind == Stmt::Kind::Function) {
                laterDefinitions_.try_emplace(statement->name, statement->nameRef);
            }
        }
        scopes_.emplace_back();
        for (auto& statement : program.statements) {
            CheckStatement(*statement);
        }
        for (const auto& [name, family] : families_) {
            program.families.push_back(family.family);
        }
        // A function nobody calls is still checked, with ℝ for parameters
        // without a type, so its mistakes show up.
        for (auto& [name, function] : functions_) {
            if (function.definition->instances.empty()) {
                std::vector<Type> types;
                for (const Param& param : function.definition->params) {
                    types.push_back(param.declaredType.empty() ? Type::Real : ParamType(param));
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

    struct FamilyInfo {
        Family family;
        SourceRef ref;
    };

    std::vector<Diagnostic>& diagnostics_;
    std::vector<Scope> scopes_;
    // Families exist for the whole program, from their first element.
    std::map<std::string, FamilyInfo> families_;
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

    // A parameter's declared type, sizes included (ℝ^n is a vector).
    static Type ParamType(const Param& param) {
        bool natural = false;
        return WithDims(SetType(param.declaredType, natural), param.dims.size());
    }

    // A declared type with sizes: reports a set that can't have them, and
    // checks the sizes are whole numbers if `checkSizes`.
    Type DeclaredWithDims(const std::string& set, const SourceRef& ref,
                          const std::vector<std::shared_ptr<Expr>>& dims, bool& natural,
                          bool checkSizes) {
        const Type element = DeclaredType(set, ref, natural);
        if (dims.empty() || element == Type::Error) {
            return element;
        }
        if (element != Type::Real || natural) {
            Error("Vectors and matrices hold real numbers: write ℝ^… .", ref);
            return Type::Error;
        }
        if (dims.size() > 2) {
            Error("Only vectors (ℝ^n) and matrices (ℝ^{n×m}) can be used for now.", ref);
            return Type::Error;
        }
        if (checkSizes) {
            for (const auto& size : dims) {
                if (!size) {
                    if (dims.size() == 2) {
                        Error("A matrix needs both of its sizes.", ref);
                        return Type::Error;
                    }
                    continue;
                }
                const Type type = CheckExpr(*size);
                if (type != Type::Int && type != Type::Error) {
                    Error("A size must be a whole number (ℤ), but this is in " +
                              std::string(TypeName(type)) + ".",
                          size->ref);
                }
            }
        }
        return WithDims(element, dims.size());
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
        if (auto family = families_.find(name); family != families_.end()) {
            Error(name + " is already a family (" + Row(family->second.ref) +
                      "), with elements like " + name + "_1; choose another name.",
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

    // Is the subscript on `name` a label, part of the name (v_{max})? It is
    // when it is a single name that isn't defined, on a name that isn't a
    // variable or family; otherwise it is an index.
    bool IsLabel(const std::string& name, const std::vector<std::unique_ptr<Expr>>& items) {
        if (items.size() != 1 || items[0]->kind != Expr::Kind::Name) {
            return false;
        }
        if (Lookup(name) || families_.contains(name)) {
            return false;
        }
        const std::string& label = items[0]->text;
        return !Lookup(label) && !families_.contains(label);
    }

    // The statement's subscript is a label: move it into the name.
    void ApplyLabel(Stmt& statement) {
        const Expr& label = *statement.indices[0];
        statement.name += "_" + label.text;
        statement.ident += "_" + label.ident;
        statement.nameRef = Span(statement.nameRef, statement.indexRef);
        statement.indices.clear();
    }

    // Each index must be a whole number; reports one that isn't.
    bool RequireWholeIndices(std::vector<std::unique_ptr<Expr>>& items) {
        bool ok = true;
        for (auto& item : items) {
            if (item->kind == Expr::Kind::All || item->kind == Expr::Kind::Range) {
                Error("Only a single entry can be given a value here, not a range.", item->ref);
                ok = false;
                continue;
            }
            const Type type = CheckExpr(*item);
            if (type != Type::Int && type != Type::Error) {
                Error("An index must be a whole number (ℤ), but this is in " +
                          std::string(TypeName(type)) + ".",
                      item->ref);
                ok = false;
            }
            ok &= type != Type::Error;
        }
        return ok;
    }

    // x_i := value: an element of the family x, made on first use.
    void CheckDefineElement(Stmt& statement) {
        if (!statement.declaredType.empty()) {
            Error("An element of a family can't have its own type.", statement.typeRef);
        }
        const Type value = statement.value ? CheckExpr(*statement.value) : Type::Error;
        if (!RequireWholeIndices(statement.indices)) {
            return;
        }
        const std::string& name = statement.name;
        if (const Variable* variable = Lookup(name)) {
            if (IsArray(variable->type)) {
                Error(name + " is " + (variable->type == Type::Vector ? "a vector" : "a matrix") +
                          ". To change an entry, write " + name + "_… ← …",
                      statement.nameRef);
            } else {
                Error(name + " is already defined (" + Row(variable->ref) +
                          ") as a single value, so it can't have elements.",
                      statement.nameRef);
            }
            return;
        }
        if (statement.indices.size() != 1) {
            Error("A family has one index, as in " + name + "_i.", statement.indexRef);
            return;
        }
        if (value == Type::Error) {
            return;
        }
        statement.access = "family";
        auto existing = families_.find(name);
        if (existing == families_.end()) {
            if (FindBuiltinFunction(name) || functions_.contains(name)) {
                Error(name + " is a function; choose another name.", statement.nameRef);
                return;
            }
            Type element = value;
            if (IsNumeric(value)) {
                element = Type::Real;
            } else if (value != Type::Bool && value != Type::String) {
                Error("A family holds numbers, true/false or strings, not " +
                          std::string(TypeName(value)) + ".",
                      statement.value->ref);
                return;
            }
            Family family{.name = name, .ident = statement.ident, .greek = statement.greek,
                          .element = element};
            existing = families_.emplace(name, FamilyInfo{family, statement.nameRef}).first;
        }
        statement.type = existing->second.family.element;
        CheckStore(statement.type, value, name + "_…", TypeName(statement.type),
                   statement.value->ref);
    }

    void CheckDefine(Stmt& statement) {
        if (!statement.indices.empty()) {
            if (!IsLabel(statement.name, statement.indices)) {
                CheckDefineElement(statement);
                return;
            }
            ApplyLabel(statement);
        }
        Type type = Type::Error;
        if (!statement.declaredType.empty()) {
            type = DeclaredWithDims(statement.declaredType, statement.typeRef, statement.dims,
                                    statement.natural, true);
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
                CheckStore(type, value, statement.name,
                           IsArray(type) ? TypeName(type) : statement.declaredType.c_str(),
                           statement.value->ref);
            }
        }
        statement.type = type;
        if (CheckNewName(statement.name, statement.nameRef)) {
            scopes_.back()[statement.name] = {type, statement.natural, false, statement.nameRef};
        }
    }

    // x_i ← value: an entry of a vector or matrix, or an element of a family.
    void CheckAssignElement(Stmt& statement) {
        const Type value = CheckExpr(*statement.value);
        if (!RequireWholeIndices(statement.indices)) {
            return;
        }
        const std::string& name = statement.name;
        std::size_t wanted = 1;
        if (auto family = families_.find(name); family != families_.end()) {
            statement.access = "family";
            statement.type = family->second.family.element;
        } else if (const Variable* variable = Lookup(name)) {
            if (variable->loop) {
                Error(name + " counts the loop, so it can't be changed inside it.",
                      statement.nameRef);
                return;
            }
            if (variable->type == Type::Vector) {
                statement.access = "vector";
            } else if (variable->type == Type::Matrix) {
                statement.access = "matrix";
                wanted = 2;
            } else {
                if (variable->type != Type::Error) {
                    Error(name + " is a single value (" + TypeName(variable->type) +
                              "), so it has no entries.",
                          statement.nameRef);
                }
                return;
            }
            statement.type = Type::Real;
        } else {
            Error(name + " isn't defined yet. To define an element, write " + name + "_… := …",
                  statement.nameRef);
            return;
        }
        if (statement.indices.size() != wanted) {
            Error(wanted == 1 ? name + " has one index, as in " + name + "_i."
                              : name + " is a matrix, with two indices: " + name + "_{i,j}.",
                  statement.indexRef);
            return;
        }
        CheckStore(statement.type, value, name + "_…", TypeName(statement.type),
                   statement.value->ref);
    }

    void CheckAssign(Stmt& statement) {
        if (!statement.indices.empty()) {
            if (!IsLabel(statement.name, statement.indices)) {
                CheckAssignElement(statement);
                return;
            }
            ApplyLabel(statement);
        }
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
                ok &= DeclaredWithDims(param.declaredType, param.typeRef, param.dims, natural,
                                       false) != Type::Error;
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
            ok &= DeclaredWithDims(statement.declaredType, statement.typeRef, statement.dims,
                                   natural, false) != Type::Error;
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
            types.push_back(ParamType(param));
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
            instance.result = WithDims(SetType(definition.declaredType, instance.naturalResult),
                                       definition.dims.size());
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
            case Expr::Kind::Index: return CheckIndex(expr);
            case Expr::Kind::All:
                Error("* can only be used in a subscript, as in A_{i,*}.", expr.ref);
                return Type::Error;
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
                if (IsArray(expr.args[0]->type)) {
                    return expr.args[0]->type;
                }
                return RequireNumbers(expr, expr.op) ? expr.args[0]->type : Type::Error;
            case Expr::Kind::Binary: return CheckBinary(expr);
            case Expr::Kind::Matrix: {
                if (!RequireNumbers(expr, "A matrix")) {
                    return Type::Error;
                }
                if (expr.text == "vmatrix") {
                    return Type::Real;  // |…| of a matrix is its determinant
                }
                return expr.columns == 1 ? Type::Vector : Type::Matrix;
            }
            case Expr::Kind::Transpose:
                if (!IsArray(expr.args[0]->type)) {
                    Error("⊤ turns a vector or matrix on its side; this is in " +
                              std::string(TypeName(expr.args[0]->type)) + ".",
                          expr.args[0]->ref);
                    return Type::Error;
                }
                return Type::Matrix;
            case Expr::Kind::Fraction:
                if (IsArray(expr.args[0]->type) && IsNumeric(expr.args[1]->type)) {
                    return expr.args[0]->type;
                }
                return RequireNumbers(expr, "A fraction") ? Type::Real : Type::Error;
            case Expr::Kind::Root:
                return RequireNumbers(expr, "A root") ? Type::Real : Type::Error;
            case Expr::Kind::Power: {
                if (expr.args[0]->type == Type::Matrix) {
                    if (expr.args[1]->type != Type::Int) {
                        Error("A matrix can only be raised to a whole power (ℤ), such as A^2 or "
                              "A^{-1}.",
                              expr.args[1]->ref);
                        return Type::Error;
                    }
                    return Type::Matrix;
                }
                if (expr.args[0]->type == Type::Vector) {
                    Error("A vector can't be raised to a power. For its transpose, write ^⊤; "
                          "for v⋅v, write v⋅v.",
                          expr.args[0]->ref);
                    return Type::Error;
                }
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
        if (auto family = families_.find(expr.text); family != families_.end()) {
            if (family->second.family.element != Type::Real) {
                Error(expr.text + " is a family of " + TypeName(family->second.family.element) +
                          "; use its elements, as in " + expr.text + "_1.",
                      expr.ref);
                return Type::Error;
            }
            expr.access = "family";
            return Type::Vector;
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
        if (IsArray(left) || IsArray(right) || op == "⊙" || op == "∖") {
            return CheckArrayBinary(expr);
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

    // An operator with a vector or matrix on at least one side. Sets
    // expr.access to what the code generator should do.
    Type CheckArrayBinary(Expr& expr) {
        const std::string& op = expr.op;
        const Expr& a = *expr.args[0];
        const Expr& b = *expr.args[1];
        const Type left = a.type;
        const Type right = b.type;
        auto fail = [&](const std::string& message) {
            Error(message, expr.opRef.Empty() ? expr.ref : expr.opRef);
            return Type::Error;
        };
        auto describe = [](Type type) -> std::string {
            switch (type) {
                case Type::Vector: return "a vector";
                case Type::Matrix: return "a matrix";
                case Type::Int:
                case Type::Real: return "a number";
                default: return std::string("something in ") + TypeName(type);
            }
        };
        if (op == "+" || op == "−") {
            if (left == right) {
                expr.access = op == "+" ? "add" : "sub";
                return left;
            }
            return fail(std::string("Can't ") + (op == "+" ? "add " : "subtract ") +
                        describe(right) + (op == "+" ? " to " : " from ") + describe(left) +
                        ".");
        }
        if (op == "⊙") {
            if (left == right && IsArray(left)) {
                expr.access = "hadamard";
                return left;
            }
            return fail("⊙ multiplies two vectors (or two matrices) entry by entry.");
        }
        if (op == "∖") {
            if (left == Type::Matrix && IsArray(right)) {
                expr.access = "solve";
                return right;
            }
            return fail("A∖b solves A⋅x = b: it needs a matrix on the left and a vector or "
                        "matrix on the right.");
        }
        if (op == "/") {
            if (IsArray(left) && IsNumeric(right)) {
                expr.access = "divide";
                return left;
            }
            return fail("Only a vector or matrix divided by a number can be written with /. "
                        "To solve A⋅x = b, write A∖b.");
        }
        if (op == "mod") {
            return fail("mod needs whole numbers.");
        }
        // ⋅, × and writing values next to each other.
        if (IsNumeric(left) && IsArray(right)) {
            expr.access = "scale";
            return right;
        }
        if (IsArray(left) && IsNumeric(right)) {
            expr.access = "scale_right";
            return left;
        }
        if (left == Type::Vector && right == Type::Vector) {
            expr.access = op == "×" ? "cross" : "dot";
            return op == "×" ? Type::Vector : Type::Real;
        }
        // vᵀ⋅w is the dot product, a number.
        if (a.kind == Expr::Kind::Transpose && a.args[0]->type == Type::Vector &&
            right == Type::Vector) {
            expr.access = "dot_transposed";
            return Type::Real;
        }
        if (left == Type::Matrix && IsArray(right)) {
            expr.access = "matmul";
            return right;
        }
        if (left == Type::Vector && right == Type::Matrix) {
            return fail("A vector (a column) can't be multiplied by a matrix. Did you mean "
                        "v^⊤⋅A?");
        }
        return fail("Can't multiply " + describe(left) + " by " + describe(right) + ".");
    }

    // x_i, A_{i,j}, v_{2..4}, A_{i,*}; a label (v_{max}) becomes a name, and
    // I_n, 0_n, 1_n become identity, zeros and ones.
    Type CheckIndex(Expr& expr) {
        Expr& base = *expr.args[0];
        std::vector<std::unique_ptr<Expr>> items;
        for (std::size_t i = 1; i < expr.args.size(); ++i) {
            items.push_back(std::move(expr.args[i]));
        }
        expr.args.resize(1);
        auto restore = [&] {
            for (auto& item : items) {
                expr.args.push_back(std::move(item));
            }
        };
        if (base.kind == Expr::Kind::Name && IsLabel(base.text, items)) {
            expr.kind = Expr::Kind::Name;
            expr.text = base.text + "_" + items[0]->text;
            expr.ident = base.ident + "_" + items[0]->ident;
            expr.greek = base.greek;
            expr.args.clear();
            return CheckName(expr);
        }
        // I_n, 0_n, 0_{n×m}, 1_n: sized identity, zeros and ones.
        const bool identity = base.kind == Expr::Kind::Name && base.text == "I" && !Lookup("I") &&
                              !families_.contains("I");
        const bool filled = base.kind == Expr::Kind::Number && (base.text == "0" || base.text == "1");
        if (identity || filled) {
            std::vector<std::unique_ptr<Expr>> sizes;
            for (auto& item : items) {
                if (item->kind == Expr::Kind::Binary && item->op == "×") {
                    sizes.push_back(std::move(item->args[0]));
                    sizes.push_back(std::move(item->args[1]));
                } else {
                    sizes.push_back(std::move(item));
                }
            }
            expr.kind = Expr::Kind::Call;
            expr.text = identity ? "identity" : base.text == "0" ? "zeros" : "ones";
            expr.opRef = expr.ref;
            expr.args = std::move(sizes);
            if (identity && expr.args.size() != 1) {
                Error("I_n is the n×n identity matrix: it has one size.", expr.ref);
                return Type::Error;
            }
            if (expr.args.size() > 2) {
                Error("Give one size (a vector) or two (a matrix).", expr.ref);
                return Type::Error;
            }
            return CheckCall(expr, false);
        }

        restore();
        // A family element.
        if (base.kind == Expr::Kind::Name && !Lookup(base.text)) {
            auto family = families_.find(base.text);
            if (family == families_.end()) {
                CheckName(base);
                return Type::Error;
            }
            base.type = Type::Vector;
            if (expr.args.size() != 2) {
                Error("A family has one index, as in " + base.text + "_i.", expr.opRef);
                return Type::Error;
            }
            const Type index = CheckExpr(*expr.args[1]);
            if (index != Type::Int) {
                if (index != Type::Error) {
                    Error("An index must be a whole number (ℤ), but this is in " +
                              std::string(TypeName(index)) + ".",
                          expr.args[1]->ref);
                }
                return Type::Error;
            }
            expr.access = "family";
            return family->second.family.element;
        }

        const Type type = CheckExpr(base);
        if (type == Type::Error) {
            return Type::Error;
        }
        if (!IsArray(type)) {
            Error((base.kind == Expr::Kind::Name ? base.text : std::string("This")) +
                      " is a single value (" + TypeName(type) + "), so it has no entries.",
                  expr.opRef);
            return Type::Error;
        }
        const std::size_t wanted = type == Type::Vector ? 1 : 2;
        if (expr.args.size() - 1 != wanted) {
            Error(type == Type::Vector ? "A vector has one index, as in v_i."
                                       : "A matrix has two indices, as in A_{i,j}.",
                  expr.opRef);
            return Type::Error;
        }
        // Each index picks one entry (a whole number), or several (a range or *).
        std::size_t spans = 0;
        bool ok = true;
        for (std::size_t i = 1; i < expr.args.size(); ++i) {
            Expr& item = *expr.args[i];
            if (item.kind == Expr::Kind::All) {
                ++spans;
                continue;
            }
            if (item.kind == Expr::Kind::Range) {
                ++spans;
                item.type = Type::Int;
                for (auto& bound : item.args) {
                    const Type bound_type = CheckExpr(*bound);
                    if (bound_type != Type::Int && bound_type != Type::Error) {
                        Error("A range of indices needs whole numbers (ℤ).", bound->ref);
                        ok = false;
                    }
                }
                continue;
            }
            const Type index = CheckExpr(item);
            if (index != Type::Int && index != Type::Error) {
                Error("An index must be a whole number (ℤ), but this is in " +
                          std::string(TypeName(index)) + ".",
                      item.ref);
                ok = false;
            }
            ok &= index != Type::Error;
        }
        if (!ok) {
            return Type::Error;
        }
        expr.access = type == Type::Vector ? "vector" : "matrix";
        if (spans == 0) {
            return Type::Real;
        }
        if (type == Type::Vector || spans == 1) {
            return Type::Vector;
        }
        return Type::Matrix;
    }

    Type CheckCompare(Expr& expr) {
        bool ok = true;
        for (std::size_t i = 0; i < expr.ops.size(); ++i) {
            const std::string& op = expr.ops[i];
            const Type a = expr.args[i]->type;
            const Type b = expr.args[i + 1]->type;
            if (IsArray(a) || IsArray(b)) {
                if (a != b || (op != "=" && op != "≠" && op != "≈")) {
                    Error(a != b ? "Can't compare " + std::string(TypeName(a)) + " with " +
                                       TypeName(b) + "."
                                 : op + " can't compare vectors or matrices; = ≠ and ≈ can.",
                          Span(expr.args[i]->ref, expr.args[i + 1]->ref));
                    ok = false;
                }
            } else if (op == "=" || op == "≠") {
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
            const Type declared = ParamType(param);
            ok &= CheckStore(declared, argument, param.name,
                             IsArray(declared) ? TypeName(declared) : param.declaredType.c_str(),
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

    // Functions of vectors and matrices (Builtins: Result::Special).
    Type CheckSpecialCall(Expr& expr) {
        const std::string& name = expr.text;
        auto want = [&](std::size_t i, Type type, const char* what) {
            const Type actual = expr.args[i]->type;
            if (actual == type || (type == Type::Real && IsNumeric(actual))) {
                return true;
            }
            Error(name + " needs " + what + ", but this is in " + TypeName(actual) + ".",
                  expr.args[i]->ref);
            return false;
        };
        if (name == "det" || name == "trace" || name == "inverse" || name == "eigenvalues" ||
            name == "eigenvectors" || name == "rank" || name == "rows" || name == "columns") {
            if (!want(0, Type::Matrix, "a matrix")) return Type::Error;
            if (name == "inverse" || name == "eigenvectors") return Type::Matrix;
            if (name == "eigenvalues") return Type::Vector;
            if (name == "rank" || name == "rows" || name == "columns") return Type::Int;
            return Type::Real;
        }
        if (name == "transpose") {
            if (!IsArray(expr.args[0]->type)) {
                return want(0, Type::Matrix, "a vector or matrix") ? Type::Matrix : Type::Error;
            }
            return Type::Matrix;
        }
        if (name == "length") {
            return want(0, Type::Vector, "a vector") ? Type::Int : Type::Error;
        }
        if (name == "norm") {
            const Type type = expr.args[0]->type;
            if (!IsArray(type) && !IsNumeric(type)) {
                return want(0, Type::Vector, "a vector, matrix or number") ? Type::Real
                                                                           : Type::Error;
            }
            return Type::Real;
        }
        if (name == "solve") {
            if (!want(0, Type::Matrix, "a matrix first")) return Type::Error;
            if (!IsArray(expr.args[1]->type)) {
                return want(1, Type::Vector, "a vector or matrix second") ? Type::Vector
                                                                          : Type::Error;
            }
            return expr.args[1]->type;
        }
        // identity(n), zeros(n), zeros(n, m), ones(n), ones(n, m)
        bool ok = true;
        for (std::size_t i = 0; i < expr.args.size(); ++i) {
            ok &= want(i, Type::Int, "whole-number sizes (ℤ)");
        }
        if (!ok) return Type::Error;
        if (name == "identity") return Type::Matrix;
        return expr.args.size() == 1 ? Type::Vector : Type::Matrix;
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
        if (function->result == BuiltinFunction::Result::Special) {
            return CheckSpecialCall(expr);
        }
        for (const auto& arg : expr.args) {
            if (IsArray(arg->type) && expr.text == "abs") {
                Error("For the length of a vector, write ‖v‖ (\\norm) or norm(v).", arg->ref);
                return Type::Error;
            }
        }
        switch (function->result) {
            case BuiltinFunction::Result::Special: break;
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
