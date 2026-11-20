#include "Ast.h"

namespace lang {

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
