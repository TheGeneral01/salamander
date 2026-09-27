#include "compiler/internal/Eft.h"
#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/internal/var.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

static std::unique_ptr<LiteralExpr> integerLiteral(const std::string& value) {
    auto literal = std::make_unique<LiteralExpr>();
    literal->value = makeToken(INTLIT, value, 0, 0);
    return literal;
}

static std::unique_ptr<LiteralExpr> floatLiteral(const std::string& value) {
    auto literal = std::make_unique<LiteralExpr>();
    literal->value = makeToken(FLOATLIT, value, 0, 0);
    return literal;
}

static bool hasFunctionOrCall(const std::vector<std::unique_ptr<Stmt>>& statements);

static bool hasUserCall(const Expr* expr) {
    if (!expr) return false;
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) return call->funcName.originalTxt != "print";
    if (auto* binary = dynamic_cast<const BinOpExpr*>(expr)) {
        return hasUserCall(binary->left.get()) || hasUserCall(binary->right.get());
    }
    if (auto* unary = dynamic_cast<const UnOpExpr*>(expr)) return hasUserCall(unary->value.get());
    if (auto* cast = dynamic_cast<const CastExpr*>(expr)) return hasUserCall(cast->value.get());
    if (auto* test = dynamic_cast<const TypeIsExpr*>(expr)) return hasUserCall(test->value.get());
    if (auto* dispatch = dynamic_cast<const TypeDispatchExpr*>(expr)) {
        for (const auto& item : dispatch->cases) {
            if (hasUserCall(item.value.get())) return true;
            for (const auto& guard : item.guards) if (hasUserCall(guard.value.get())) return true;
            if (hasFunctionOrCall(item.statements)) return true;
        }
    }
    return false;
}

static bool hasFunctionOrCall(const std::vector<std::unique_ptr<Stmt>>& statements) {
    for (const auto& stmt : statements) {
        if (dynamic_cast<const FuncDefStmt*>(stmt.get())) return true;
        if (auto* variable = dynamic_cast<const VarDeclStmt*>(stmt.get())) {
            if (hasUserCall(variable->init.get())) return true;
        } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
            if (hasUserCall(branch->condition.get()) || hasFunctionOrCall(branch->thenBody) ||
                hasFunctionOrCall(branch->elseBody)) return true;
        } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
            if (hasUserCall(loop->condition.get()) || hasFunctionOrCall(loop->body)) return true;
        } else if (auto* expr = dynamic_cast<const ExprStmt*>(stmt.get())) {
            if (hasUserCall(expr->expr.get())) return true;
        } else if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt.get())) {
            if (hasUserCall(ret->body.get())) return true;
        }
    }
    return false;
}

