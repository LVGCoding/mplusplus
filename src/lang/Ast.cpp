#include "Ast.h"

namespace lang {

std::string FieldInfo::TagValue(const std::string& tag) const {
    for (const Tag& t : tags) {
        if (t.name == tag) {
            return t.args.empty() ? "" : t.args[0];
        }
    }
    return "";
}

bool FieldInfo::HasTag(const std::string& tag) const {
    for (const Tag& t : tags) {
        if (t.name == tag) return true;
    }
    return false;
}

const FieldInfo* TypeInfo::Field(const std::string& name) const {
    for (const FieldInfo& field : fields) {
        if (field.name == name) return &field;
    }
    return nullptr;
}

bool SameType(const Type& a, const Type& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case Type::List: return SameType(a.info->element, b.info->element);
        case Type::Map:
            return SameType(a.info->element, b.info->element) &&
                   SameType(a.info->key, b.info->key);
        case Type::Struct: return a.info == b.info;
        default: return true;
    }
}

std::unique_ptr<Expr> Clone(const Expr& expr) {
    auto copy = std::make_unique<Expr>();
    copy->kind = expr.kind;
    copy->ref = expr.ref;
    copy->text = expr.text;
    copy->ident = expr.ident;
    copy->greek = expr.greek;
    copy->op = expr.op;
    copy->ops = expr.ops;
    copy->opRef = expr.opRef;
    copy->var = expr.var;
    copy->varIdent = expr.varIdent;
    copy->varGreek = expr.varGreek;
    copy->varRef = expr.varRef;
    copy->fields = expr.fields;
    copy->rows = expr.rows;
    copy->columns = expr.columns;
    for (const auto& arg : expr.args) {
        copy->args.push_back(Clone(*arg));
    }
    for (const auto& condition : expr.conditions) {
        copy->conditions.push_back(condition ? Clone(*condition) : nullptr);
    }
    return copy;
}

std::unique_ptr<Stmt> Clone(const Stmt& statement) {
    auto copy = std::make_unique<Stmt>();
    copy->kind = statement.kind;
    copy->ref = statement.ref;
    copy->name = statement.name;
    copy->ident = statement.ident;
    copy->greek = statement.greek;
    copy->nameRef = statement.nameRef;
    copy->declaredType = statement.declaredType;
    copy->typeRef = statement.typeRef;
    copy->dims = statement.dims;
    copy->mapKey = statement.mapKey;
    copy->list = statement.list;
    copy->choices = statement.choices;
    copy->member = statement.member;
    copy->memberRef = statement.memberRef;
    for (const auto& index : statement.indices) {
        copy->indices.push_back(Clone(*index));
    }
    copy->indexRef = statement.indexRef;
    if (statement.value) {
        copy->value = Clone(*statement.value);
    }
    copy->body = Clone(statement.body);
    copy->elseBody = Clone(statement.elseBody);
    copy->hasElse = statement.hasElse;
    copy->params = statement.params;
    return copy;
}

Block Clone(const Block& block) {
    Block copy;
    for (const auto& statement : block) {
        copy.push_back(Clone(*statement));
    }
    return copy;
}

}  // namespace lang
