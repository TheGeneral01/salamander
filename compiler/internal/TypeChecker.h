#pragma once

#include "../astObjs.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class TypeChecker {
    using TypeSet = std::set<std::string>;
    using TypeEnvironment = std::unordered_map<std::string, TypeSet>;

    std::unordered_set<std::string> classNames;
    std::unordered_map<std::string, TypeSet> dynamicTypes;
    std::unordered_map<std::string, std::vector<std::string>> functionParameters;
    std::unordered_map<std::string, std::vector<TypeSet>> functionParameterTypes;
    std::unordered_map<std::string, std::vector<bool>> functionDynamicParameters;
    std::unordered_map<std::string, TypeSet> functionReturnTypes;

    struct ReturnRoute {
        std::string type;
        std::vector<TypeGuard> guards;
    };
    std::unordered_map<std::string, std::vector<ReturnRoute>> functionRoutes;

    static SALTKN typeToken(const std::string& name) {
        return makeToken(TYPE, name, 0, 0);
    }

    static bool isDynamicType(const SALTKN& token) {
        return token.originalTxt == "dyn" || (!token.originalTxt.empty() && token.type == DYN);
    }

    static std::string literalType(const SALTKN& token) {
        switch (token.type) {
            case INTLIT: return "int";
            case FLOATLIT: return "float";
            case STRLIT:
            case FSTRLIT: return "string";
            case TRUE:
            case FALSE: return "bool";
            default: return {};
        }
    }

    static std::unique_ptr<Expr> cloneExpr(const Expr* expr) {
        if (!expr) return nullptr;
        if (dynamic_cast<const NullExpr*>(expr)) return std::make_unique<NullExpr>();
        if (auto* literal = dynamic_cast<const LiteralExpr*>(expr)) {
            auto copy = std::make_unique<LiteralExpr>();
            copy->value = literal->value;
            return copy;
        }
        if (auto* variable = dynamic_cast<const VarExpr*>(expr)) {
            auto copy = std::make_unique<VarExpr>();
            copy->name = variable->name;
            return copy;
        }
        if (auto* binary = dynamic_cast<const BinOpExpr*>(expr)) {
            auto copy = std::make_unique<BinOpExpr>();
            copy->left = cloneExpr(binary->left.get());
            copy->op = binary->op;
            copy->right = cloneExpr(binary->right.get());
            return copy;
        }
        if (auto* unary = dynamic_cast<const UnOpExpr*>(expr)) {
            auto copy = std::make_unique<UnOpExpr>();
            copy->value = cloneExpr(unary->value.get());
            copy->op = unary->op;
            return copy;
        }
        if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
            auto copy = std::make_unique<CallExpr>();
            copy->funcName = call->funcName;
            for (const auto& arg : call->args) copy->args.push_back(cloneExpr(arg.get()));
            return copy;
        }
        if (auto* list = dynamic_cast<const ListExpr*>(expr)) {
            auto copy = std::make_unique<ListExpr>();
            copy->type = list->type;
            for (const auto& item : list->itms) copy->itms.push_back(cloneExpr(item.get()));
            return copy;
        }
        if (auto* cast = dynamic_cast<const CastExpr*>(expr)) {
            auto copy = std::make_unique<CastExpr>();
            copy->value = cloneExpr(cast->value.get());
            copy->targetType = cast->targetType;
            return copy;
        }
        if (auto* test = dynamic_cast<const TypeIsExpr*>(expr)) {
            auto copy = std::make_unique<TypeIsExpr>();
            copy->value = cloneExpr(test->value.get());
            copy->checkedType = test->checkedType;
            return copy;
        }
        if (auto* dispatch = dynamic_cast<const TypeDispatchExpr*>(expr)) {
            auto copy = std::make_unique<TypeDispatchExpr>();
            for (const auto& item : dispatch->cases) {
                TypeDispatchCase newCase;
                newCase.value = cloneExpr(item.value.get());
                for (const auto& guard : item.guards) {
                    newCase.guards.push_back({cloneExpr(guard.value.get()), guard.checkedType,
                                              guard.isCondition, guard.negate});
                }
                for (const auto& stmt : item.statements) newCase.statements.push_back(cloneStmt(stmt.get()));
                copy->cases.push_back(std::move(newCase));
            }
            return copy;
        }
        throw std::runtime_error("Type checker cannot clone this expression node");
    }

    static TypeSet mergeTypes(const TypeSet& lhs, const TypeSet& rhs) {
        TypeSet result = lhs;
        result.insert(rhs.begin(), rhs.end());
        return result;
    }

    static std::vector<TypeGuard> cloneGuards(const std::vector<TypeGuard>& guards) {
        std::vector<TypeGuard> copies;
        for (const auto& guard : guards) {
            copies.push_back({cloneExpr(guard.value.get()), guard.checkedType,
                              guard.isCondition, guard.negate});
        }
        return copies;
    }

    static std::unique_ptr<Expr> substituteExpr(
        std::unique_ptr<Expr> expr,
        const std::unordered_map<std::string, const Expr*>& bindings) {
        if (!expr) return nullptr;
        if (auto* variable = dynamic_cast<VarExpr*>(expr.get())) {
            auto binding = bindings.find(variable->name.originalTxt);
            return binding == bindings.end() ? std::move(expr) : cloneExpr(binding->second);
        }
        if (auto* binary = dynamic_cast<BinOpExpr*>(expr.get())) {
            binary->left = substituteExpr(std::move(binary->left), bindings);
            binary->right = substituteExpr(std::move(binary->right), bindings);
        } else if (auto* unary = dynamic_cast<UnOpExpr*>(expr.get())) {
            unary->value = substituteExpr(std::move(unary->value), bindings);
        } else if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
            for (auto& argument : call->args) argument = substituteExpr(std::move(argument), bindings);
        } else if (auto* list = dynamic_cast<ListExpr*>(expr.get())) {
            for (auto& item : list->itms) item = substituteExpr(std::move(item), bindings);
        } else if (auto* cast = dynamic_cast<CastExpr*>(expr.get())) {
            cast->value = substituteExpr(std::move(cast->value), bindings);
        } else if (auto* test = dynamic_cast<TypeIsExpr*>(expr.get())) {
            test->value = substituteExpr(std::move(test->value), bindings);
        } else if (auto* dispatch = dynamic_cast<TypeDispatchExpr*>(expr.get())) {
            for (auto& item : dispatch->cases) {
                for (auto& guard : item.guards) guard.value = substituteExpr(std::move(guard.value), bindings);
                item.value = substituteExpr(std::move(item.value), bindings);
            }
        }
        return expr;
    }

    static bool isNumeric(const std::string& type) {
        return type == "int" || type == "float";
    }

    static std::string resultType(const std::string& lhs, SalTknType op,
                                  const std::string& rhs) {
        if (op == ASSIGN || op == ADDASGN || op == SUBASGN ||
            op == MULTASGN || op == DIVASGN) {
            if (lhs == rhs || (isNumeric(lhs) && isNumeric(rhs))) return lhs;
            throw std::runtime_error("Cannot assign " + rhs + " to " + lhs);
        }
        if (op == ADD && lhs == "string" && rhs == "string") return "string";
        if (isNumeric(lhs) && isNumeric(rhs)) {
            if (op == LESS || op == GREATER || op == LESSEQ || op == GREATEREQ ||
                op == EQUAL || op == NEQUAL) return "bool";
            return lhs == "float" || rhs == "float" ? "float" : "int";
        }
        if (lhs == rhs && (op == EQUAL || op == NEQUAL)) return "bool";
        throw std::runtime_error("Operator is not defined for " + lhs + " and " + rhs);
    }

    TypeSet infer(const Expr* expr, const TypeEnvironment& environment) const {
        if (!expr || dynamic_cast<const NullExpr*>(expr)) return {};
        if (auto* literal = dynamic_cast<const LiteralExpr*>(expr)) {
            std::string type = literalType(literal->value);
            return type.empty() ? TypeSet{} : TypeSet{type};
        }
        if (auto* variable = dynamic_cast<const VarExpr*>(expr)) {
            auto found = environment.find(variable->name.originalTxt);
            if (found != environment.end()) {
                if (found->second.size() == 1 && *found->second.begin() == "dyn") {
                    auto dynamic = dynamicTypes.find(variable->name.originalTxt);
                    if (dynamic != dynamicTypes.end()) return dynamic->second;
                    return {};
                }
                return found->second;
            }
            auto dynamic = dynamicTypes.find(variable->name.originalTxt);
            if (dynamic != dynamicTypes.end()) return dynamic->second;
            const std::size_t separator = variable->name.originalTxt.rfind('$');
            if (separator != std::string::npos) {
                const std::string variantType = variable->name.originalTxt.substr(separator + 1);
                if (variantType == "int" || variantType == "float" || variantType == "bool" ||
                    variantType == "string" || classNames.find(variantType) != classNames.end()) return {variantType};
            }
            if (classNames.find(variable->name.originalTxt) != classNames.end()) return {variable->name.originalTxt};
            return {};
        }
        if (auto* cast = dynamic_cast<const CastExpr*>(expr)) return {cast->targetType.originalTxt};
        if (auto* dispatch = dynamic_cast<const TypeDispatchExpr*>(expr)) {
            TypeSet result;
            for (const auto& item : dispatch->cases) result = mergeTypes(result, infer(item.value.get(), environment));
            return result;
        }
        if (dynamic_cast<const TypeIsExpr*>(expr)) return {"bool"};
        if (auto* binary = dynamic_cast<const BinOpExpr*>(expr)) {
            TypeSet results;
            for (const auto& lhs : infer(binary->left.get(), environment)) {
                for (const auto& rhs : infer(binary->right.get(), environment)) {
                    results.insert(resultType(lhs, binary->op.type, rhs));
                }
            }
            return results;
        }
        if (auto* unary = dynamic_cast<const UnOpExpr*>(expr)) {
            TypeSet input = infer(unary->value.get(), environment);
            if (unary->op.type == NOT) return {"bool"};
            for (const auto& type : input) {
                if (!isNumeric(type)) throw std::runtime_error("Unary math operator requires a numeric operand");
            }
            return input;
        }
        if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
            auto knownReturn = functionReturnTypes.find(call->funcName.originalTxt);
            if (knownReturn != functionReturnTypes.end()) return knownReturn->second;
            auto routes = functionRoutes.find(call->funcName.originalTxt);
            if (routes != functionRoutes.end()) {
                TypeSet result;
                for (const auto& route : routes->second) result.insert(route.type);
                return result;
            }
            return {};
        }
        if (auto* list = dynamic_cast<const ListExpr*>(expr)) {
            TypeSet result;
            for (const auto& item : list->itms) result = mergeTypes(result, infer(item.get(), environment));
            return result.empty() ? TypeSet{"list"} : TypeSet{"list"};
        }
        return {};
    }

    void collectClassesAndDyn(const std::vector<std::unique_ptr<Stmt>>& statements,
                              TypeEnvironment& environment, const std::string& scope = "") {
        for (const auto& statement : statements) {
            if (auto* classDef = dynamic_cast<const ClassDefStmt*>(statement.get())) {
                classNames.insert(classDef->name.originalTxt);
                TypeEnvironment classEnvironment = environment;
                for (const auto& member : classDef->body) {
                    if (auto* variable = dynamic_cast<const VarDeclStmt*>(member.get())) {
                        classEnvironment[variable->name.originalTxt] = {variable->type.originalTxt};
                    }
                }
                collectClassesAndDyn(classDef->body, classEnvironment,
                                     functionKey(scope, classDef->name.originalTxt));
            } else if (auto* variable = dynamic_cast<const VarDeclStmt*>(statement.get())) {
                if (isDynamicType(variable->type)) {
                    TypeSet types = infer(variable->init.get(), environment);
                    dynamicTypes[variable->name.originalTxt] = types;
                } else if (!variable->type.originalTxt.empty()) {
                    environment[variable->name.originalTxt] = {variable->type.originalTxt};
                } else {
                    environment[variable->name.originalTxt] = infer(variable->init.get(), environment);
                }
            } else if (auto* function = dynamic_cast<const FuncDefStmt*>(statement.get())) {
                TypeEnvironment functionEnvironment = environment;
                const std::string key = functionKey(scope, function->name.originalTxt);
                auto parameterTypes = functionParameterTypes.find(key);
                for (std::size_t index = 0; index < function->params.size(); ++index) {
                    TypeSet types = isDynamicType(function->params[index].type) &&
                                    parameterTypes != functionParameterTypes.end()
                        ? parameterTypes->second[index]
                        : TypeSet{function->params[index].type.originalTxt};
                    functionEnvironment[function->params[index].name.originalTxt] = types.empty()
                        ? TypeSet{"dyn"} : types;
                }
                collectClassesAndDyn(function->body, functionEnvironment, key);
            } else if (auto* branch = dynamic_cast<const IfStmt*>(statement.get())) {
                TypeEnvironment thenEnvironment = environment;
                TypeEnvironment elseEnvironment = environment;
                collectClassesAndDyn(branch->thenBody, thenEnvironment, scope);
                collectClassesAndDyn(branch->elseBody, elseEnvironment, scope);
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(statement.get())) {
                TypeEnvironment loopEnvironment = environment;
                collectClassesAndDyn(loop->body, loopEnvironment, scope);
            }
        }
    }

    void collectClassNames(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (const auto& statement : statements) {
            if (auto* classDef = dynamic_cast<const ClassDefStmt*>(statement.get())) {
                classNames.insert(classDef->name.originalTxt);
                collectClassNames(classDef->body);
            } else if (auto* function = dynamic_cast<const FuncDefStmt*>(statement.get())) {
                collectClassNames(function->body);
            } else if (auto* branch = dynamic_cast<const IfStmt*>(statement.get())) {
                collectClassNames(branch->thenBody);
                collectClassNames(branch->elseBody);
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(statement.get())) {
                collectClassNames(loop->body);
            }
        }
    }

    void collectFunctionSignatures(const std::vector<std::unique_ptr<Stmt>>& statements,
                                   const std::string& scope = "") {
        for (const auto& statement : statements) {
            if (auto* classDef = dynamic_cast<const ClassDefStmt*>(statement.get())) {
                collectFunctionSignatures(classDef->body,
                    functionKey(scope, classDef->name.originalTxt));
            } else if (auto* function = dynamic_cast<const FuncDefStmt*>(statement.get())) {
                const std::string key = functionKey(scope, function->name.originalTxt);
                auto& names = functionParameters[key];
                auto& types = functionParameterTypes[key];
                auto& dynamic = functionDynamicParameters[key];
                names.clear();
                types.clear();
                dynamic.clear();
                for (const auto& parameter : function->params) {
                    names.push_back(parameter.name.originalTxt);
                    dynamic.push_back(isDynamicType(parameter.type));
                    types.push_back(dynamic.back() ? TypeSet{} : TypeSet{parameter.type.originalTxt});
                }
                collectFunctionSignatures(function->body, key);
            } else if (auto* branch = dynamic_cast<const IfStmt*>(statement.get())) {
                collectFunctionSignatures(branch->thenBody, scope);
                collectFunctionSignatures(branch->elseBody, scope);
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(statement.get())) {
                collectFunctionSignatures(loop->body, scope);
            }
        }
    }

    void collectCallArgumentTypes(const Expr* expr, const TypeEnvironment& environment) {
        if (!expr) return;
        if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
            auto dynamicParameters = functionDynamicParameters.find(call->funcName.originalTxt);
            auto parameterTypes = functionParameterTypes.find(call->funcName.originalTxt);
            if (dynamicParameters != functionDynamicParameters.end() &&
                parameterTypes != functionParameterTypes.end()) {
                const std::size_t count = std::min(call->args.size(), dynamicParameters->second.size());
                for (std::size_t index = 0; index < count; ++index) {
                    if (dynamicParameters->second[index]) {
                        parameterTypes->second[index] = mergeTypes(
                            parameterTypes->second[index], infer(call->args[index].get(), environment));
                    }
                }
            }
            for (const auto& argument : call->args) collectCallArgumentTypes(argument.get(), environment);
        } else if (auto* binary = dynamic_cast<const BinOpExpr*>(expr)) {
            collectCallArgumentTypes(binary->left.get(), environment);
            collectCallArgumentTypes(binary->right.get(), environment);
        } else if (auto* unary = dynamic_cast<const UnOpExpr*>(expr)) {
            collectCallArgumentTypes(unary->value.get(), environment);
        } else if (auto* list = dynamic_cast<const ListExpr*>(expr)) {
            for (const auto& item : list->itms) collectCallArgumentTypes(item.get(), environment);
        } else if (auto* cast = dynamic_cast<const CastExpr*>(expr)) {
            collectCallArgumentTypes(cast->value.get(), environment);
        } else if (auto* test = dynamic_cast<const TypeIsExpr*>(expr)) {
            collectCallArgumentTypes(test->value.get(), environment);
        } else if (auto* dispatch = dynamic_cast<const TypeDispatchExpr*>(expr)) {
            for (const auto& item : dispatch->cases) {
                for (const auto& guard : item.guards) collectCallArgumentTypes(guard.value.get(), environment);
                for (const auto& stmt : item.statements) collectCallArgumentTypes(stmt.get(), environment);
                collectCallArgumentTypes(item.value.get(), environment);
            }
        }
    }

    void collectCallArgumentTypes(const Stmt* stmt, const TypeEnvironment& environment) {
        if (auto* variable = dynamic_cast<const VarDeclStmt*>(stmt)) {
            collectCallArgumentTypes(variable->init.get(), environment);
        } else if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt)) {
            collectCallArgumentTypes(ret->body.get(), environment);
        } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt)) {
            collectCallArgumentTypes(exprStmt->expr.get(), environment);
        } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt)) {
            collectCallArgumentTypes(branch->condition.get(), environment);
            for (const auto& child : branch->thenBody) collectCallArgumentTypes(child.get(), environment);
            for (const auto& child : branch->elseBody) collectCallArgumentTypes(child.get(), environment);
        } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt)) {
            collectCallArgumentTypes(loop->condition.get(), environment);
            for (const auto& child : loop->body) collectCallArgumentTypes(child.get(), environment);
        } else if (auto* function = dynamic_cast<const FuncDefStmt*>(stmt)) {
            for (const auto& child : function->body) collectCallArgumentTypes(child.get(), environment);
        } else if (auto* classDef = dynamic_cast<const ClassDefStmt*>(stmt)) {
            for (const auto& child : classDef->body) collectCallArgumentTypes(child.get(), environment);
        }
    }

    void collectCallArgumentTypes(const std::vector<std::unique_ptr<Stmt>>& statements,
                                  const TypeEnvironment& environment) {
        for (const auto& statement : statements) collectCallArgumentTypes(statement.get(), environment);
    }

    void collectDynamicAssignments(const std::vector<std::unique_ptr<Stmt>>& statements,
                                   const TypeEnvironment& environment) {
        for (const auto& statement : statements) {
            if (auto* assignment = dynamic_cast<const ExprStmt*>(statement.get())) {
                auto* binary = dynamic_cast<const BinOpExpr*>(assignment->expr.get());
                auto* target = binary ? dynamic_cast<const VarExpr*>(binary->left.get()) : nullptr;
                if (binary && binary->op.type == ASSIGN && target) {
                    auto found = dynamicTypes.find(target->name.originalTxt);
                    if (found != dynamicTypes.end()) {
                        found->second = mergeTypes(found->second, infer(binary->right.get(), environment));
                    }
                }
            } else if (auto* branch = dynamic_cast<const IfStmt*>(statement.get())) {
                collectDynamicAssignments(branch->thenBody, environment);
                collectDynamicAssignments(branch->elseBody, environment);
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(statement.get())) {
                collectDynamicAssignments(loop->body, environment);
            } else if (auto* function = dynamic_cast<const FuncDefStmt*>(statement.get())) {
                collectDynamicAssignments(function->body, environment);
            } else if (auto* classDef = dynamic_cast<const ClassDefStmt*>(statement.get())) {
                collectDynamicAssignments(classDef->body, environment);
            }
        }
    }

    static std::unique_ptr<Expr> makeCast(std::unique_ptr<Expr> value, const std::string& type) {
        auto cast = std::make_unique<CastExpr>();
        cast->value = std::move(value);
        cast->targetType = typeToken(type);
        return cast;
    }

    static std::unique_ptr<Expr> makeTypeTest(const std::string& name, const std::string& type) {
        auto test = std::make_unique<TypeIsExpr>();
        auto variable = std::make_unique<VarExpr>();
        variable->name = makeToken(IDENTIFIER, name, 0, 0);
        test->value = std::move(variable);
        test->checkedType = typeToken(type);
        return test;
    }

    static std::string functionKey(const std::string& scope, const std::string& name) {
        return scope.empty() ? name : scope + ":" + name;
    }

    static std::unique_ptr<Stmt> cloneStmt(const Stmt* stmt) {
        if (dynamic_cast<const NullStmt*>(stmt)) return std::make_unique<NullStmt>();
        if (dynamic_cast<const BreakStmt*>(stmt)) return std::make_unique<BreakStmt>();
        if (dynamic_cast<const ContinueStmt*>(stmt)) return std::make_unique<ContinueStmt>();
        if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt)) {
            auto copy = std::make_unique<ReturnStmt>();
            copy->body = cloneExpr(ret->body.get());
            return copy;
        }
        if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt)) {
            auto copy = std::make_unique<ExprStmt>();
            copy->expr = cloneExpr(exprStmt->expr.get());
            return copy;
        }
        if (auto* variable = dynamic_cast<const VarDeclStmt*>(stmt)) {
            auto copy = std::make_unique<VarDeclStmt>();
            copy->attributes = variable->attributes;
            copy->type = variable->type;
            copy->name = variable->name;
            copy->init = cloneExpr(variable->init.get());
            return copy;
        }
        if (auto* branch = dynamic_cast<const IfStmt*>(stmt)) {
            auto copy = std::make_unique<IfStmt>();
            copy->condition = cloneExpr(branch->condition.get());
            for (const auto& child : branch->thenBody) copy->thenBody.push_back(cloneStmt(child.get()));
            for (const auto& child : branch->elseBody) copy->elseBody.push_back(cloneStmt(child.get()));
            return copy;
        }
        if (auto* loop = dynamic_cast<const WhileStmt*>(stmt)) {
            auto copy = std::make_unique<WhileStmt>();
            copy->condition = cloneExpr(loop->condition.get());
            copy->shader = loop->shader;
            for (const auto& child : loop->body) copy->body.push_back(cloneStmt(child.get()));
            return copy;
        }
        if (auto* nestedClass = dynamic_cast<const ClassDefStmt*>(stmt)) {
            auto copy = std::make_unique<ClassDefStmt>();
            copy->name = nestedClass->name;
            copy->inheritance = nestedClass->inheritance;
            copy->attributes = nestedClass->attributes;
            for (const auto& child : nestedClass->body) copy->body.push_back(cloneStmt(child.get()));
            return copy;
        }
        if (auto* function = dynamic_cast<const FuncDefStmt*>(stmt)) {
            auto copy = std::make_unique<FuncDefStmt>();
            copy->name = function->name;
            copy->params = function->params;
            copy->returnType = function->returnType;
            copy->attributes = function->attributes;
            for (const auto& child : function->body) copy->body.push_back(cloneStmt(child.get()));
            return copy;
        }
        throw std::runtime_error("Type checker cannot clone this statement node");
    }

    void collectReturns(const std::vector<std::unique_ptr<Stmt>>& statements,
                        TypeEnvironment environment, TypeSet& types,
                        std::vector<ReturnRoute>& routes,
                        const std::vector<TypeGuard>& guards = {}) const {
        for (const auto& stmt : statements) {
            if (auto* variable = dynamic_cast<const VarDeclStmt*>(stmt.get())) {
                TypeSet declaredTypes = isDynamicType(variable->type)
                    ? infer(variable->init.get(), environment)
                    : (variable->type.originalTxt.empty()
                        ? infer(variable->init.get(), environment)
                        : TypeSet{variable->type.originalTxt});
                environment[variable->name.originalTxt] = std::move(declaredTypes);
            } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt.get())) {
                auto* assignment = dynamic_cast<const BinOpExpr*>(exprStmt->expr.get());
                auto* target = assignment ? dynamic_cast<const VarExpr*>(assignment->left.get()) : nullptr;
                if (assignment && assignment->op.type == ASSIGN && target) {
                    TypeSet assignedTypes = infer(assignment->right.get(), environment);
                    auto existing = environment.find(target->name.originalTxt);
                    if (existing != environment.end() && existing->second.size() == 1 &&
                        *existing->second.begin() == "dyn") {
                        existing->second = mergeTypes(existing->second, assignedTypes);
                    } else {
                        environment[target->name.originalTxt] = std::move(assignedTypes);
                    }
                }
            } else if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt.get())) {
                TypeSet found = infer(ret->body.get(), environment);
                for (const auto& type : found) {
                    types.insert(type);
                    auto routeGuards = cloneGuards(guards);
                    if (found.size() > 1) {
                        auto typeTest = std::make_unique<TypeIsExpr>();
                        typeTest->value = cloneExpr(ret->body.get());
                        typeTest->checkedType = typeToken(type);
                        routeGuards.push_back({std::move(typeTest), typeToken(type), true, false});
                    }
                    routes.push_back({type, std::move(routeGuards)});
                }
            } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                TypeEnvironment thenEnvironment = environment;
                TypeEnvironment elseEnvironment = environment;
                auto thenGuards = cloneGuards(guards);
                thenGuards.push_back({cloneExpr(branch->condition.get()), typeToken(""), true, false});
                collectReturns(branch->thenBody, thenEnvironment, types, routes, thenGuards);
                auto elseGuards = cloneGuards(guards);
                elseGuards.push_back({cloneExpr(branch->condition.get()), typeToken(""), true, true});
                collectReturns(branch->elseBody, elseEnvironment, types, routes, elseGuards);
                for (const auto& [name, thenTypes] : thenEnvironment) {
                    auto found = elseEnvironment.find(name);
                    if (found != elseEnvironment.end()) {
                        environment[name] = mergeTypes(thenTypes, found->second);
                    } else {
                        environment[name] = thenTypes;
                    }
                }
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
                collectReturns(loop->body, environment, types, routes, guards);
            }
        }
    }

    void analyzeFunctions(std::vector<std::unique_ptr<Stmt>>& statements,
                          const std::string& scope, TypeEnvironment environment) {
        for (const auto& stmt : statements) {
            if (auto* classDef = dynamic_cast<ClassDefStmt*>(stmt.get())) {
                TypeEnvironment classEnvironment = environment;
                for (const auto& member : classDef->body) {
                    if (auto* field = dynamic_cast<const VarDeclStmt*>(member.get())) {
                        classEnvironment[field->name.originalTxt] = {field->type.originalTxt};
                    }
                }
                analyzeFunctions(classDef->body, functionKey(scope, classDef->name.originalTxt), classEnvironment);
            } else if (auto* function = dynamic_cast<FuncDefStmt*>(stmt.get())) {
                TypeEnvironment functionEnvironment = environment;
                const std::string key = functionKey(scope, function->name.originalTxt);
                auto& parameterTypes = functionParameterTypes[key];
                auto& dynamicParameters = functionDynamicParameters[key];
                parameterTypes.resize(function->params.size());
                dynamicParameters.resize(function->params.size());
                for (std::size_t index = 0; index < function->params.size(); ++index) {
                    const auto& parameter = function->params[index];
                    dynamicParameters[index] = isDynamicType(parameter.type);
                    if (!dynamicParameters[index]) parameterTypes[index] = {parameter.type.originalTxt};
                    functionEnvironment[parameter.name.originalTxt] = parameterTypes[index].empty()
                        ? TypeSet{"dyn"} : parameterTypes[index];
                }
                TypeSet returnTypes;
                std::vector<ReturnRoute> routes;
                collectReturns(function->body, functionEnvironment, returnTypes, routes);
                auto& parameterNames = functionParameters[key];
                parameterNames.clear();
                for (const auto& parameter : function->params) parameterNames.push_back(parameter.name.originalTxt);
                if (returnTypes.size() == 1) {
                    function->returnType = typeToken(*returnTypes.begin());
                    functionReturnTypes[key] = returnTypes;
                    functionRoutes.erase(key);
                } else if (returnTypes.size() > 1) {
                    functionRoutes[key] = std::move(routes);
                    functionReturnTypes[key] = returnTypes;
                }
                analyzeFunctions(function->body, key, functionEnvironment);
            } else if (auto* branch = dynamic_cast<IfStmt*>(stmt.get())) {
                analyzeFunctions(branch->thenBody, scope, environment);
                analyzeFunctions(branch->elseBody, scope, environment);
            } else if (auto* loop = dynamic_cast<WhileStmt*>(stmt.get())) {
                analyzeFunctions(loop->body, scope, environment);
            }
        }
    }

    static std::size_t countFunctions(const std::vector<std::unique_ptr<Stmt>>& statements) {
        std::size_t count = 0;
        for (const auto& stmt : statements) {
            if (auto* function = dynamic_cast<const FuncDefStmt*>(stmt.get())) {
                ++count;
                count += countFunctions(function->body);
            } else if (auto* classDef = dynamic_cast<const ClassDefStmt*>(stmt.get())) {
                count += countFunctions(classDef->body);
            } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                count += countFunctions(branch->thenBody) + countFunctions(branch->elseBody);
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
                count += countFunctions(loop->body);
            }
        }
        return count;
    }

    static bool containsReturnOfType(const std::vector<std::unique_ptr<Stmt>>& statements,
                                     const std::string& type,
                                     const TypeEnvironment& environment,
                                     const TypeChecker& checker) {
        for (const auto& stmt : statements) {
            if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt.get())) {
                TypeSet returnTypes = checker.infer(ret->body.get(), environment);
                if (returnTypes.find(type) != returnTypes.end()) return true;
            } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                if (containsReturnOfType(branch->thenBody, type, environment, checker) ||
                    containsReturnOfType(branch->elseBody, type, environment, checker)) return true;
            }
        }
        return false;
    }

    void collectLocalTypes(const std::vector<std::unique_ptr<Stmt>>& statements,
                           TypeEnvironment& environment) const {
        for (const auto& stmt : statements) {
            if (auto* variable = dynamic_cast<const VarDeclStmt*>(stmt.get())) {
                TypeSet types = isDynamicType(variable->type)
                    ? infer(variable->init.get(), environment)
                    : (variable->type.originalTxt.empty()
                        ? infer(variable->init.get(), environment)
                        : TypeSet{variable->type.originalTxt});
                environment[variable->name.originalTxt] = std::move(types);
            } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(stmt.get())) {
                auto* assignment = dynamic_cast<const BinOpExpr*>(exprStmt->expr.get());
                auto* target = assignment ? dynamic_cast<const VarExpr*>(assignment->left.get()) : nullptr;
                if (assignment && assignment->op.type == ASSIGN && target) {
                    environment[target->name.originalTxt] = infer(assignment->right.get(), environment);
                }
            } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                TypeEnvironment thenEnvironment = environment;
                TypeEnvironment elseEnvironment = environment;
                collectLocalTypes(branch->thenBody, thenEnvironment);
                collectLocalTypes(branch->elseBody, elseEnvironment);
                for (const auto& [name, thenTypes] : thenEnvironment) {
                    auto found = elseEnvironment.find(name);
                    environment[name] = found == elseEnvironment.end()
                        ? thenTypes : mergeTypes(thenTypes, found->second);
                }
            } else if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
                TypeEnvironment loopEnvironment = environment;
                collectLocalTypes(loop->body, loopEnvironment);
                for (const auto& [name, types] : loopEnvironment) {
                    environment[name] = mergeTypes(environment[name], types);
                }
            }
        }
    }

    void specializeReturns(std::vector<std::unique_ptr<Stmt>>& statements,
                           const std::string& type, const TypeEnvironment& environment) const {
        for (auto it = statements.begin(); it != statements.end();) {
            if (auto* ret = dynamic_cast<ReturnStmt*>(it->get())) {
                TypeSet returnTypes = infer(ret->body.get(), environment);
                if (returnTypes.find(type) == returnTypes.end()) {
                    it = statements.erase(it);
                    continue;
                }
                if (returnTypes.size() > 1) ret->body = makeCast(std::move(ret->body), type);
            } else if (auto* branch = dynamic_cast<IfStmt*>(it->get())) {
                if (containsReturnOfType(branch->thenBody, type, environment, *this)) {
                    specializeReturns(branch->thenBody, type, environment);
                } else {
                    branch->thenBody.clear();
                }
                if (containsReturnOfType(branch->elseBody, type, environment, *this)) {
                    specializeReturns(branch->elseBody, type, environment);
                } else {
                    branch->elseBody.clear();
                }
                if (branch->thenBody.empty() && branch->elseBody.empty()) {
                    it = statements.erase(it);
                    continue;
                }
                if (branch->elseBody.empty()) {
                    auto replacement = std::move(branch->thenBody);
                    it = statements.erase(it);
                    it = statements.insert(it, std::make_move_iterator(replacement.begin()),
                                           std::make_move_iterator(replacement.end()));
                    continue;
                }
                if (branch->thenBody.empty()) {
                    auto replacement = std::move(branch->elseBody);
                    it = statements.erase(it);
                    it = statements.insert(it, std::make_move_iterator(replacement.begin()),
                                           std::make_move_iterator(replacement.end()));
                    continue;
                }
            }
            ++it;
        }
    }

    void splitFunctions(std::vector<std::unique_ptr<Stmt>>& statements,
                        const std::string& scope, const TypeEnvironment& environment) {
        for (auto it = statements.begin(); it != statements.end();) {
            if (auto* function = dynamic_cast<FuncDefStmt*>(it->get())) {
                const std::string key = functionKey(scope, function->name.originalTxt);
                auto routes = functionRoutes.find(key);
                if (routes != functionRoutes.end()) {
                    TypeEnvironment functionEnvironment = environment;
                    auto parameterTypes = functionParameterTypes.find(key);
                    for (std::size_t index = 0; index < function->params.size(); ++index) {
                        TypeSet types = isDynamicType(function->params[index].type) &&
                                        parameterTypes != functionParameterTypes.end()
                            ? parameterTypes->second[index]
                            : TypeSet{function->params[index].type.originalTxt};
                        functionEnvironment[function->params[index].name.originalTxt] = types.empty()
                            ? TypeSet{"dyn"} : types;
                    }
                    collectLocalTypes(function->body, functionEnvironment);
                    std::vector<std::unique_ptr<Stmt>> variants;
                    TypeSet types;
                    for (const auto& route : routes->second) types.insert(route.type);
                    for (const auto& type : types) {
                        auto copy = std::unique_ptr<FuncDefStmt>(
                            static_cast<FuncDefStmt*>(cloneStmt(function).release()));
                        copy->name.originalTxt += "$" + type;
                        copy->returnType = typeToken(type);
                        specializeReturns(copy->body, type, functionEnvironment);
                        variants.push_back(std::move(copy));
                    }
                    it = statements.erase(it);
                    it = statements.insert(it, std::make_move_iterator(variants.begin()),
                                           std::make_move_iterator(variants.end()));
                    std::advance(it, static_cast<std::ptrdiff_t>(variants.size()));
                    continue;
                }
                TypeEnvironment functionEnvironment = environment;
                auto parameterTypes = functionParameterTypes.find(key);
                for (std::size_t index = 0; index < function->params.size(); ++index) {
                    TypeSet types = isDynamicType(function->params[index].type) &&
                                    parameterTypes != functionParameterTypes.end()
                        ? parameterTypes->second[index]
                        : TypeSet{function->params[index].type.originalTxt};
                    functionEnvironment[function->params[index].name.originalTxt] = types.empty()
                        ? TypeSet{"dyn"} : types;
                }
                splitFunctions(function->body, key, functionEnvironment);
            } else if (auto* classDef = dynamic_cast<ClassDefStmt*>(it->get())) {
                splitFunctions(classDef->body, functionKey(scope, classDef->name.originalTxt), environment);
            }
            ++it;
        }
    }

    TypeSet checkExpr(std::unique_ptr<Expr>& expr, TypeEnvironment& environment) {
        if (!expr) return {};
        if (auto* variable = dynamic_cast<VarExpr*>(expr.get())) {
            const std::string name = variable->name.originalTxt;
            auto dynamic = dynamicTypes.find(name);
            if (dynamic != dynamicTypes.end()) {
                auto dispatch = std::make_unique<TypeDispatchExpr>();
                for (const auto& type : dynamic->second) {
                    TypeDispatchCase item;
                    item.guards.push_back({makeTypeTest(name + "$dynamic", type), typeToken(type), false, false});
                    auto variant = std::make_unique<VarExpr>();
                    variant->name = makeToken(IDENTIFIER, name + "$" + type, variable->name.line, variable->name.col);
                    item.value = std::move(variant);
                    dispatch->cases.push_back(std::move(item));
                }
                expr = std::move(dispatch);
                return dynamic->second;
            }
            return infer(expr.get(), environment);
        }
        if (auto* binary = dynamic_cast<BinOpExpr*>(expr.get())) {
            auto* lhsVariable = dynamic_cast<VarExpr*>(binary->left.get());
            if (binary->op.type == ASSIGN && lhsVariable && dynamicTypes.find(lhsVariable->name.originalTxt) != dynamicTypes.end()) {
                lhsVariable->name.originalTxt += "$dynamic";
                auto rhsTypes = checkExpr(binary->right, environment);
                return rhsTypes;
            }
            TypeSet lhs = checkExpr(binary->left, environment);
            TypeSet rhs = checkExpr(binary->right, environment);
            if (lhs.empty() || rhs.empty()) return {};
            auto* lhsDispatch = dynamic_cast<TypeDispatchExpr*>(binary->left.get());
            auto* rhsDispatch = dynamic_cast<TypeDispatchExpr*>(binary->right.get());
            if (lhsDispatch || rhsDispatch) {
                std::vector<TypeDispatchCase> lhsCases;
                std::vector<TypeDispatchCase> rhsCases;
                if (lhsDispatch) lhsCases = std::move(lhsDispatch->cases);
                else lhsCases.push_back({{}, {}, cloneExpr(binary->left.get())});
                if (rhsDispatch) rhsCases = std::move(rhsDispatch->cases);
                else rhsCases.push_back({{}, {}, cloneExpr(binary->right.get())});
                auto dispatch = std::make_unique<TypeDispatchExpr>();
                for (const auto& leftCase : lhsCases) {
                    for (const auto& rightCase : rhsCases) {
                        TypeSet leftTypes = infer(leftCase.value.get(), environment);
                        TypeSet rightTypes = infer(rightCase.value.get(), environment);
                        if (leftTypes.empty() || rightTypes.empty()) continue;
                        const std::string leftType = *leftTypes.begin();
                        const std::string rightType = *rightTypes.begin();
                        const std::string result = resultType(leftType, binary->op.type, rightType);
                        TypeDispatchCase item;
                        for (const auto& guard : leftCase.guards) item.guards.push_back({cloneExpr(guard.value.get()), guard.checkedType});
                        for (const auto& guard : rightCase.guards) item.guards.push_back({cloneExpr(guard.value.get()), guard.checkedType});
                        auto operation = std::make_unique<BinOpExpr>();
                        operation->op = binary->op;
                        operation->left = leftType != rightType && isNumeric(leftType) && isNumeric(rightType)
                            ? makeCast(cloneExpr(leftCase.value.get()), result == "float" ? "float" : leftType)
                            : cloneExpr(leftCase.value.get());
                        operation->right = leftType != rightType && isNumeric(leftType) && isNumeric(rightType)
                            ? makeCast(cloneExpr(rightCase.value.get()), result == "float" ? "float" : rightType)
                            : cloneExpr(rightCase.value.get());
                        item.value = std::move(operation);
                        dispatch->cases.push_back(std::move(item));
                    }
                }
                expr = std::move(dispatch);
                return infer(expr.get(), environment);
            }
            const std::string leftType = *lhs.begin();
            const std::string rightType = *rhs.begin();
            const std::string result = resultType(leftType, binary->op.type, rightType);
            if (isNumeric(leftType) && isNumeric(rightType) && leftType != rightType) {
                if (result == "float") {
                    if (leftType == "int") binary->left = makeCast(std::move(binary->left), "float");
                    if (rightType == "int") binary->right = makeCast(std::move(binary->right), "float");
                }
            }
            return {result};
        }
        if (auto* unary = dynamic_cast<UnOpExpr*>(expr.get())) {
            TypeSet types = checkExpr(unary->value, environment);
            for (const auto& type : types) {
                if (!isNumeric(type) && unary->op.type != NOT) {
                    throw std::runtime_error("Unary math operator requires a numeric operand");
                }
            }
            return unary->op.type == NOT ? TypeSet{"bool"} : types;
        }
        if (auto* cast = dynamic_cast<CastExpr*>(expr.get())) {
            checkExpr(cast->value, environment);
            return {cast->targetType.originalTxt};
        }
        if (auto* test = dynamic_cast<TypeIsExpr*>(expr.get())) {
            checkExpr(test->value, environment);
            return {"bool"};
        }
        if (auto* dispatch = dynamic_cast<TypeDispatchExpr*>(expr.get())) {
            TypeSet result;
            for (auto& item : dispatch->cases) result = mergeTypes(result, checkExpr(item.value, environment));
            return result;
        }
        if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
            for (auto& argument : call->args) checkExpr(argument, environment);
            auto routes = functionRoutes.find(call->funcName.originalTxt);
            if (routes != functionRoutes.end()) {
                auto dispatch = std::make_unique<TypeDispatchExpr>();
                std::unordered_map<std::string, const Expr*> bindings;
                auto parameterNames = functionParameters.find(call->funcName.originalTxt);
                if (parameterNames == functionParameters.end() || parameterNames->second.size() != call->args.size()) {
                    throw std::runtime_error("Cannot specialize call arguments for " + call->funcName.originalTxt);
                }
                for (std::size_t index = 0; index < call->args.size(); ++index) {
                    bindings.emplace(parameterNames->second[index], call->args[index].get());
                }
                for (const auto& route : routes->second) {
                    TypeDispatchCase item;
                    item.guards = cloneGuards(route.guards);
                    for (auto& guard : item.guards) {
                        if (guard.isCondition) guard.value = substituteExpr(std::move(guard.value), bindings);
                    }
                    auto variantCall = std::make_unique<CallExpr>();
                    variantCall->funcName = call->funcName;
                    variantCall->funcName.originalTxt += "$" + route.type;
                    for (const auto& argument : call->args) {
                        variantCall->args.push_back(cloneExpr(argument.get()));
                    }
                    item.value = std::move(variantCall);
                    dispatch->cases.push_back(std::move(item));
                }
                expr = std::move(dispatch);
                return infer(expr.get(), environment);
            }
            return infer(call, environment);
        }
        if (auto* list = dynamic_cast<ListExpr*>(expr.get())) {
            for (auto& item : list->itms) checkExpr(item, environment);
            return {"list"};
        }
        return infer(expr.get(), environment);
    }

    void rewriteStatements(std::vector<std::unique_ptr<Stmt>>& statements, TypeEnvironment environment) {
        for (auto it = statements.begin(); it != statements.end();) {
            if (auto* variable = dynamic_cast<VarDeclStmt*>(it->get())) {
                TypeSet valueTypes = checkExpr(variable->init, environment);
                if (isDynamicType(variable->type)) {
                    const std::string name = variable->name.originalTxt;
                    auto foundTypes = dynamicTypes.find(name);
                    if (foundTypes == dynamicTypes.end() || foundTypes->second.empty()) {
                        throw std::runtime_error("Cannot determine possible types for dyn variable " + name);
                    }
                    auto backing = std::make_unique<VarDeclStmt>();
                    backing->attributes = variable->attributes;
                    backing->type = variable->type;
                    backing->type.originalTxt = "dyn";
                    backing->name = makeToken(IDENTIFIER, name + "$dynamic", variable->name.line, variable->name.col);
                    backing->init = std::move(variable->init);
                    std::vector<std::unique_ptr<Stmt>> replacements;
                    replacements.push_back(std::move(backing));
                    for (const auto& type : foundTypes->second) {
                        auto branch = std::make_unique<IfStmt>();
                        branch->condition = makeTypeTest(name + "$dynamic", type);
                        auto specialized = std::make_unique<VarDeclStmt>();
                        specialized->type = typeToken(type);
                        specialized->name = makeToken(IDENTIFIER, name + "$" + type, variable->name.line, variable->name.col);
                        auto backingValue = std::make_unique<VarExpr>();
                        backingValue->name = makeToken(IDENTIFIER, name + "$dynamic", variable->name.line, variable->name.col);
                        specialized->init = makeCast(std::move(backingValue), type);
                        branch->thenBody.push_back(std::move(specialized));
                        replacements.push_back(std::move(branch));
                    }
                    it = statements.erase(it);
                    it = statements.insert(it, std::make_move_iterator(replacements.begin()),
                                           std::make_move_iterator(replacements.end()));
                    std::advance(it, static_cast<std::ptrdiff_t>(replacements.size()));
                    continue;
                }
                if (variable->type.originalTxt.empty()) environment[variable->name.originalTxt] = valueTypes;
                else environment[variable->name.originalTxt] = {variable->type.originalTxt};
            } else if (auto* function = dynamic_cast<FuncDefStmt*>(it->get())) {
                TypeEnvironment functionEnvironment = environment;
                for (const auto& parameter : function->params) functionEnvironment[parameter.name.originalTxt] = {parameter.type.originalTxt};
                rewriteStatements(function->body, std::move(functionEnvironment));
            } else if (auto* classDef = dynamic_cast<ClassDefStmt*>(it->get())) {
                TypeEnvironment classEnvironment = environment;
                for (const auto& member : classDef->body) {
                    if (auto* field = dynamic_cast<VarDeclStmt*>(member.get())) classEnvironment[field->name.originalTxt] = {field->type.originalTxt};
                }
                rewriteStatements(classDef->body, std::move(classEnvironment));
            } else if (auto* branch = dynamic_cast<IfStmt*>(it->get())) {
                checkExpr(branch->condition, environment);
                rewriteStatements(branch->thenBody, environment);
                rewriteStatements(branch->elseBody, environment);
            } else if (auto* loop = dynamic_cast<WhileStmt*>(it->get())) {
                checkExpr(loop->condition, environment);
                rewriteStatements(loop->body, environment);
            } else if (auto* ret = dynamic_cast<ReturnStmt*>(it->get())) {
                checkExpr(ret->body, environment);
            } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(it->get())) {
                auto* assignment = dynamic_cast<BinOpExpr*>(exprStmt->expr.get());
                auto* target = assignment ? dynamic_cast<VarExpr*>(assignment->left.get()) : nullptr;
                auto dynamic = target ? dynamicTypes.find(target->name.originalTxt) : dynamicTypes.end();
                if (assignment && assignment->op.type == ASSIGN && target &&
                    dynamic != dynamicTypes.end()) {
                    const std::string name = target->name.originalTxt;
                    checkExpr(assignment->right, environment);
                    target->name.originalTxt += "$dynamic";
                    std::vector<std::unique_ptr<Stmt>> updates;
                    for (const auto& type : dynamic->second) {
                        auto branch = std::make_unique<IfStmt>();
                        branch->condition = makeTypeTest(name + "$dynamic", type);
                        auto update = std::make_unique<ExprStmt>();
                        auto variantAssignment = std::make_unique<BinOpExpr>();
                        auto variant = std::make_unique<VarExpr>();
                        variant->name = makeToken(IDENTIFIER, name + "$" + type,
                                                  target->name.line, target->name.col);
                        variantAssignment->left = std::move(variant);
                        variantAssignment->op = makeToken(ASSIGN, "=", target->name.line,
                                                         target->name.col);
                        auto backing = std::make_unique<VarExpr>();
                        backing->name = makeToken(IDENTIFIER, name + "$dynamic",
                                                  target->name.line, target->name.col);
                        variantAssignment->right = makeCast(std::move(backing), type);
                        update->expr = std::move(variantAssignment);
                        branch->thenBody.push_back(std::move(update));
                        updates.push_back(std::move(branch));
                    }
                    const auto insertion = std::next(it);
                    it = statements.insert(insertion, std::make_move_iterator(updates.begin()),
                                           std::make_move_iterator(updates.end()));
                    std::advance(it, static_cast<std::ptrdiff_t>(updates.size()));
                    continue;
                }
                checkExpr(exprStmt->expr, environment);
            }
            ++it;
        }
    }

public:
    void check(std::vector<std::unique_ptr<Stmt>>& ast) {
        classNames.clear();
        dynamicTypes.clear();
        functionRoutes.clear();
        functionParameters.clear();
        functionParameterTypes.clear();
        functionDynamicParameters.clear();
        functionReturnTypes.clear();
        TypeEnvironment environment;
        collectClassNames(ast);
        collectFunctionSignatures(ast);
        collectClassesAndDyn(ast, environment);
        collectDynamicAssignments(ast, environment);
        for (std::size_t pass = 0; pass <= countFunctions(ast); ++pass) {
            collectClassesAndDyn(ast, environment);
            collectDynamicAssignments(ast, environment);
            collectCallArgumentTypes(ast, environment);
            analyzeFunctions(ast, "", environment);
        }
        collectClassesAndDyn(ast, environment);
        collectDynamicAssignments(ast, environment);
        splitFunctions(ast, "", environment);
        rewriteStatements(ast, std::move(environment));
    }
};
