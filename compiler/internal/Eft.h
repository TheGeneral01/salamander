/* =================================================================================================== */
/*                                                                                                     */
/*  Module: Eft.h                                                                                      */
/*  Description: Preprocesses and compiles SPIRV bytecode, adds optimizations.                         */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "astObjs.h"
#include "SALItms.h"
#include "fileScope.h"
#include "TypeChecker.h"
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

class Preprocessor {
    private:
    std::unordered_map<std::string, std::unique_ptr<FuncDefStmt>> functionNodes;

    using ExprBindings = std::unordered_map<std::string, const Expr*>;

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
        if (auto* index = dynamic_cast<const IndexExpr*>(expr)) {
            auto copy = std::make_unique<IndexExpr>();
            copy->target = cloneExpr(index->target.get());
            copy->index = cloneExpr(index->index.get());
            return copy;
        }
        if (auto* map = dynamic_cast<const AsyncMapExpr*>(expr)) {
            auto copy = std::make_unique<AsyncMapExpr>();
            copy->operation = cloneExpr(map->operation.get());
            copy->iterator = map->iterator;
            copy->source = cloneExpr(map->source.get());
            copy->elementTypes = map->elementTypes;
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
                for (const auto& stmt : item.statements) newCase.statements.push_back(cloneStmt(stmt.get()));
                for (const auto& guard : item.guards) {
                    newCase.guards.push_back({cloneExpr(guard.value.get()), guard.checkedType,
                                              guard.isCondition, guard.negate});
                }
                copy->cases.push_back(std::move(newCase));
            }
            return copy;
        }
        throw std::runtime_error("Cannot inline an unsupported expression node");
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
        if (auto* var = dynamic_cast<const VarDeclStmt*>(stmt)) {
            auto copy = std::make_unique<VarDeclStmt>();
            copy->attributes = var->attributes;
            copy->type = var->type;
            copy->name = var->name;
            copy->init = cloneExpr(var->init.get());
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
        if (auto* classDef = dynamic_cast<const ClassDefStmt*>(stmt)) {
            auto copy = std::make_unique<ClassDefStmt>();
            copy->name = classDef->name;
            copy->inheritance = classDef->inheritance;
            copy->attributes = classDef->attributes;
            for (const auto& child : classDef->body) copy->body.push_back(cloneStmt(child.get()));
            return copy;
        }
        throw std::runtime_error("Cannot inline an unsupported statement node");
    }

    static std::unique_ptr<Expr> substituteExpr(
        std::unique_ptr<Expr> expr, const ExprBindings& bindings) {
        if (!expr) return nullptr;
        if (auto* variable = dynamic_cast<VarExpr*>(expr.get())) {
            auto binding = bindings.find(variable->name.originalTxt);
            return binding == bindings.end() ? std::move(expr) : cloneExpr(binding->second);
        }
        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            index->target = substituteExpr(std::move(index->target), bindings);
            index->index = substituteExpr(std::move(index->index), bindings);
        } else if (auto* map = dynamic_cast<AsyncMapExpr*>(expr.get())) {
            map->operation = substituteExpr(std::move(map->operation), bindings);
            map->source = substituteExpr(std::move(map->source), bindings);
        } else if (auto* binary = dynamic_cast<BinOpExpr*>(expr.get())) {
            binary->left = substituteExpr(std::move(binary->left), bindings);
            binary->right = substituteExpr(std::move(binary->right), bindings);
        } else if (auto* unary = dynamic_cast<UnOpExpr*>(expr.get())) {
            unary->value = substituteExpr(std::move(unary->value), bindings);
        } else if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
            for (auto& arg : call->args) arg = substituteExpr(std::move(arg), bindings);
        } else if (auto* list = dynamic_cast<ListExpr*>(expr.get())) {
            for (auto& item : list->itms) item = substituteExpr(std::move(item), bindings);
        } else if (auto* cast = dynamic_cast<CastExpr*>(expr.get())) {
            cast->value = substituteExpr(std::move(cast->value), bindings);
        } else if (auto* test = dynamic_cast<TypeIsExpr*>(expr.get())) {
            test->value = substituteExpr(std::move(test->value), bindings);
        } else if (auto* dispatch = dynamic_cast<TypeDispatchExpr*>(expr.get())) {
            for (auto& item : dispatch->cases) {
                for (auto& guard : item.guards) guard.value = substituteExpr(std::move(guard.value), bindings);
                substituteStatements(item.statements, bindings);
                item.value = substituteExpr(std::move(item.value), bindings);
            }
        }
        return expr;
    }

    static void substituteStatements(
        std::vector<std::unique_ptr<Stmt>>& statements, const ExprBindings& bindings) {
        for (auto& stmt : statements) {
            if (auto* var = dynamic_cast<VarDeclStmt*>(stmt.get())) {
                var->init = substituteExpr(std::move(var->init), bindings);
            } else if (auto* ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
                ret->body = substituteExpr(std::move(ret->body), bindings);
            } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get())) {
                exprStmt->expr = substituteExpr(std::move(exprStmt->expr), bindings);
            } else if (auto* branch = dynamic_cast<IfStmt*>(stmt.get())) {
                branch->condition = substituteExpr(std::move(branch->condition), bindings);
                substituteStatements(branch->thenBody, bindings);
                substituteStatements(branch->elseBody, bindings);
            } else if (auto* loop = dynamic_cast<WhileStmt*>(stmt.get())) {
                loop->condition = substituteExpr(std::move(loop->condition), bindings);
                substituteStatements(loop->body, bindings);
            }
        }
    }

    static bool containsReturn(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (const auto& stmt : statements) {
            if (dynamic_cast<const ReturnStmt*>(stmt.get())) return true;
            if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                if (containsReturn(branch->thenBody) || containsReturn(branch->elseBody)) return true;
            }
            if (auto* loop = dynamic_cast<const WhileStmt*>(stmt.get())) {
                if (containsReturn(loop->body)) return true;
            }
        }
        return false;
    }

    struct InlineReturnPath {
        std::vector<TypeGuard> guards;
        std::vector<std::unique_ptr<Stmt>> statements;
        std::unique_ptr<Expr> value;
    };

    struct InlinePathState {
        std::vector<TypeGuard> guards;
        std::vector<std::unique_ptr<Stmt>> statements;
    };

    static std::vector<TypeGuard> cloneGuards(const std::vector<TypeGuard>& guards) {
        std::vector<TypeGuard> copies;
        for (const auto& guard : guards) {
            copies.push_back({cloneExpr(guard.value.get()), guard.checkedType,
                              guard.isCondition, guard.negate});
        }
        return copies;
    }

    static std::vector<InlinePathState> collectInlineReturnPaths(
        const std::vector<std::unique_ptr<Stmt>>& statements,
        std::vector<InlinePathState> active,
        std::vector<InlineReturnPath>& paths) {
        for (const auto& stmt : statements) {
            if (auto* ret = dynamic_cast<const ReturnStmt*>(stmt.get())) {
                for (auto& state : active) {
                    paths.push_back({std::move(state.guards), std::move(state.statements),
                                     cloneExpr(ret->body.get())});
                }
                return {};
            } else if (auto* branch = dynamic_cast<const IfStmt*>(stmt.get())) {
                std::vector<InlinePathState> continuing;
                for (const auto& state : active) {
                    InlinePathState thenState{cloneGuards(state.guards), {}};
                    for (const auto& previous : state.statements) thenState.statements.push_back(cloneStmt(previous.get()));
                    thenState.guards.push_back({cloneExpr(branch->condition.get()), makeToken(TYPE, "", 0, 0), true, false});
                    std::vector<InlinePathState> thenInitial;
                    thenInitial.push_back(std::move(thenState));
                    auto thenContinuing = collectInlineReturnPaths(branch->thenBody,
                                                                   std::move(thenInitial), paths);
                    for (auto& item : thenContinuing) continuing.push_back(std::move(item));

                    InlinePathState elseState{cloneGuards(state.guards), {}};
                    for (const auto& previous : state.statements) elseState.statements.push_back(cloneStmt(previous.get()));
                    elseState.guards.push_back({cloneExpr(branch->condition.get()), makeToken(TYPE, "", 0, 0), true, true});
                    std::vector<InlinePathState> elseInitial;
                    elseInitial.push_back(std::move(elseState));
                    auto elseContinuing = collectInlineReturnPaths(branch->elseBody,
                                                                   std::move(elseInitial), paths);
                    for (auto& item : elseContinuing) continuing.push_back(std::move(item));
                }
                active = std::move(continuing);
            } else {
                for (auto& state : active) state.statements.push_back(cloneStmt(stmt.get()));
            }
        }
        return active;
    }

    const FuncDefStmt* findFunction(const std::string& name) const {
        auto function = functionNodes.find(name);
        return function == functionNodes.end() ? nullptr : function->second.get();
    }

    ExprBindings makeBindings(const FuncDefStmt& function, const CallExpr& call) const {
        if (function.params.size() != call.args.size()) {
            throw std::runtime_error("Wrong number of arguments for inlined function: " +
                                     function.name.originalTxt);
        }
        ExprBindings bindings;
        for (std::size_t index = 0; index < function.params.size(); ++index) {
            bindings.emplace(function.params[index].name.originalTxt, call.args[index].get());
        }
        return bindings;
    }

    std::unique_ptr<Expr> inlineExpr(
        std::unique_ptr<Expr> expr, std::unordered_set<std::string>& activeFunctions) {
        if (!expr) return nullptr;
        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            index->target = inlineExpr(std::move(index->target), activeFunctions);
            index->index = inlineExpr(std::move(index->index), activeFunctions);
        } else if (auto* map = dynamic_cast<AsyncMapExpr*>(expr.get())) {
            map->source = inlineExpr(std::move(map->source), activeFunctions);
            map->operation = inlineExpr(std::move(map->operation), activeFunctions);
        } else if (auto* binary = dynamic_cast<BinOpExpr*>(expr.get())) {
            binary->left = inlineExpr(std::move(binary->left), activeFunctions);
            binary->right = inlineExpr(std::move(binary->right), activeFunctions);
        } else if (auto* unary = dynamic_cast<UnOpExpr*>(expr.get())) {
            unary->value = inlineExpr(std::move(unary->value), activeFunctions);
        } else if (auto* list = dynamic_cast<ListExpr*>(expr.get())) {
            for (auto& item : list->itms) item = inlineExpr(std::move(item), activeFunctions);
        } else if (auto* cast = dynamic_cast<CastExpr*>(expr.get())) {
            cast->value = inlineExpr(std::move(cast->value), activeFunctions);
        } else if (auto* test = dynamic_cast<TypeIsExpr*>(expr.get())) {
            test->value = inlineExpr(std::move(test->value), activeFunctions);
        } else if (auto* dispatch = dynamic_cast<TypeDispatchExpr*>(expr.get())) {
            std::vector<TypeDispatchCase> flattenedCases;
            for (auto& item : dispatch->cases) {
                for (auto& guard : item.guards) guard.value = inlineExpr(std::move(guard.value), activeFunctions);
                inlineStatements(item.statements, activeFunctions);
                item.value = inlineExpr(std::move(item.value), activeFunctions);
                auto* nested = dynamic_cast<TypeDispatchExpr*>(item.value.get());
                if (!nested) {
                    flattenedCases.push_back(std::move(item));
                    continue;
                }
                std::unique_ptr<TypeDispatchExpr> nestedDispatch(
                    static_cast<TypeDispatchExpr*>(item.value.release()));
                for (auto& nestedCase : nestedDispatch->cases) {
                    TypeDispatchCase merged;
                    merged.guards = cloneGuards(item.guards);
                    for (auto& guard : nestedCase.guards) merged.guards.push_back(std::move(guard));
                    for (const auto& stmt : item.statements) merged.statements.push_back(cloneStmt(stmt.get()));
                    for (auto& stmt : nestedCase.statements) merged.statements.push_back(std::move(stmt));
                    merged.value = std::move(nestedCase.value);
                    flattenedCases.push_back(std::move(merged));
                }
            }
            dispatch->cases = std::move(flattenedCases);
        } else if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
            for (auto& arg : call->args) arg = inlineExpr(std::move(arg), activeFunctions);
            const std::string name = call->funcName.originalTxt;
            const FuncDefStmt* function = findFunction(name);
            if (!function) return expr;
            if (!activeFunctions.insert(name).second) {
                throw std::runtime_error("Recursive function cannot be fully inlined: " + name);
            }
            ExprBindings bindings = makeBindings(*function, *call);
            std::unique_ptr<Expr> expanded;
            if (function->body.size() == 1) {
                if (auto* ret = dynamic_cast<const ReturnStmt*>(function->body.front().get())) {
                    expanded = substituteExpr(cloneExpr(ret->body.get()), bindings);
                }
            }
            if (!expanded) {
                std::vector<InlineReturnPath> paths;
                InlinePathState initial;
                std::vector<InlinePathState> initialStates;
                initialStates.push_back(std::move(initial));
                auto fallthrough = collectInlineReturnPaths(function->body, std::move(initialStates), paths);
                if (!fallthrough.empty()) {
                    activeFunctions.erase(name);
                    throw std::runtime_error("Function can complete without returning a value: " + name);
                }
                if (paths.empty()) {
                    activeFunctions.erase(name);
                    throw std::runtime_error("Expression call requires a returned value: " + name);
                }
                auto dispatch = std::make_unique<TypeDispatchExpr>();
                for (auto& path : paths) {
                    TypeDispatchCase item;
                    item.statements = std::move(path.statements);
                    substituteStatements(item.statements, bindings);
                    for (auto& guard : path.guards) {
                        guard.value = substituteExpr(std::move(guard.value), bindings);
                        item.guards.push_back({inlineExpr(std::move(guard.value), activeFunctions),
                                               guard.checkedType, guard.isCondition, guard.negate});
                    }
                    item.value = inlineExpr(
                        substituteExpr(std::move(path.value), bindings), activeFunctions);
                    dispatch->cases.push_back(std::move(item));
                }
                expanded = std::move(dispatch);
            } else {
                expanded = inlineExpr(std::move(expanded), activeFunctions);
            }
            activeFunctions.erase(name);
            return expanded;
        }
        return expr;
    }

    void inlineStatements(
        std::vector<std::unique_ptr<Stmt>>& statements,
        std::unordered_set<std::string>& activeFunctions) {
        for (std::size_t index = 0; index < statements.size();) {
            if (auto* exprStmt = dynamic_cast<ExprStmt*>(statements[index].get())) {
                auto* call = dynamic_cast<CallExpr*>(exprStmt->expr.get());
                const FuncDefStmt* function = call ? findFunction(call->funcName.originalTxt) : nullptr;
                if (function) {
                    const std::string name = call->funcName.originalTxt;
                    if (!activeFunctions.insert(name).second) {
                        throw std::runtime_error("Recursive function cannot be fully inlined: " + name);
                    }
                    ExprBindings bindings = makeBindings(*function, *call);
                    std::vector<std::unique_ptr<Stmt>> expanded;
                    if (function->body.size() == 1) {
                        if (auto* ret = dynamic_cast<const ReturnStmt*>(function->body.front().get())) {
                            if (ret->body) {
                                auto eval = std::make_unique<ExprStmt>();
                                eval->expr = substituteExpr(cloneExpr(ret->body.get()), bindings);
                                expanded.push_back(std::move(eval));
                            }
                        } else {
                            for (const auto& bodyStmt : function->body) {
                                expanded.push_back(cloneStmt(bodyStmt.get()));
                            }
                        }
                    } else {
                        if (containsReturn(function->body)) {
                            activeFunctions.erase(name);
                            throw std::runtime_error("Statement call cannot inline a function with return control flow: " + name);
                        }
                        for (const auto& bodyStmt : function->body) {
                            expanded.push_back(cloneStmt(bodyStmt.get()));
                        }
                    }
                    substituteStatements(expanded, bindings);
                    inlineStatements(expanded, activeFunctions);
                    activeFunctions.erase(name);
                    statements.erase(statements.begin() + static_cast<std::ptrdiff_t>(index));
                    statements.insert(statements.begin() + static_cast<std::ptrdiff_t>(index),
                                      std::make_move_iterator(expanded.begin()),
                                      std::make_move_iterator(expanded.end()));
                    index += expanded.size();
                    continue;
                }
            }

            Stmt* stmt = statements[index].get();
            if (auto* var = dynamic_cast<VarDeclStmt*>(stmt)) {
                var->init = inlineExpr(std::move(var->init), activeFunctions);
            } else if (auto* ret = dynamic_cast<ReturnStmt*>(stmt)) {
                ret->body = inlineExpr(std::move(ret->body), activeFunctions);
            } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
                exprStmt->expr = inlineExpr(std::move(exprStmt->expr), activeFunctions);
            } else if (auto* branch = dynamic_cast<IfStmt*>(stmt)) {
                branch->condition = inlineExpr(std::move(branch->condition), activeFunctions);
                inlineStatements(branch->thenBody, activeFunctions);
                inlineStatements(branch->elseBody, activeFunctions);
            } else if (auto* loop = dynamic_cast<WhileStmt*>(stmt)) {
                loop->condition = inlineExpr(std::move(loop->condition), activeFunctions);
                inlineStatements(loop->body, activeFunctions);
            } else if (auto* classDef = dynamic_cast<ClassDefStmt*>(stmt)) {
                inlineStatements(classDef->body, activeFunctions);
            }
            ++index;
        }
    }

    void extractFunctions(std::vector<std::unique_ptr<Stmt>>& statements, const std::string& scope) {
        for (auto it = statements.begin(); it != statements.end();) {
            if (auto* func = dynamic_cast<FuncDefStmt*>(it->get())) {
                const std::string key = scope.empty()
                    ? func->name.originalTxt
                    : scope + ":" + func->name.originalTxt;
                if (functionNodes.find(key) != functionNodes.end()) {
                    throw std::runtime_error("Duplicate flattened function: " + key);
                }

                std::unique_ptr<FuncDefStmt> ownedFunc(
                    static_cast<FuncDefStmt*>(it->release()));
                it = statements.erase(it);
                extractFunctions(ownedFunc->body, key);
                functionNodes.emplace(key, std::move(ownedFunc));
                continue;
            }

            Stmt* stmt = it->get();
            ++it;
            if (auto* classDef = dynamic_cast<ClassDefStmt*>(stmt)) {
                const std::string classScope = scope.empty()
                    ? classDef->name.originalTxt
                    : scope + ":" + classDef->name.originalTxt;
                extractFunctions(classDef->body, classScope);
            } else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
                extractFunctions(ifStmt->thenBody, scope);
                extractFunctions(ifStmt->elseBody, scope);
            } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
                extractFunctions(whileStmt->body, scope);
            }
        }
    }

    public:
    std::vector<std::unique_ptr<Stmt>> processedAST;

    void flattenAST(std::vector<std::unique_ptr<Stmt>> ast) {
        processedAST = std::move(ast);
        TypeChecker typeChecker;
        typeChecker.check(processedAST);
        functionNodes.clear();
        extractFunctions(processedAST, "");
        std::unordered_set<std::string> activeFunctions;
        inlineStatements(processedAST, activeFunctions);
        functionNodes.clear();
    }
};