int main() {
    lexer sourceLexer;
    std::ostringstream lexerDebugOutput;
    std::streambuf* previousOutput = std::cout.rdbuf(lexerDebugOutput.rdbuf());
    auto sourceTokens = sourceLexer.lexFile("test.sal");
    parser sourceParser(sourceTokens);
    auto parsedSource = sourceParser.parse();
    std::cout.rdbuf(previousOutput);
    assert(!parsedSource.empty());
    assert(dynamic_cast<ClassDefStmt*>(parsedSource.front().get()) != nullptr);
    std::size_t parsedFunctionCount = 0;
    for (const auto& statement : parsedSource) {
        if (dynamic_cast<FuncDefStmt*>(statement.get())) ++parsedFunctionCount;
    }
    assert(parsedFunctionCount >= 2);

    SalInt castSource(12);
    auto asFloat = castSource.castTo(SalType::FLOAT);
    assert(asFloat->getType() == SalType::FLOAT);
    assert(static_cast<SalFloat*>(asFloat.get())->value == 12.0f);
    SalString numericText("3.25");
    auto parsedFloat = numericText.castTo(SalType::FLOAT);
    assert(static_cast<SalFloat*>(parsedFloat.get())->value == 3.25f);
    bool rejectedMalformedCast = false;
    try {
        SalString malformedText("3.25x");
        malformedText.castTo(SalType::FLOAT);
    } catch (const std::runtime_error&) {
        rejectedMalformedCast = true;
    }
    assert(rejectedMalformedCast);

    std::vector<std::unique_ptr<Stmt>> ast;

    auto dynamicValue = std::make_unique<VarDeclStmt>();
    dynamicValue->type = makeToken(DYN, "dyn", 0, 0);
    dynamicValue->name = makeToken(IDENTIFIER, "value", 0, 4);
    dynamicValue->init = integerLiteral("2");
    ast.push_back(std::move(dynamicValue));

    auto dynamicAssignment = std::make_unique<ExprStmt>();
    auto assignment = std::make_unique<BinOpExpr>();
    auto assignmentTarget = std::make_unique<VarExpr>();
    assignmentTarget->name = makeToken(IDENTIFIER, "value", 1, 0);
    assignment->left = std::move(assignmentTarget);
    assignment->op = makeToken(ASSIGN, "=", 1, 6);
    assignment->right = floatLiteral("2.25");
    dynamicAssignment->expr = std::move(assignment);
    ast.push_back(std::move(dynamicAssignment));

    auto operation = std::make_unique<VarDeclStmt>();
    operation->type = makeToken(TYPE, "float", 2, 0);
    operation->name = makeToken(IDENTIFIER, "sum", 2, 6);
    auto addition = std::make_unique<BinOpExpr>();
    auto dynamicReference = std::make_unique<VarExpr>();
    dynamicReference->name = makeToken(IDENTIFIER, "value", 2, 12);
    addition->left = std::move(dynamicReference);
    addition->op = makeToken(ADD, "+", 2, 18);
    addition->right = floatLiteral("1.5");
    operation->init = std::move(addition);
    ast.push_back(std::move(operation));

    auto classDef = std::make_unique<ClassDefStmt>();
    classDef->name = makeToken(CLASS, "Point", 2, 0);
    ast.push_back(std::move(classDef));
    auto pointA = std::make_unique<VarDeclStmt>();
    pointA->type = makeToken(IDENTIFIER, "Point", 3, 0);
    pointA->name = makeToken(IDENTIFIER, "a", 3, 6);
    ast.push_back(std::move(pointA));
    auto pointB = std::make_unique<VarDeclStmt>();
    pointB->type = makeToken(IDENTIFIER, "Point", 4, 0);
    pointB->name = makeToken(IDENTIFIER, "b", 4, 6);
    ast.push_back(std::move(pointB));
    auto equality = std::make_unique<VarDeclStmt>();
    equality->type = makeToken(TYPE, "bool", 5, 0);
    equality->name = makeToken(IDENTIFIER, "same", 5, 5);
    auto compare = std::make_unique<BinOpExpr>();
    auto leftPoint = std::make_unique<VarExpr>();
    leftPoint->name = makeToken(IDENTIFIER, "a", 5, 12);
    auto rightPoint = std::make_unique<VarExpr>();
    rightPoint->name = makeToken(IDENTIFIER, "b", 5, 17);
    compare->left = std::move(leftPoint);
    compare->op = makeToken(EQUAL, "==", 5, 14);
    compare->right = std::move(rightPoint);
    equality->init = std::move(compare);
    ast.push_back(std::move(equality));

    auto singleReturn = std::make_unique<FuncDefStmt>();
    singleReturn->name = makeToken(IDENTIFIER, "identity", 6, 0);
    singleReturn->params.push_back({makeToken(IDENTIFIER, "number", 6, 15),
                                    makeToken(TYPE, "int", 6, 11), false});
    auto returnNumber = std::make_unique<ReturnStmt>();
    auto numberReference = std::make_unique<VarExpr>();
    numberReference->name = makeToken(IDENTIFIER, "number", 7, 8);
    returnNumber->body = std::move(numberReference);
    singleReturn->body.push_back(std::move(returnNumber));
    auto* singleReturnPtr = singleReturn.get();
    ast.push_back(std::move(singleReturn));

    auto returnedDynamic = std::make_unique<VarDeclStmt>();
    returnedDynamic->type = makeToken(DYN, "dyn", 8, 0);
    returnedDynamic->name = makeToken(IDENTIFIER, "from_function", 8, 4);
    auto identityCall = std::make_unique<CallExpr>();
    identityCall->funcName = makeToken(IDENTIFIER, "identity", 8, 20);
    identityCall->args.push_back(integerLiteral("7"));
    returnedDynamic->init = std::move(identityCall);
    ast.push_back(std::move(returnedDynamic));

    auto multiReturn = std::make_unique<FuncDefStmt>();
    multiReturn->name = makeToken(IDENTIFIER, "choose", 9, 0);
    multiReturn->params.push_back({makeToken(IDENTIFIER, "flag", 9, 12),
                                   makeToken(TYPE, "bool", 9, 8), false});
    auto branch = std::make_unique<IfStmt>();
    auto condition = std::make_unique<VarExpr>();
    condition->name = makeToken(IDENTIFIER, "flag", 9, 7);
    branch->condition = std::move(condition);
    auto intReturn = std::make_unique<ReturnStmt>();
    intReturn->body = integerLiteral("1");
    branch->thenBody.push_back(std::move(intReturn));
    auto floatReturn = std::make_unique<ReturnStmt>();
    floatReturn->body = floatLiteral("2.0");
    branch->elseBody.push_back(std::move(floatReturn));
    multiReturn->body.push_back(std::move(branch));
    ast.push_back(std::move(multiReturn));

    auto chooseCall = std::make_unique<VarDeclStmt>();
    chooseCall->name = makeToken(IDENTIFIER, "choice", 12, 0);
    auto call = std::make_unique<CallExpr>();
    call->funcName = makeToken(IDENTIFIER, "choose", 12, 9);
    auto trueArg = std::make_unique<LiteralExpr>();
    trueArg->value = makeToken(TRUE, "true", 12, 16);
    call->args.push_back(std::move(trueArg));
    chooseCall->init = std::move(call);
    ast.push_back(std::move(chooseCall));

    auto forwardDynamic = std::make_unique<VarDeclStmt>();
    forwardDynamic->type = makeToken(DYN, "dyn", 13, 0);
    forwardDynamic->name = makeToken(IDENTIFIER, "forward_value", 13, 4);
    auto forwardCall = std::make_unique<CallExpr>();
    forwardCall->funcName = makeToken(IDENTIFIER, "later", 13, 20);
    forwardDynamic->init = std::move(forwardCall);
    ast.push_back(std::move(forwardDynamic));
    auto laterFunction = std::make_unique<FuncDefStmt>();
    laterFunction->name = makeToken(IDENTIFIER, "later", 14, 0);
    auto laterReturn = std::make_unique<ReturnStmt>();
    laterReturn->body = integerLiteral("5");
    laterFunction->body.push_back(std::move(laterReturn));
    ast.push_back(std::move(laterFunction));

    TypeChecker checker;
    checker.check(ast);

    assert(singleReturnPtr->returnType.originalTxt == "int");
    auto* rewrittenDyn = dynamic_cast<IfStmt*>(ast[1].get());
    assert(rewrittenDyn != nullptr);
    VarDeclStmt* rewrittenSum = nullptr;
    VarDeclStmt* specializedCompare = nullptr;
    for (const auto& node : ast) {
        auto* variable = dynamic_cast<VarDeclStmt*>(node.get());
        if (!variable) continue;
        if (variable->name.originalTxt == "sum") rewrittenSum = variable;
        if (variable->name.originalTxt == "same") specializedCompare = variable;
    }
    assert(rewrittenSum != nullptr);
    auto* sumDispatch = dynamic_cast<TypeDispatchExpr*>(rewrittenSum->init.get());
    assert(sumDispatch != nullptr && sumDispatch->cases.size() == 2);
    bool foundPromotedIntCase = false;
    for (const auto& item : sumDispatch->cases) {
        bool isIntCase = false;
        for (const auto& guard : item.guards) {
            if (guard.checkedType.originalTxt == "int") isIntCase = true;
        }
        if (!isIntCase) continue;
        auto* caseOperation = dynamic_cast<BinOpExpr*>(item.value.get());
        auto* promotedInt = caseOperation
            ? dynamic_cast<CastExpr*>(caseOperation->left.get())
            : nullptr;
        foundPromotedIntCase = promotedInt && promotedInt->targetType.originalTxt == "float";
    }
    assert(foundPromotedIntCase);
    bool foundDynamicVariantRefresh = false;
    for (const auto& node : ast) {
        auto* branch = dynamic_cast<IfStmt*>(node.get());
        auto* test = branch ? dynamic_cast<TypeIsExpr*>(branch->condition.get()) : nullptr;
        auto* backingName = test ? dynamic_cast<VarExpr*>(test->value.get()) : nullptr;
        if (!backingName || backingName->name.originalTxt != "value$dynamic" ||
            test->checkedType.originalTxt != "int" || branch->thenBody.empty()) continue;
        auto* update = dynamic_cast<ExprStmt*>(branch->thenBody.front().get());
        auto* updateExpr = update ? dynamic_cast<BinOpExpr*>(update->expr.get()) : nullptr;
        auto* updatedVariant = updateExpr ? dynamic_cast<VarExpr*>(updateExpr->left.get()) : nullptr;
        if (updatedVariant && updatedVariant->name.originalTxt == "value$int") {
            foundDynamicVariantRefresh = true;
        }
    }
    assert(foundDynamicVariantRefresh);
    assert(specializedCompare != nullptr);
    assert(dynamic_cast<BinOpExpr*>(specializedCompare->init.get()) != nullptr);
    bool foundIntVariant = false;
    for (const auto& node : ast) {
        auto* function = dynamic_cast<FuncDefStmt*>(node.get());
        if (function && function->name.originalTxt == "choose$int") foundIntVariant = true;
    }
    assert(foundIntVariant);
    bool foundReturnedIntVariant = false;
    for (const auto& node : ast) {
        auto* branch = dynamic_cast<IfStmt*>(node.get());
        if (!branch) continue;
        auto* test = dynamic_cast<TypeIsExpr*>(branch->condition.get());
        if (!test || test->checkedType.originalTxt != "int") continue;
        auto* dynamicName = dynamic_cast<VarExpr*>(test->value.get());
        if (dynamicName && dynamicName->name.originalTxt == "from_function$dynamic") {
            foundReturnedIntVariant = true;
        }
    }
    assert(foundReturnedIntVariant);
    bool foundForwardIntVariant = false;
    for (const auto& node : ast) {
        auto* branch = dynamic_cast<IfStmt*>(node.get());
        if (!branch) continue;
        auto* test = dynamic_cast<TypeIsExpr*>(branch->condition.get());
        if (!test || test->checkedType.originalTxt != "int") continue;
        auto* dynamicName = dynamic_cast<VarExpr*>(test->value.get());
        if (dynamicName && dynamicName->name.originalTxt == "forward_value$dynamic") {
            foundForwardIntVariant = true;
        }
    }
    assert(foundForwardIntVariant);
    VarDeclStmt* choice = nullptr;
    for (const auto& node : ast) {
        auto* variable = dynamic_cast<VarDeclStmt*>(node.get());
        if (variable && variable->name.originalTxt == "choice") choice = variable;
    }
    assert(choice != nullptr && dynamic_cast<TypeDispatchExpr*>(choice->init.get()) != nullptr);

    std::vector<std::unique_ptr<Stmt>> inlineAst;
    auto conditionalFunction = std::make_unique<FuncDefStmt>();
    conditionalFunction->name = makeToken(IDENTIFIER, "select", 20, 0);
    conditionalFunction->params.push_back({makeToken(IDENTIFIER, "condition", 20, 10),
                                           makeToken(TYPE, "bool", 20, 0), false});
    auto returnBranch = std::make_unique<IfStmt>();
    auto branchCondition = std::make_unique<VarExpr>();
    branchCondition->name = makeToken(IDENTIFIER, "condition", 21, 4);
    returnBranch->condition = std::move(branchCondition);
    auto firstReturn = std::make_unique<ReturnStmt>();
    firstReturn->body = integerLiteral("1");
    returnBranch->thenBody.push_back(std::move(firstReturn));
    auto secondReturn = std::make_unique<ReturnStmt>();
    secondReturn->body = floatLiteral("2.5");
    returnBranch->elseBody.push_back(std::move(secondReturn));
    conditionalFunction->body.push_back(std::move(returnBranch));
    inlineAst.push_back(std::move(conditionalFunction));
    auto selected = std::make_unique<VarDeclStmt>();
    selected->name = makeToken(IDENTIFIER, "selected", 23, 0);
    auto selectedCall = std::make_unique<CallExpr>();
    selectedCall->funcName = makeToken(IDENTIFIER, "select", 23, 11);
    auto selectedCondition = std::make_unique<LiteralExpr>();
    selectedCondition->value = makeToken(TRUE, "true", 23, 18);
    selectedCall->args.push_back(std::move(selectedCondition));
    selected->init = std::move(selectedCall);
    inlineAst.push_back(std::move(selected));
    Preprocessor flattener;
    flattener.flattenAST(std::move(inlineAst));
    assert(!hasFunctionOrCall(flattener.processedAST));

    std::vector<std::unique_ptr<Stmt>> dynamicReturnAst;
    auto unionInput = std::make_unique<VarDeclStmt>();
    unionInput->type = makeToken(DYN, "dyn", 30, 0);
    unionInput->name = makeToken(IDENTIFIER, "union_input", 30, 4);
    unionInput->init = integerLiteral("8");
    dynamicReturnAst.push_back(std::move(unionInput));
    auto unionAssignment = std::make_unique<ExprStmt>();
    auto unionAssignExpr = std::make_unique<BinOpExpr>();
    auto unionTarget = std::make_unique<VarExpr>();
    unionTarget->name = makeToken(IDENTIFIER, "union_input", 31, 0);
    unionAssignExpr->left = std::move(unionTarget);
    unionAssignExpr->op = makeToken(ASSIGN, "=", 31, 12);
    unionAssignExpr->right = floatLiteral("8.5");
    unionAssignment->expr = std::move(unionAssignExpr);
    dynamicReturnAst.push_back(std::move(unionAssignment));
    auto dynamicIdentity = std::make_unique<FuncDefStmt>();
    dynamicIdentity->name = makeToken(IDENTIFIER, "dynamic_identity", 32, 0);
    dynamicIdentity->params.push_back({makeToken(IDENTIFIER, "candidate", 32, 20),
                                       makeToken(DYN, "dyn", 32, 16), false});
    auto dynamicReturn = std::make_unique<ReturnStmt>();
    auto candidateReference = std::make_unique<VarExpr>();
    candidateReference->name = makeToken(IDENTIFIER, "candidate", 33, 8);
    dynamicReturn->body = std::move(candidateReference);
    dynamicIdentity->body.push_back(std::move(dynamicReturn));
    dynamicReturnAst.push_back(std::move(dynamicIdentity));
    auto dynamicResult = std::make_unique<VarDeclStmt>();
    dynamicResult->name = makeToken(IDENTIFIER, "dynamic_result", 34, 0);
    auto identityCallExpr = std::make_unique<CallExpr>();
    identityCallExpr->funcName = makeToken(IDENTIFIER, "dynamic_identity", 34, 17);
    auto unionArgument = std::make_unique<VarExpr>();
    unionArgument->name = makeToken(IDENTIFIER, "union_input", 34, 34);
    identityCallExpr->args.push_back(std::move(unionArgument));
    dynamicResult->init = std::move(identityCallExpr);
    dynamicReturnAst.push_back(std::move(dynamicResult));
    Preprocessor dynamicReturnFlattener;
    dynamicReturnFlattener.flattenAST(std::move(dynamicReturnAst));
    assert(!hasFunctionOrCall(dynamicReturnFlattener.processedAST));
    auto* flattenedDynamicResult = dynamic_cast<VarDeclStmt*>(dynamicReturnFlattener.processedAST.back().get());
    assert(flattenedDynamicResult != nullptr);
    auto* dynamicResultDispatch = dynamic_cast<TypeDispatchExpr*>(flattenedDynamicResult->init.get());
    assert(dynamicResultDispatch != nullptr && dynamicResultDispatch->cases.size() == 2);

    std::vector<std::unique_ptr<Stmt>> setupReturnAst;
    auto setupFunction = std::make_unique<FuncDefStmt>();
    setupFunction->name = makeToken(IDENTIFIER, "calculate", 40, 0);
    setupFunction->params.push_back({makeToken(IDENTIFIER, "condition", 40, 15),
                                     makeToken(TYPE, "bool", 40, 11), false});
    setupFunction->params.push_back({makeToken(IDENTIFIER, "seed", 40, 29),
                                     makeToken(TYPE, "int", 40, 25), false});
    auto setupDeclaration = std::make_unique<VarDeclStmt>();
    setupDeclaration->type = makeToken(TYPE, "int", 41, 4);
    setupDeclaration->name = makeToken(IDENTIFIER, "prepared", 41, 8);
    auto setupAddition = std::make_unique<BinOpExpr>();
    auto seedReference = std::make_unique<VarExpr>();
    seedReference->name = makeToken(IDENTIFIER, "seed", 41, 18);
    setupAddition->left = std::move(seedReference);
    setupAddition->op = makeToken(ADD, "+", 41, 23);
    setupAddition->right = integerLiteral("1");
    setupDeclaration->init = std::move(setupAddition);
    setupFunction->body.push_back(std::move(setupDeclaration));
    auto setupBranch = std::make_unique<IfStmt>();
    auto setupCondition = std::make_unique<VarExpr>();
    setupCondition->name = makeToken(IDENTIFIER, "condition", 42, 7);
    setupBranch->condition = std::move(setupCondition);
    auto integerReturn = std::make_unique<ReturnStmt>();
    auto integerResultExpr = std::make_unique<VarExpr>();
    integerResultExpr->name = makeToken(IDENTIFIER, "prepared", 43, 15);
    integerReturn->body = std::move(integerResultExpr);
    setupBranch->thenBody.push_back(std::move(integerReturn));
    auto floatingReturn = std::make_unique<ReturnStmt>();
    auto floatingResultExpr = std::make_unique<BinOpExpr>();
    auto preparedReference = std::make_unique<VarExpr>();
    preparedReference->name = makeToken(IDENTIFIER, "prepared", 45, 15);
    floatingResultExpr->left = std::move(preparedReference);
    floatingResultExpr->op = makeToken(DIVD, "/", 45, 24);
    floatingResultExpr->right = floatLiteral("2.0");
    floatingReturn->body = std::move(floatingResultExpr);
    setupBranch->elseBody.push_back(std::move(floatingReturn));
    setupFunction->body.push_back(std::move(setupBranch));
    setupReturnAst.push_back(std::move(setupFunction));
    auto setupResult = std::make_unique<VarDeclStmt>();
    setupResult->name = makeToken(IDENTIFIER, "calculated", 47, 0);
    auto setupCall = std::make_unique<CallExpr>();
    setupCall->funcName = makeToken(IDENTIFIER, "calculate", 47, 14);
    auto conditionArg = std::make_unique<LiteralExpr>();
    conditionArg->value = makeToken(TRUE, "true", 47, 24);
    setupCall->args.push_back(std::move(conditionArg));
    setupCall->args.push_back(integerLiteral("6"));
    setupResult->init = std::move(setupCall);
    setupReturnAst.push_back(std::move(setupResult));
    Preprocessor setupFlattener;
    setupFlattener.flattenAST(std::move(setupReturnAst));
    assert(!hasFunctionOrCall(setupFlattener.processedAST));
    auto* flattenedSetupResult = dynamic_cast<VarDeclStmt*>(setupFlattener.processedAST.back().get());
    assert(flattenedSetupResult != nullptr);
    auto* setupDispatch = dynamic_cast<TypeDispatchExpr*>(flattenedSetupResult->init.get());
    assert(setupDispatch != nullptr && setupDispatch->cases.size() == 2);
    for (const auto& item : setupDispatch->cases) assert(!item.statements.empty());
}
