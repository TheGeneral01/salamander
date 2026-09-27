#include "compiler/internal/Eft.h"
#include <cassert>
#include <memory>

static bool containsFunctionOrCall(const Expr* expr) {
    if (!expr) return false;
    if (dynamic_cast<const CallExpr*>(expr)) return true;
    if (auto* binary = dynamic_cast<const BinOpExpr*>(expr)) {
        return containsFunctionOrCall(binary->left.get()) || containsFunctionOrCall(binary->right.get());
    }
    if (auto* unary = dynamic_cast<const UnOpExpr*>(expr)) {
        return containsFunctionOrCall(unary->value.get());
    }
    if (auto* list = dynamic_cast<const ListExpr*>(expr)) {
        for (const auto& item : list->itms) {
            if (containsFunctionOrCall(item.get())) return true;
        }
    }
    return false;
}

static bool containsFunctionOrCall(const std::vector<std::unique_ptr<Stmt>>& statements) {
    for (const auto& stmt : statements) {
        if (dynamic_cast<const FuncDefStmt*>(stmt.get())) return true;
        if (auto* classDef = dynamic_cast<const ClassDefStmt*>(stmt.get())) {
            if (containsFunctionOrCall(classDef->body)) return true;
        } else if (auto* var = dynamic_cast<const VarDeclStmt*>(stmt.get())) {
            if (containsFunctionOrCall(var->init.get())) return true;
        } else if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt.get())) {
            if (containsFunctionOrCall(ret->body.get())) return true;
        } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt.get())) {
            if (containsFunctionOrCall(exprStmt->expr.get())) return true;
        } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
            if (containsFunctionOrCall(branch->condition.get()) ||
                containsFunctionOrCall(branch->thenBody) || containsFunctionOrCall(branch->elseBody)) return true;
        } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
            if (containsFunctionOrCall(loop->condition.get()) || containsFunctionOrCall(loop->body)) return true;
        }
    }
    return false;
}

int main() {
    std::vector<std::unique_ptr<Stmt>> ast;

    auto classDef = std::make_unique<ClassDefStmt>();
    classDef->name = makeToken(CLASS, "Counter", 0, 0);

    auto member = std::make_unique<VarDeclStmt>();
    member->name = makeToken(IDENTIFIER, "value", 1, 4);
    classDef->body.push_back(std::move(member));

    auto method = std::make_unique<FuncDefStmt>();
    method->name = makeToken(IDENTIFIER, "increment", 2, 4);
    method->params.push_back({makeToken(IDENTIFIER, "amount", 2, 14),
                              makeToken(TYPE, "int", 2, 7), false});
    auto returnStmt = std::make_unique<ReturnStmt>();
    auto addition = std::make_unique<BinOpExpr>();
    addition->left = std::make_unique<VarExpr>();
    static_cast<VarExpr*>(addition->left.get())->name = makeToken(IDENTIFIER, "amount", 2, 0);
    addition->op = makeToken(ADD, "+", 2, 0);
    addition->right = std::make_unique<LiteralExpr>();
    static_cast<LiteralExpr*>(addition->right.get())->value = makeToken(INTLIT, "1", 2, 0);
    returnStmt->body = std::move(addition);
    method->body.push_back(std::move(returnStmt));
    classDef->body.push_back(std::move(method));
    ast.push_back(std::move(classDef));

    auto result = std::make_unique<VarDeclStmt>();
    result->name = makeToken(IDENTIFIER, "result", 4, 0);
    auto call = std::make_unique<CallExpr>();
    call->funcName = makeToken(IDENTIFIER, "Counter:increment", 4, 9);
    auto argument = std::make_unique<LiteralExpr>();
    argument->value = makeToken(INTLIT, "4", 4, 28);
    call->args.push_back(std::move(argument));
    result->init = std::move(call);
    ast.push_back(std::move(result));

    auto procedure = std::make_unique<FuncDefStmt>();
    procedure->name = makeToken(IDENTIFIER, "record", 5, 0);
    procedure->params.push_back({makeToken(IDENTIFIER, "input", 5, 7),
                                 makeToken(TYPE, "int", 5, 0), false});
    auto localValue = std::make_unique<VarDeclStmt>();
    localValue->name = makeToken(IDENTIFIER, "local_value", 6, 4);
    localValue->init = std::make_unique<VarExpr>();
    static_cast<VarExpr*>(localValue->init.get())->name = makeToken(IDENTIFIER, "input", 6, 18);
    procedure->body.push_back(std::move(localValue));
    ast.push_back(std::move(procedure));

    auto procedureCall = std::make_unique<ExprStmt>();
    auto recordCall = std::make_unique<CallExpr>();
    recordCall->funcName = makeToken(IDENTIFIER, "record", 8, 0);
    auto recordArgument = std::make_unique<LiteralExpr>();
    recordArgument->value = makeToken(INTLIT, "9", 8, 7);
    recordCall->args.push_back(std::move(recordArgument));
    procedureCall->expr = std::move(recordCall);
    ast.push_back(std::move(procedureCall));

    auto freeFunction = std::make_unique<FuncDefStmt>();
    freeFunction->name = makeToken(IDENTIFIER, "main", 4, 0);
    freeFunction->body.push_back(std::make_unique<NullStmt>());
    ast.push_back(std::move(freeFunction));

    Preprocessor preprocessor;
    preprocessor.flattenAST(std::move(ast));

    auto* flattenedClass = dynamic_cast<ClassDefStmt*>(preprocessor.processedAST.front().get());
    assert(flattenedClass != nullptr);
    assert(flattenedClass->body.size() == 1);
    assert(dynamic_cast<VarDeclStmt*>(flattenedClass->body.front().get()) != nullptr);
    assert(preprocessor.processedAST.size() == 3);
    auto* flattenedResult = dynamic_cast<VarDeclStmt*>(preprocessor.processedAST[1].get());
    assert(flattenedResult != nullptr);
    auto* flattenedExpr = dynamic_cast<BinOpExpr*>(flattenedResult->init.get());
    assert(flattenedExpr != nullptr);
    auto* inlinedArgument = dynamic_cast<LiteralExpr*>(flattenedExpr->left.get());
    assert(inlinedArgument != nullptr && inlinedArgument->value.originalTxt == "4");
    auto* flattenedProcedureLocal = dynamic_cast<VarDeclStmt*>(preprocessor.processedAST[2].get());
    assert(flattenedProcedureLocal != nullptr);
    auto* statementArgument = dynamic_cast<LiteralExpr*>(flattenedProcedureLocal->init.get());
    assert(statementArgument != nullptr && statementArgument->value.originalTxt == "9");
    assert(!containsFunctionOrCall(preprocessor.processedAST));
}