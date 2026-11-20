#include "Checker.h"

#include "Builtins.h"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <map>
#include <string>

namespace lang {

std::string TypeName(const Type& type) {
    switch (type.kind) {
        case Type::Struct: return type.info->name;
        case Type::Figure: return "Figure";
        case Type::List: return TypeName(type.info->element) + "*";
        case Type::Map:
            return TypeName(type.info->element) + "^(" + TypeName(type.info->key) + ")";
        case Type::Int: return "ℤ";
        case Type::Real: return "ℝ";
        case Type::Bool: return "𝔹";
        case Type::String: return "Σ*";
        case Type::Vector: return "ℝ^n";
        case Type::Matrix: return "ℝ^(n×m)";
        case Type::SetInt: return "sets of ℤ";
        case Type::SetReal: return "sets of ℝ";
        case Type::SetString: return "sets of Σ*";
        case Type::EmptySet: return "∅";
        case Type::Void: return "nothing";
        case Type::Error: return "?";
    }
    return "?";
}

namespace {

bool IsNumeric(Type type) { return type == Type::Int || type == Type::Real; }
bool IsArray(Type type) { return type == Type::Vector || type == Type::Matrix; }
bool IsSet(Type type) {
    return type == Type::SetInt || type == Type::SetReal || type == Type::SetString ||
           type == Type::EmptySet;
}

// What a set holds (Error for ∅), and the set holding a type (Error if none).
Type ElementOf(Type set) {
    switch (set.kind) {
        case Type::SetInt: return Type::Int;
        case Type::SetReal: return Type::Real;
        case Type::SetString: return Type::String;
        default: return Type::Error;
    }
}
Type SetOf(Type element) {
    switch (element.kind) {
        case Type::Int: return Type::SetInt;
        case Type::Real: return Type::SetReal;
        case Type::String: return Type::SetString;
        default: return Type::Error;
    }
}

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
    if (set == "Σ*") return Type::String;
    return Type::Error;
}

// The type of a value that can be either of two types, e.g. the cases of a
// piecewise function. Sets `ok` to false if they don't fit together.
Type Unify(Type a, Type b, bool& ok) {
    ok = true;
    if (a == Type::Error || b == Type::Error) return Type::Error;
    if (IsNumeric(a) && IsNumeric(b)) return Join(a, b);
    if (a == b) {
        ok = SameType(a, b);
        return ok ? a : Type(Type::Error);
    }
    // ∅ fits any set; a set of ℤ fits with a set of ℝ.
    if (a == Type::EmptySet && IsSet(b)) return b;
    if (b == Type::EmptySet && IsSet(a)) return a;
    if ((a == Type::SetInt && b == Type::SetReal) || (a == Type::SetReal && b == Type::SetInt)) {
        return Type::SetReal;
    }
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
        for (const auto& info : structs_) {
            program.structs.push_back(info);
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
        // Declared with choices (x ∈ {"a", "b"}): what it may hold.
        std::shared_ptr<Expr> choices;
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
    // Struct types: those with names (Person := ⟨…⟩), and every struct type in
    // the order made, for the code generator.
    std::map<std::string, Type> structTypes_;
    std::vector<std::shared_ptr<TypeInfo>> structs_;
    std::map<std::string, Type> anonymousStructs_;
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
            case Stmt::Kind::Graph: CheckGraph(statement); break;
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
    bool CheckStore(Type target, Type value, const std::string& name, const std::string& targetName,
                    const SourceRef& ref) {
        if (target == value && (target == Type::Struct ? StructsFit(target, value)
                                                       : SameType(target, value))) {
            return true;
        }
        if (target == value && target != Type::Error) {
            Error(name + " is in " + targetName + ", but this is in " + TypeName(value) + ".",
                  ref);
            return false;
        }
        if (target == Type::Error || value == Type::Error ||
            (target == Type::Real && value == Type::Int) ||
            (IsSet(target) && value == Type::EmptySet) ||
            (target == Type::SetReal && value == Type::SetInt)) {
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
    Type DeclaredType(const std::string& set, const SourceRef& ref, bool& natural,
                      bool report = true) {
        if (auto named = structTypes_.find(set); named != structTypes_.end()) {
            natural = false;
            return named->second;
        }
        const Type type = SetType(set, natural);
        if (type == Type::Error && report) {
            if (set == "Σ") {
                Error("Σ alone is a single character, which M++ doesn't have yet; for text, "
                      "write Σ*.",
                      ref);
            } else if (!set.empty() && (set[0] & 0x80) == 0) {
                Error(set + " isn't a type. Define it first, as in " + set +
                          " := ⟨name ∈ Σ*⟩.",
                      ref);
            } else {
                Error(set + " can't be used yet.", ref);
            }
        }
        return type;
    }

    static Type MakeList(const Type& element) {
        auto info = std::make_shared<TypeInfo>();
        info->element = element;
        return Type(Type::List, info);
    }

    static Type MakeMap(const Type& key, const Type& value) {
        auto info = std::make_shared<TypeInfo>();
        info->key = key;
        info->element = value;
        return Type(Type::Map, info);
    }

    // The full type of a declaration (Param or Stmt): set, sizes, map keys,
    // choices and list. Reports problems if `report`; checks sizes if
    // `checkSizes`.
    template <class Spec>
    Type ResolveType(const Spec& spec, bool& natural, bool report, bool checkSizes) {
        natural = false;
        Type type;
        if (spec.choices) {
            const Type choices = report ? CheckExpr(*spec.choices) : spec.choices->type;
            if (!IsSet(choices) || choices == Type::EmptySet) {
                if (report && choices != Type::Error) {
                    Error("Choices are written as a set of values, as in {\"a\", \"b\"}.",
                          spec.typeRef);
                }
                return Type::Error;
            }
            type = ElementOf(choices);
        } else if (!spec.mapKey.empty()) {
            bool ignored = false;
            const Type key = DeclaredType(spec.mapKey, spec.typeRef, ignored, report);
            const Type value = DeclaredType(spec.declaredType, spec.typeRef, natural, report);
            if (key == Type::Error || value == Type::Error) return Type::Error;
            if (key != Type::Int && key != Type::String && key != Type::Real) {
                if (report) Error("A map's keys must be numbers or strings.", spec.typeRef);
                return Type::Error;
            }
            natural = false;
            type = MakeMap(key, value);
        } else if (!spec.dims.empty()) {
            type = report ? DeclaredWithDims(spec.declaredType, spec.typeRef, spec.dims, natural,
                                             checkSizes)
                          : WithDims(DeclaredType(spec.declaredType, spec.typeRef, natural, false),
                                     spec.dims.size());
        } else {
            type = DeclaredType(spec.declaredType, spec.typeRef, natural, report);
        }
        if (spec.list && type != Type::Error) {
            natural = false;
            type = MakeList(type);
        }
        return type;
    }

    // A parameter's declared type, already checked by CheckFunction.
    Type ParamType(const Param& param) {
        bool natural = false;
        return ResolveType(param, natural, false, false);
    }

    // Can a struct of type `value` be stored in one of type `target`? Yes if
    // they have the same fields, of the same types.
    static bool StructsFit(const Type& target, const Type& value) {
        if (target.info == value.info) return true;
        if (target.info->fields.size() != value.info->fields.size()) return false;
        for (const FieldInfo& field : target.info->fields) {
            const FieldInfo* other = value.info->Field(field.name);
            if (!other || !SameType(field.type, other->type)) return false;
        }
        return true;
    }

    // Can a @default(…) value written as `text` be a value of `type`?
    static bool DefaultFits(const std::string& text, const Type& type) {
        if (type == Type::String) return true;
        if (type == Type::Bool) return text == "true" || text == "false";
        if (type == Type::Int || type == Type::Real) {
            if (text.empty()) return false;
            bool point = false;
            for (std::size_t i = 0; i < text.size(); ++i) {
                const char c = text[i];
                if (c == '-' && i == 0) continue;
                if (c == '.' && !point && type == Type::Real) {
                    point = true;
                    continue;
                }
                if (c < '0' || c > '9') return false;
            }
            return true;
        }
        return false;
    }

    // A struct type from fields declared in ⟨…⟩. `name` is empty for one
    // without a name; those with the same fields are shared.
    Type MakeStruct(const std::string& name, const std::string& ident, Expr& definition) {
        auto info = std::make_shared<TypeInfo>();
        bool ok = true;
        for (Param& field : definition.fields) {
            for (const FieldInfo& before : info->fields) {
                if (before.name == field.name) {
                    Error("There are two fields called " + field.name + ".", field.ref);
                    ok = false;
                }
            }
            bool natural = false;
            const Type type = ResolveType(field, natural, true, true);
            ok &= type != Type::Error;
            FieldInfo info_field{.name = field.name, .ident = field.ident, .greek = field.greek,
                                 .type = type, .tags = field.tags};
            for (const Tag& tag : field.tags) {
                static const char* KNOWN[] = {"label",  "align", "filler", "format", "prefix",
                                              "postfix", "unit", "hidden", "default"};
                bool known = false;
                for (const char* k : KNOWN) known |= tag.name == k;
                if (!known) {
                    Error("There's no tag @" + tag.name + ". Tags are @label, @align, @filler, "
                          "@format, @prefix, @postfix, @unit, @hidden and @default.",
                          tag.ref);
                    ok = false;
                } else if (tag.name != "hidden" && tag.args.size() != 1) {
                    Error("@" + tag.name + " takes one value, as in @" + tag.name + "(\"…\").",
                          tag.ref);
                    ok = false;
                } else if (tag.name == "default" && !DefaultFits(tag.args[0], type)) {
                    Error("@default(" + tag.args[0] + ") doesn't fit " + field.name + ", which is in " +
                              TypeName(type) + ".",
                          tag.ref);
                    ok = false;
                } else if (tag.name == "align" && tag.args[0] != "left" &&
                           tag.args[0] != "right" && tag.args[0] != "center") {
                    Error("@align is \"left\", \"right\" or \"center\".", tag.ref);
                    ok = false;
                }
            }
            info->fields.push_back(std::move(info_field));
        }
        if (!ok) {
            return Type::Error;
        }
        return RegisterStruct(std::move(info), name, ident);
    }

    // Names a struct type and adds it to those the code generator writes. One
    // without a name is shared with any other of the same fields and tags.
    Type RegisterStruct(std::shared_ptr<TypeInfo> info, const std::string& name,
                        const std::string& ident) {
        std::string described = "⟨";
        for (std::size_t i = 0; i < info->fields.size(); ++i) {
            described += (i ? ", " : "") + info->fields[i].name + " ∈ " +
                         TypeName(info->fields[i].type);
        }
        described += "⟩";
        if (name.empty()) {
            // Tags matter to how it prints, so they are part of what is shared.
            std::string key = described;
            for (const FieldInfo& field : info->fields) {
                for (const Tag& tag : field.tags) {
                    key += "@" + field.name + "." + tag.name;
                    for (const auto& arg : tag.args) key += "(" + arg + ")";
                }
            }
            if (auto found = anonymousStructs_.find(key); found != anonymousStructs_.end()) {
                return found->second;
            }
            info->name = described;
            info->cppName = "S" + std::to_string(structs_.size());
            structs_.push_back(info);
            Type type(Type::Struct, info);
            anonymousStructs_.emplace(key, type);
            return type;
        }
        info->name = name;
        info->cppName = "S_" + ident;
        structs_.push_back(info);
        return Type(Type::Struct, info);
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
        if (const Variable* variable = Lookup(statement.name);
            variable && (variable->type == Type::Map || variable->type == Type::List)) {
            // m_{key} := v is the same as m_{key} ← v.
            CheckAssignElement(statement);
            return;
        }
        const Type value = statement.value ? CheckExpr(*statement.value) : Type(Type::Error);
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
        // T := ⟨field ∈ type, …⟩ names a struct type.
        if (statement.value && statement.value->kind == Expr::Kind::StructType) {
            CheckTypeDefinition(statement);
            return;
        }
        const bool declared = !statement.declaredType.empty() || statement.choices;
        Type type = Type::Error;
        if (declared) {
            type = ResolveType(statement, statement.natural, true, true);
        }
        if (statement.value) {
            const Type value =
                declared ? CheckExprAs(*statement.value, type) : CheckExpr(*statement.value);
            if (!declared) {
                type = value == Type::EmptySet ? Type::SetInt : value;
                if (value == Type::Void) {
                    Error("This doesn't give a value to store in " + statement.name + ".",
                          statement.value->ref);
                    type = Type::Error;
                }
            } else {
                CheckStore(type, value, statement.name,
                           statement.natural ? std::string("ℕ") : TypeName(type),
                           statement.value->ref);
            }
        }
        statement.type = type;
        if (CheckNewName(statement.name, statement.nameRef)) {
            scopes_.back()[statement.name] = {type, statement.natural, false, statement.nameRef,
                                              statement.choices};
        }
    }

    // Person := ⟨name ∈ Σ*, age ∈ ℝ⟩
    void CheckTypeDefinition(Stmt& statement) {
        statement.access = "type";
        if (context_ || scopes_.size() > 1) {
            Error("Struct types are defined outside of blocks and functions.", statement.nameRef);
            return;
        }
        if (structTypes_.contains(statement.name)) {
            Error(statement.name + " is already a type.", statement.nameRef);
            return;
        }
        if (!statement.declaredType.empty()) {
            Error("A struct type has no type of its own; write " + statement.name + " := ⟨…⟩.",
                  statement.typeRef);
        }
        if (!CheckNewName(statement.name, statement.nameRef)) {
            return;
        }
        const Type type = MakeStruct(statement.name, statement.ident, *statement.value);
        if (type != Type::Error) {
            structTypes_[statement.name] = type;
        }
    }

    // `expr` where a value of type `expected` is wanted: ⟨…⟩ and […] take
    // their type from there.
    Type CheckExprAs(Expr& expr, const Type& expected) {
        if (expected == Type::Struct && expr.kind == Expr::Kind::StructValue) {
            expr.type = CheckStructValue(expr, &expected);
            return expr.type;
        }
        if (expected == Type::List && expr.kind == Expr::Kind::List) {
            const Type& element = expected.info->element;
            bool ok = true;
            for (auto& item : expr.args) {
                const Type type = CheckExprAs(*item, element);
                ok &= CheckStore(element, type, "This list", TypeName(expected), item->ref);
            }
            expr.type = ok ? expected : Type(Type::Error);
            return expr.type;
        }
        return CheckExpr(expr);
    }

    // ⟨name = "Bob", age = 4⟩: of the type `target`, or of a struct type made
    // from its fields.
    Type CheckStructValue(Expr& expr, const Type* target) {
        bool ok = true;
        for (std::size_t i = 0; i < expr.ops.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                if (expr.ops[j] == expr.ops[i]) {
                    Error("There are two fields called " + expr.ops[i] + ".", expr.args[i]->ref);
                    ok = false;
                }
            }
        }
        if (target) {
            const TypeInfo& info = *target->info;
            for (std::size_t i = 0; i < expr.ops.size(); ++i) {
                const FieldInfo* field = info.Field(expr.ops[i]);
                if (!field) {
                    CheckExpr(*expr.args[i]);
                    Error(info.name + " has no field called " + expr.ops[i] + ".",
                          expr.args[i]->ref);
                    ok = false;
                    continue;
                }
                const Type value = CheckExprAs(*expr.args[i], field->type);
                ok &= CheckStore(field->type, value, field->name, TypeName(field->type),
                                 expr.args[i]->ref);
            }
            for (const FieldInfo& field : info.fields) {
                bool given = false;
                for (const auto& name : expr.ops) given |= name == field.name;
                if (!given && !field.HasTag("default")) {
                    Error("This leaves out " + field.name + ": give it a value, or the field a "
                          "@default(…).",
                          expr.ref);
                    ok = false;
                }
            }
            return ok ? *target : Type(Type::Error);
        }
        auto info = std::make_shared<TypeInfo>();
        for (std::size_t i = 0; i < expr.ops.size(); ++i) {
            const Type value = CheckExpr(*expr.args[i]);
            if (value == Type::Void) {
                Error("This doesn't give a value.", expr.args[i]->ref);
            }
            if (value == Type::Error || value == Type::Void) {
                ok = false;
                continue;
            }
            info->fields.push_back(FieldInfo{.name = expr.ops[i], .ident = expr.ops[i],
                                             .type = value == Type::EmptySet ? Type::SetInt
                                                                             : value});
        }
        return ok ? RegisterStruct(std::move(info), "", "") : Type(Type::Error);
    }

    // The type a type expression stands for: ℝ, Σ*, Person, ⟨…⟩ (for T*).
    Type TypeFromExpr(Expr& expr) {
        switch (expr.kind) {
            case Expr::Kind::TypeName: {
                bool natural = false;
                if (expr.text == "ℕ") {
                    Error("A list of ℕ isn't possible yet; use ℤ*.", expr.ref);
                    return Type::Error;
                }
                return DeclaredType(expr.text, expr.ref, natural);
            }
            case Expr::Kind::Name: {
                bool natural = false;
                return DeclaredType(expr.text, expr.ref, natural);
            }
            case Expr::Kind::StructType: return MakeStruct("", "", expr);
            default:
                Error("Expected a type before *, such as ℝ* or Person*.", expr.ref);
                return Type::Error;
        }
    }

    // x_i ← value: an entry of a vector or matrix, or an element of a family.
    void CheckAssignElement(Stmt& statement) {
        const std::string& name = statement.name;
        if (const Variable* variable = Lookup(name);
            variable && (variable->type == Type::Map || variable->type == Type::List)) {
            const bool map = variable->type == Type::Map;
            if (variable->loop) {
                Error(name + " counts the loop, so it can't be changed inside it.",
                      statement.nameRef);
                return;
            }
            if (statement.indices.size() != 1) {
                Error(name + " has one index, as in " + name + "_i.", statement.indexRef);
                return;
            }
            const Type wanted = map ? variable->type.info->key : Type(Type::Int);
            const Type index = CheckExpr(*statement.indices[0]);
            if (index != Type::Error && !(index == wanted || (wanted == Type::Real &&
                                                              index == Type::Int))) {
                Error(map ? "The keys of " + name + " are in " + TypeName(wanted) +
                                ", but this is in " + TypeName(index) + "."
                          : "An index must be a whole number (ℤ), but this is in " +
                                TypeName(index) + ".",
                      statement.indices[0]->ref);
                return;
            }
            statement.access = map ? "map" : "list";
            statement.type = variable->type.info->element;
            const Type value = CheckExprAs(*statement.value, statement.type);
            CheckStore(statement.type, value, name + "_…", TypeName(statement.type),
                       statement.value->ref);
            return;
        }
        const Type value = CheckExpr(*statement.value);
        if (!RequireWholeIndices(statement.indices)) {
            return;
        }
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

    // p.name ← value
    void CheckAssignMember(Stmt& statement) {
        const Variable* variable = Lookup(statement.name);
        if (!variable) {
            CheckExpr(*statement.value);
            Error(statement.name + " isn't defined.", statement.nameRef);
            return;
        }
        if (variable->type != Type::Struct) {
            CheckExpr(*statement.value);
            if (variable->type != Type::Error) {
                Error(statement.name + " is in " + TypeName(variable->type) +
                          ", which has no fields.",
                      statement.nameRef);
            }
            return;
        }
        const FieldInfo* field = variable->type.info->Field(statement.member);
        if (!field) {
            CheckExpr(*statement.value);
            Error(TypeName(variable->type) + " has no field called " + statement.member + ".",
                  statement.memberRef);
            return;
        }
        statement.type = field->type;
        const Type value = CheckExprAs(*statement.value, field->type);
        CheckStore(field->type, value, statement.member, TypeName(field->type),
                   statement.value->ref);
    }

    void CheckAssign(Stmt& statement) {
        if (!statement.member.empty()) {
            CheckAssignMember(statement);
            return;
        }
        if (!statement.indices.empty()) {
            if (!IsLabel(statement.name, statement.indices)) {
                CheckAssignElement(statement);
                return;
            }
            ApplyLabel(statement);
        }
        const Variable* found = Lookup(statement.name);
        const Type value = found ? CheckExprAs(*statement.value, found->type)
                                 : CheckExpr(*statement.value);
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
        statement.choices = variable->choices;
        CheckStore(statement.type, value, statement.name,
                   statement.natural ? std::string("ℕ") : TypeName(statement.type), statement.value->ref);
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
        statement.type = CheckDomain(*statement.value, "for");
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

    // What a variable goes through: a range (a..b), a set or a vector.
    // Returns the type of its elements.
    Type CheckDomain(Expr& domain, const char* what) {
        if (domain.kind == Expr::Kind::Range) {
            Type type = Type::Int;
            bool ok = true;
            for (auto& bound : domain.args) {
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
            domain.type = ok ? SetOf(type) : Type::Error;
            return ok ? type : Type::Error;
        }
        const Type type = CheckExpr(domain);
        if (type == Type::Error) {
            return Type::Error;
        }
        if (type == Type::EmptySet) {
            domain.type = Type::SetInt;
            return Type::Int;
        }
        if (IsSet(type)) {
            return ElementOf(type);
        }
        if (type == Type::Vector) {
            return Type::Real;
        }
        if (type == Type::List) {
            return type.info->element;
        }
        if (type == Type::Map) {
            return type.info->key;  // a map is gone through by its keys
        }
        Error(std::string(what) + " needs a range (1..n), a set or a vector to go through, but "
                                  "this is in " + TypeName(type) + ".",
              domain.ref);
        return Type::Error;
    }

    // Checks `body` with the expression's variable bound to each element of
    // its domain (args[0]); returns the body's type.
    Type CheckBound(Expr& expr, Expr& body, const char* what, Expr* condition = nullptr) {
        const Type element = CheckDomain(*expr.args[0], what);
        scopes_.emplace_back();
        // A bound variable may reuse a name from outside, like k in Σ_k.
        if (FindBuiltinFunction(expr.var) || functions_.contains(expr.var)) {
            Error(expr.var + " is a function; choose another name.", expr.varRef);
        }
        scopes_.back()[expr.var] = {element, false, true, expr.varRef};
        if (condition) {
            RequireCondition(*condition, "the :");
        }
        const Type type = element == Type::Error ? Type::Error : CheckExpr(body);
        scopes_.pop_back();
        return element == Type::Error ? Type::Error : type;
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
            if (!param.declaredType.empty() || param.choices) {
                ok &= ResolveType(param, natural, true, false) != Type::Error;
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
            ok &= ResolveType(statement, natural, true, false) != Type::Error;
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
            bool same = existing->params.size() == types.size();
            for (std::size_t i = 0; same && i < types.size(); ++i) {
                same = SameType(existing->params[i], types[i]);
            }
            if (same) {
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
            instance.result = ResolveType(definition, instance.naturalResult, false, false);
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
            case Expr::Kind::Range: {
                // As a value, a range is the set of its numbers.
                const Type element = CheckDomain(expr, "A range");
                return element == Type::Error ? Type::Error : SetOf(element);
            }
            case Expr::Kind::EmptySet: return Type::EmptySet;
            case Expr::Kind::Set: return CheckSetLiteral(expr);
            case Expr::Kind::Builder: {
                Expr* condition = expr.conditions[0].get();
                const Type element = CheckBound(expr, *expr.args[1], "A set", condition);
                if (element == Type::Error) {
                    return Type::Error;
                }
                const Type set = SetOf(element);
                if (set == Type::Error) {
                    Error("A set can hold numbers or strings, not " +
                              std::string(TypeName(element)) + ".",
                          expr.args[1]->ref);
                }
                return set;
            }
            case Expr::Kind::Quantifier: {
                const Type type = CheckBound(expr, *expr.args[1], expr.text.c_str());
                if (type != Type::Bool && type != Type::Error) {
                    Error("After " + expr.text + " x ∈ S : comes a condition, true or false (𝔹); "
                          "this is in " + TypeName(type) + ".",
                          expr.args[1]->ref);
                    return Type::Error;
                }
                return type;
            }
            case Expr::Kind::BigOp: return CheckBigOp(expr);
            case Expr::Kind::TypeName:
                Error(expr.text + " is a type; it can be used after ∈ (x ∈ " + expr.text +
                          " := …), or as " + expr.text + "* for a new list.",
                      expr.ref);
                return Type::Error;
            case Expr::Kind::StructType:
                Error("⟨… ∈ …⟩ describes a struct type: name it (Person := ⟨…⟩), or make a list "
                      "of them with ⟨…⟩*.",
                      expr.ref);
                return Type::Error;
            case Expr::Kind::StructValue: return CheckStructValue(expr, nullptr);
            case Expr::Kind::ListOf: {
                const Type element = TypeFromExpr(*expr.args[0]);
                return element == Type::Error ? Type(Type::Error) : MakeList(element);
            }
            case Expr::Kind::List: {
                Type element;
                bool first = true;
                for (auto& item : expr.args) {
                    const Type type = CheckExpr(*item);
                    if (type == Type::Error) return Type::Error;
                    if (type == Type::Void) {
                        Error("This doesn't give a value.", item->ref);
                        return Type::Error;
                    }
                    bool ok = true;
                    element = first ? type : Unify(element, type, ok);
                    first = false;
                    if (!ok) {
                        Error("A list's elements must all be of one type.", item->ref);
                        return Type::Error;
                    }
                }
                return MakeList(element == Type::EmptySet ? Type(Type::SetInt) : element);
            }
            case Expr::Kind::Member: {
                const Type object = CheckExpr(*expr.args[0]);
                if (object == Type::Error) return Type::Error;
                if (object != Type::Struct) {
                    Error("This is in " + TypeName(object) + ", which has no fields.",
                          expr.args[0]->ref);
                    return Type::Error;
                }
                const FieldInfo* field = object.info->Field(expr.text);
                if (!field) {
                    Error(TypeName(object) + " has no field called " + expr.text + ".",
                          expr.opRef);
                    return Type::Error;
                }
                return field->type;
            }
            case Expr::Kind::Method: return CheckMethod(expr, statement);
            case Expr::Kind::Template:
                if (CheckArgs(expr) == Type::Error) return Type::Error;
                return Type::String;
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
        if (op == "∪" || op == "∩" || op == "Δ" || (op == "∖" && left != Type::Matrix) ||
            IsSet(left) || IsSet(right)) {
            return CheckSetBinary(expr);
        }
        if (IsArray(left) || IsArray(right) || op == "⊙" || op == "∖") {
            return CheckArrayBinary(expr);
        }
        if ((left == Type::String || right == Type::String) &&
            (op == "+" || op == "⋅" || op == "×")) {
            if (op == "+" && left == Type::String && right == Type::String) {
                return Type::String;  // joined
            }
            const bool repeat = op == "⋅" || op == "×";
            if (repeat && ((left == Type::String && right == Type::Int) ||
                           (left == Type::Int && right == Type::String))) {
                expr.access = "repeat";
                return Type::String;
            }
            Error(op == "+" ? "+ joins two strings. To join other values, use a template: "
                              "\"x = ${x}\"."
                  : "A string can be repeated a whole number of times: \"ab\"⋅3.",
                  expr.opRef.Empty() ? expr.ref : expr.opRef);
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

    // ∪ ∩ ∖ Δ between sets. Sets expr.access to the runtime function.
    Type CheckSetBinary(Expr& expr) {
        const std::string& op = expr.op;
        const Type left = expr.args[0]->type;
        const Type right = expr.args[1]->type;
        if (op != "∪" && op != "∩" && op != "∖" && op != "Δ") {
            Error(op == "+" ? "To join two sets, write A ∪ B."
                            : op + " can't be used with sets; they have ∪ ∩ ∖ Δ.",
                  expr.opRef);
            return Type::Error;
        }
        for (const Expr* side : {expr.args[0].get(), expr.args[1].get()}) {
            if (!IsSet(side->type)) {
                Error(op + " needs two sets, but this is in " + TypeName(side->type) + ".",
                      side->ref);
                return Type::Error;
            }
        }
        bool ok = true;
        Type type = Unify(left, right, ok);
        if (!ok) {
            Error("Can't combine " + std::string(TypeName(left)) + " with " + TypeName(right) +
                      ".",
                  expr.opRef);
            return Type::Error;
        }
        expr.access = op == "∪" ? "set_union" : op == "∩" ? "set_intersection"
                    : op == "∖" ? "set_difference" : "set_symmetric";
        return type == Type::EmptySet ? Type::SetInt : type;
    }

    // {a, b, 1..n}: numbers or strings, all of one kind.
    Type CheckSetLiteral(Expr& expr) {
        Type element = Type::Error;
        bool first = true;
        bool failed = false;
        for (auto& item : expr.args) {
            Type type;
            if (item->kind == Expr::Kind::Range) {
                type = CheckDomain(*item, "A range");
            } else {
                type = CheckExpr(*item);
                if (type != Type::Error && SetOf(type) == Type::Error) {
                    Error("A set can hold numbers or strings, not " +
                              std::string(TypeName(type)) + ".",
                          item->ref);
                    type = Type::Error;
                }
            }
            if (type == Type::Error) {
                failed = true;
                continue;
            }
            if (first) {
                element = type;
                first = false;
                continue;
            }
            bool ok = true;
            element = Unify(element, type, ok);
            if (!ok) {
                Error("A set's elements must all be numbers or all be strings.", item->ref);
                failed = true;
            }
        }
        return failed ? Type::Error : SetOf(element);
    }

    // Is `type` numbers to plot: a vector, or a list of ℤ or ℝ?
    static bool IsData(const Type& type) {
        return type == Type::Vector ||
               (type == Type::List && IsNumeric(type.info->element));
    }

    // A graph in the document: each line is checked as a function of its x
    // variable, or as data.
    void CheckGraph(Stmt& statement) {
        for (auto& plot : statement.plots) {
            if (!plot) {
                continue;
            }
            if (plot->kind == Expr::Kind::Call && plot->text == "points") {
                for (auto& data : plot->args) {
                    const Type type = CheckExpr(*data);
                    if (type != Type::Error && !IsData(type)) {
                        Error("Data in a graph is a vector or a list of numbers, but this is in " +
                                  TypeName(type) + ".",
                              data->ref);
                    }
                }
                continue;
            }
            scopes_.emplace_back();
            scopes_.back()[statement.name] = {Type::Real, false, true, statement.nameRef};
            const Type type = CheckExpr(*plot);
            scopes_.pop_back();
            if (type != Type::Error && !IsNumeric(type)) {
                Error("A graph's line is a function of " + statement.name + ", like y = " +
                          statement.name + "², or data, like (xs, ys); this is in " +
                          TypeName(type) + ".",
                      plot->ref);
            }
        }
    }

    // The fields of a struct value given to Figure or one of its methods:
    // each must be one of `allowed` (field → what it must be), and those
    // in `required` must be there.
    enum class FieldKind { Text, Whole, Data, Labels };
    bool CheckOptions(Expr& options, const std::string& what,
                      const std::vector<std::pair<std::string, FieldKind>>& allowed,
                      const std::vector<std::string>& required) {
        if (options.kind != Expr::Kind::StructValue) {
            CheckExpr(options);
            Error(what + " takes its settings as ⟨name = value, …⟩.", options.ref);
            return false;
        }
        bool ok = true;
        for (std::size_t i = 0; i < options.ops.size(); ++i) {
            const std::string& name = options.ops[i];
            Expr& value = *options.args[i];
            const Type type = CheckExpr(value);
            const FieldKind* kind = nullptr;
            std::string names;
            for (const auto& [field, k] : allowed) {
                if (field == name) kind = &k;
                names += (names.empty() ? "" : ", ") + field;
            }
            if (!kind) {
                Error(what + " has no setting " + name + "; it has " + names + ".", value.ref);
                ok = false;
                continue;
            }
            if (type == Type::Error) {
                ok = false;
                continue;
            }
            const bool fits = *kind == FieldKind::Text   ? type == Type::String
                            : *kind == FieldKind::Whole  ? type == Type::Int
                            : *kind == FieldKind::Labels ? type == Type::List &&
                                                               type.info->element == Type::String
                                                         : IsData(type);
            if (!fits) {
                const char* wanted = *kind == FieldKind::Text    ? "a string"
                                     : *kind == FieldKind::Whole ? "a whole number (ℤ)"
                                     : *kind == FieldKind::Labels
                                         ? "a list of strings (Σ**)"
                                         : "a vector or a list of numbers";
                Error(name + " is " + wanted + ", but this is in " + TypeName(type) + ".",
                      value.ref);
                ok = false;
            }
        }
        for (const std::string& name : required) {
            bool given = false;
            for (const auto& field : options.ops) given |= field == name;
            if (!given) {
                Error(what + " needs " + name + " = ….", options.ref);
                ok = false;
            }
        }
        return ok;
    }

    // Figure() or Figure⟨title = …, xlabel, ylabel, scaleX, scaleY⟩
    Type CheckFigure(Expr& expr) {
        if (expr.args.size() > 1) {
            Error("Figure takes its settings as ⟨title = \"…\", scaleY = \"log\"⟩.", expr.ref);
            return Type::Error;
        }
        if (expr.args.size() == 1 &&
            !CheckOptions(*expr.args[0], "Figure",
                          {{"title", FieldKind::Text}, {"xlabel", FieldKind::Text},
                           {"ylabel", FieldKind::Text}, {"scaleX", FieldKind::Text},
                           {"scaleY", FieldKind::Text}},
                          {})) {
            return Type::Error;
        }
        return Type::Figure;
    }

    // fig.Line⟨…⟩, fig.Scatter⟨…⟩, fig.Histogram⟨…⟩, fig.Bar⟨…⟩
    Type CheckFigureMethod(Expr& expr, bool statement) {
        std::string name = expr.text;
        if (!name.empty()) name[0] = static_cast<char>(std::toupper(name[0]));
        using K = FieldKind;
        const std::pair<std::string, K> legend{"legend", K::Text}, color{"color", K::Text};
        bool ok = true;
        if (expr.args.size() != 2) {
            Error("fig." + name + " takes its data as ⟨…⟩, as in fig.Line⟨x = xs, y = ys⟩.",
                  expr.ref);
            return Type::Error;
        }
        Expr& options = *expr.args[1];
        if (name == "Line") {
            ok = CheckOptions(options, "Line", {{"x", K::Data}, {"y", K::Data}, legend, color},
                              {"x", "y"});
        } else if (name == "Scatter") {
            ok = CheckOptions(options, "Scatter",
                              {{"x", K::Data}, {"y", K::Data}, legend, color, {"shape", K::Text}},
                              {"x", "y"});
        } else if (name == "Histogram") {
            ok = CheckOptions(options, "Histogram",
                              {{"values", K::Data}, {"bins", K::Whole}, legend, color},
                              {"values"});
        } else if (name == "Bar") {
            ok = CheckOptions(options, "Bar",
                              {{"values", K::Data}, {"labels", K::Labels}, legend, color},
                              {"values"});
        } else {
            CheckExpr(options);
            Error("A Figure has Line, Scatter, Histogram and Bar.", expr.opRef);
            return Type::Error;
        }
        expr.text = name;
        if (!ok) return Type::Error;
        if (expr.args[0]->kind != Expr::Kind::Name) {
            Error("This changes the figure, so it needs the figure's name, as in fig." + name +
                      "⟨…⟩.",
                  expr.args[0]->ref);
            return Type::Error;
        }
        if (!statement) {
            Error(name + " doesn't give a value.", expr.ref);
            return Type::Error;
        }
        return Type::Void;
    }

    // list.push(value), and a figure's Line, Scatter, Histogram and Bar.
    Type CheckMethod(Expr& expr, bool statement) {
        Expr& object = *expr.args[0];
        const Type type = CheckExpr(object);
        if (type == Type::Error) {
            for (std::size_t i = 1; i < expr.args.size(); ++i) CheckExpr(*expr.args[i]);
            return Type::Error;
        }
        if (type == Type::Figure) {
            return CheckFigureMethod(expr, statement);
        }
        if (expr.text != "push" || type != Type::List) {
            Error(type == Type::List ? "Lists have one method: .push(x) adds x at the end."
                                     : "This is in " + TypeName(type) +
                                           ", which has no ." + expr.text + "(…).",
                  expr.opRef);
            return Type::Error;
        }
        if (object.kind != Expr::Kind::Name) {
            Error("push changes a list, so it needs the list's name, as in L.push(x).",
                  object.ref);
            return Type::Error;
        }
        if (Lookup(object.text) && Lookup(object.text)->loop) {
            Error(object.text + " counts the loop, so it can't be changed inside it.", object.ref);
            return Type::Error;
        }
        if (expr.args.size() != 2) {
            Error("push takes one value.", expr.ref);
            return Type::Error;
        }
        const Type& element = type.info->element;
        const Type value = CheckExprAs(*expr.args[1], element);
        if (!CheckStore(element, value, object.text, TypeName(type), expr.args[1]->ref)) {
            return Type::Error;
        }
        if (!statement) {
            Error("push doesn't give a value.", expr.ref);
            return Type::Error;
        }
        return Type::Void;
    }

    // Σ and ∏ of numbers, ⋃ and ⋂ of sets.
    Type CheckBigOp(Expr& expr) {
        const bool numbers = expr.text == "sum" || expr.text == "prod";
        const char* symbol = expr.text == "sum" ? "Σ" : expr.text == "prod" ? "∏"
                           : expr.text == "bigcup" ? "⋃" : "⋂";
        const Type type = CheckBound(expr, *expr.args[1], symbol);
        if (type == Type::Error) {
            return Type::Error;
        }
        if (numbers && !IsNumeric(type)) {
            Error(std::string(symbol) + " needs numbers, but this is in " + TypeName(type) + ".",
                  expr.args[1]->ref);
            return Type::Error;
        }
        if (!numbers && !IsSet(type)) {
            Error(std::string(symbol) + " needs sets, but this is in " + TypeName(type) + ".",
                  expr.args[1]->ref);
            return Type::Error;
        }
        return type == Type::EmptySet ? Type::SetInt : type;
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
            switch (type.kind) {
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
        if (type == Type::List || type == Type::Map) {
            const bool map = type == Type::Map;
            if (expr.args.size() != 2) {
                Error("A list or map has one index.", expr.opRef);
                return Type::Error;
            }
            const Type wanted = map ? type.info->key : Type(Type::Int);
            const Type index = CheckExpr(*expr.args[1]);
            if (index == Type::Error) return Type::Error;
            if (!(index == wanted || (wanted == Type::Real && index == Type::Int))) {
                Error(map ? "The keys here are in " + TypeName(wanted) + ", but this is in " +
                                TypeName(index) + "."
                          : "An index must be a whole number (ℤ), but this is in " +
                                TypeName(index) + ".",
                      expr.args[1]->ref);
                return Type::Error;
            }
            expr.access = map ? "map" : "list";
            return type.info->element;
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
            if (op == "in") {
                if (a != Type::String || b != Type::String) {
                    if (a != Type::Error && b != Type::Error) {
                        Error("in looks for text in a string (\"lo\" in \"hello\"); for sets "
                              "and lists, write ∈.",
                              Span(expr.args[i]->ref, expr.args[i + 1]->ref));
                    }
                    ok = false;
                }
                continue;
            }
            if (op == "∈" || op == "∉") {
                if (expr.ops.size() > 1) {
                    Error(op + " can't be chained with other comparisons; use ∧.", expr.opRef);
                    ok = false;
                    continue;
                }
                const Type element = CheckDomain(*expr.args[1], op.c_str());
                if (element != Type::Error && a != Type::Error &&
                    !(IsNumeric(element) && IsNumeric(a)) && element != a) {
                    Error("Can't look for " + std::string(TypeName(a)) + " among " +
                              TypeName(element) + ".",
                          expr.args[i]->ref);
                    ok = false;
                }
                ok &= element != Type::Error;
                continue;
            }
            if (op == "⊂" || op == "⊆" || IsSet(a) || IsSet(b)) {
                bool fits = true;
                Unify(a, b, fits);
                if (!IsSet(a) || !IsSet(b) || !fits ||
                    (op != "⊂" && op != "⊆" && op != "=" && op != "≠")) {
                    Error(op == "⊂" || op == "⊆"
                              ? op + " compares two sets."
                              : "Sets can be compared with = ≠ ⊂ ⊆, and tested with ∈.",
                          Span(expr.args[i]->ref, expr.args[i + 1]->ref));
                    ok = false;
                }
                continue;
            }
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
                             IsArray(declared) ? TypeName(declared) : param.declaredType,
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
        if (expr.text == "Figure") {
            return CheckFigure(expr);
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
        if (expr.text == "abs" && expr.args.size() == 1 &&
            (IsSet(expr.args[0]->type) || expr.args[0]->type == Type::String ||
             expr.args[0]->type == Type::List || expr.args[0]->type == Type::Map)) {
            expr.access = "size";
            return Type::Int;
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
