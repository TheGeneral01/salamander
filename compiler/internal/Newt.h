/* =================================================================================================== */
/*                                                                                                     */
/*  Module: Newt.h                                                                                     */
/*  Description: Interprets the SAL AST and manages CPU/GPU execution paths.                           */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "astObjs.h"
#include "stdlib/print.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

class Newt {
    struct Object {
        std::string className;
    };

    struct Value {
        using List = std::vector<Value>;
        using ObjectPtr = std::shared_ptr<Object>;
        using Storage = std::variant<std::monostate, int64_t, double, bool,
                                      std::string, List, ObjectPtr>;

        Storage data;

        Value() = default;
        template <typename T>
        Value(T value) : data(std::move(value)) {}
    };

    enum class FlowKind { NORMAL, BREAK, CONTINUE, RETURN };

    struct Flow {
        FlowKind kind = FlowKind::NORMAL;
        Value value;
    };

    std::unordered_map<std::string, Value> variables;
    std::unordered_map<std::string, const ClassDefStmt*> classes;

    static std::string typeName(const Value& value) {
        if (std::holds_alternative<int64_t>(value.data)) return "int";
        if (std::holds_alternative<double>(value.data)) return "float";
        if (std::holds_alternative<bool>(value.data)) return "bool";
        if (std::holds_alternative<std::string>(value.data)) return "string";
        if (std::holds_alternative<Value::List>(value.data)) return "list";
        if (auto object = std::get_if<Value::ObjectPtr>(&value.data)) {
            return *object ? (*object)->className : "null";
        }
        return "null";
    }

    static double number(const Value& value) {
        if (auto item = std::get_if<int64_t>(&value.data)) return static_cast<double>(*item);
        if (auto item = std::get_if<double>(&value.data)) return *item;
        throw std::runtime_error("Expected a numeric value");
    }

    static bool truthy(const Value& value) {
        if (auto item = std::get_if<bool>(&value.data)) return *item;
        if (auto item = std::get_if<int64_t>(&value.data)) return *item != 0;
        if (auto item = std::get_if<double>(&value.data)) return *item != 0.0;
        if (auto item = std::get_if<std::string>(&value.data)) return !item->empty();
        if (auto item = std::get_if<Value::ObjectPtr>(&value.data)) return static_cast<bool>(*item);
        if (auto item = std::get_if<Value::List>(&value.data)) return !item->empty();
        return false;
    }

    static bool equal(const Value& left, const Value& right) {
        if (typeName(left) != typeName(right)) return false;
        if (auto item = std::get_if<int64_t>(&left.data)) return *item == std::get<int64_t>(right.data);
        if (auto item = std::get_if<double>(&left.data)) return *item == std::get<double>(right.data);
        if (auto item = std::get_if<bool>(&left.data)) return *item == std::get<bool>(right.data);
        if (auto item = std::get_if<std::string>(&left.data)) return *item == std::get<std::string>(right.data);
        if (auto item = std::get_if<Value::ObjectPtr>(&left.data)) return *item == std::get<Value::ObjectPtr>(right.data);
        return std::holds_alternative<std::monostate>(left.data);
    }

    static std::string display(const Value& value) {
        if (auto item = std::get_if<int64_t>(&value.data)) return std::to_string(*item);
        if (auto item = std::get_if<double>(&value.data)) return std::to_string(*item);
        if (auto item = std::get_if<bool>(&value.data)) return *item ? "True" : "False";
        if (auto item = std::get_if<std::string>(&value.data)) return *item;
        if (auto item = std::get_if<Value::List>(&value.data)) {
            std::string result = "[";
            for (std::size_t index = 0; index < item->size(); ++index) {
                if (index != 0) result += ", ";
                if (std::holds_alternative<std::string>((*item)[index].data)) {
                    result += "\"" + display((*item)[index]) + "\"";
                } else {
                    result += display((*item)[index]);
                }
            }
            return result + "]";
        }
        if (std::holds_alternative<Value::ObjectPtr>(value.data)) return "<" + typeName(value) + " object>";
        return "null";
    }

    static Value cast(Value value, const std::string& target) {
        if (target == "dyn" || target == typeName(value)) return value;
        if (target == "int") {
            if (std::holds_alternative<bool>(value.data)) return Value(int64_t(truthy(value)));
            if (auto item = std::get_if<std::string>(&value.data)) {
                try { return Value(static_cast<int64_t>(std::stoll(*item))); }
                catch (const std::exception&) { throw std::runtime_error("Cannot cast string to int"); }
            }
            return Value(static_cast<int64_t>(number(value)));
        }
        if (target == "float") {
            if (auto item = std::get_if<std::string>(&value.data)) {
                try { return Value(std::stod(*item)); }
                catch (const std::exception&) { throw std::runtime_error("Cannot cast string to float"); }
            }
            if (std::holds_alternative<bool>(value.data)) return Value(truthy(value) ? 1.0 : 0.0);
            return Value(number(value));
        }
        if (target == "bool") {
            if (auto item = std::get_if<std::string>(&value.data)) {
                if (*item == "True" || *item == "true") return Value(true);
                if (*item == "False" || *item == "false") return Value(false);
                throw std::runtime_error("Cannot cast string to bool");
            }
            return Value(truthy(value));
        }
        if (target == "string" || target == "str") return Value(display(value));
        if (target == "list" && std::holds_alternative<Value::List>(value.data)) return value;
        throw std::runtime_error("Unsupported SAL cast to " + target);
    }

    Value variable(const std::string& name) const {
        auto found = variables.find(name);
        if (found != variables.end()) return found->second;
        found = variables.find(name + "$dynamic");
        if (found != variables.end()) return found->second;
        const auto separator = name.rfind('$');
        if (separator != std::string::npos) {
            found = variables.find(name.substr(0, separator) + "$dynamic");
            if (found != variables.end()) return cast(found->second, name.substr(separator + 1));
        }
        throw std::runtime_error("Undefined SAL variable: " + name);
    }

    void assign(const std::string& name, Value value) {
        variables[name] = std::move(value);
    }

    Value evaluate(const Expr* expr) {
        if (!expr || dynamic_cast<const NullExpr*>(expr)) return {};
        if (auto literal = dynamic_cast<const LiteralExpr*>(expr)) {
            const auto& token = literal->value;
            switch (token.type) {
                case INTLIT: return Value(static_cast<int64_t>(std::stoll(token.originalTxt)));
                case FLOATLIT: return Value(std::stod(token.originalTxt));
                case STRLIT:
                case FSTRLIT:
                    return Value(token.originalTxt);
                case TRUE: return Value(true);
                case FALSE: return Value(false);
                default: throw std::runtime_error("Unsupported SAL literal: " + token.originalTxt);
            }
        }
        if (auto item = dynamic_cast<const VarExpr*>(expr)) return variable(item->name.originalTxt);
        if (auto item = dynamic_cast<const IndexExpr*>(expr)) {
            Value collection = evaluate(item->target.get());
            auto values = std::get_if<Value::List>(&collection.data);
            if (!values) throw std::runtime_error("Indexed SAL value is not a list");
            const auto index = static_cast<int64_t>(number(evaluate(item->index.get())));
            if (index < 0 || static_cast<std::size_t>(index) >= values->size()) {
                throw std::runtime_error("SAL list index is out of bounds");
            }
            return (*values)[static_cast<std::size_t>(index)];
        }
        if (auto item = dynamic_cast<const AsyncMapExpr*>(expr)) return evaluateAsyncMap(*item);
        if (auto item = dynamic_cast<const CastExpr*>(expr)) return cast(evaluate(item->value.get()), item->targetType.originalTxt);
        if (auto item = dynamic_cast<const TypeIsExpr*>(expr)) return Value(typeName(evaluate(item->value.get())) == item->checkedType.originalTxt);
        if (auto item = dynamic_cast<const BinOpExpr*>(expr)) return evaluateBinary(*item);
        if (auto item = dynamic_cast<const UnOpExpr*>(expr)) return evaluateUnary(*item);
        if (auto item = dynamic_cast<const CallExpr*>(expr)) return evaluateCall(*item);
        if (auto item = dynamic_cast<const ListExpr*>(expr)) {
            Value::List values;
            for (const auto& child : item->itms) values.push_back(evaluate(child.get()));
            return Value(std::move(values));
        }
        if (auto item = dynamic_cast<const TypeDispatchExpr*>(expr)) {
            for (const auto& branch : item->cases) {
                auto savedVariables = variables;
                Flow setup = executeBlock(branch.statements);
                if (setup.kind != FlowKind::NORMAL) throw std::runtime_error("Control flow is not allowed in SAL type dispatch setup");
                bool matches = true;
                for (const auto& guard : branch.guards) {
                    Value result = evaluate(guard.value.get());
                    bool passed = guard.isCondition || std::holds_alternative<bool>(result.data)
                        ? truthy(result)
                        : typeName(result) == guard.checkedType.originalTxt;
                    if (guard.negate) passed = !passed;
                    if (!passed) { matches = false; break; }
                }
                if (matches) return evaluate(branch.value.get());
                variables = std::move(savedVariables);
            }
            throw std::runtime_error("No matching SAL type-dispatch case");
        }
        throw std::runtime_error("Unsupported SAL expression node");
    }

    Value evaluateBinary(const BinOpExpr& expression) {
        auto target = dynamic_cast<const VarExpr*>(expression.left.get());
        auto indexedTarget = dynamic_cast<const IndexExpr*>(expression.left.get());
        const auto op = expression.op.type;
        if (op == ASSIGN || op == ADDASGN || op == SUBASGN || op == MULTASGN || op == DIVASGN) {
            if (indexedTarget && op == ASSIGN) {
                auto listVariable = dynamic_cast<const VarExpr*>(indexedTarget->target.get());
                if (!listVariable) throw std::runtime_error("SAL indexed assignment requires a list variable");
                const auto index = static_cast<int64_t>(number(evaluate(indexedTarget->index.get())));
                Value collection = variable(listVariable->name.originalTxt);
                auto values = std::get_if<Value::List>(&collection.data);
                if (!values || index < 0 || static_cast<std::size_t>(index) >= values->size()) {
                    throw std::runtime_error("SAL indexed assignment is out of bounds");
                }
                Value right = evaluate(expression.right.get());
                (*values)[static_cast<std::size_t>(index)] = right;
                assign(listVariable->name.originalTxt, std::move(collection));
                return right;
            }
            if (!target) throw std::runtime_error("SAL assignment target must be a variable");
            Value right = evaluate(expression.right.get());
            if (op != ASSIGN) {
                Value left = variable(target->name.originalTxt);
                const auto arithmetic = op == ADDASGN ? ADD : op == SUBASGN ? SUB : op == MULTASGN ? MULT : DIVD;
                right = applyOperator(arithmetic, left, right);
            }
            assign(target->name.originalTxt, right);
            return right;
        }
        return applyOperator(op, evaluate(expression.left.get()), evaluate(expression.right.get()));
    }

    static Value applyOperator(SalTknType op, const Value& left, const Value& right) {
        if (op == EQUAL) return Value(equal(left, right));
        if (op == NEQUAL) return Value(!equal(left, right));
        if (op == AND) return Value(truthy(left) && truthy(right));
        if (op == OR) return Value(truthy(left) || truthy(right));
        if (op == ADD && std::holds_alternative<std::string>(left.data) &&
            std::holds_alternative<std::string>(right.data)) {
            return Value(std::get<std::string>(left.data) + std::get<std::string>(right.data));
        }
        if (op == LESS) return Value(number(left) < number(right));
        if (op == GREATER) return Value(number(left) > number(right));
        if (op == LESSEQ) return Value(number(left) <= number(right));
        if (op == GREATEREQ) return Value(number(left) >= number(right));

        const bool integral = std::holds_alternative<int64_t>(left.data) &&
                              std::holds_alternative<int64_t>(right.data) && op != POW;
        const double lhs = number(left);
        const double rhs = number(right);
        if ((op == DIVD || op == DIVASGN) && rhs == 0.0) throw std::runtime_error("Division by zero");
        double result = 0;
        switch (op) {
            case ADD: case ADDASGN: result = lhs + rhs; break;
            case SUB: case SUBASGN: result = lhs - rhs; break;
            case MULT: case MULTASGN: result = lhs * rhs; break;
            case DIVD: case DIVASGN: result = lhs / rhs; break;
            case POW: result = std::pow(lhs, rhs); break;
            default: throw std::runtime_error("Unsupported SAL binary operator");
        }
        return integral ? Value(static_cast<int64_t>(result)) : Value(result);
    }

    Value evaluateUnary(const UnOpExpr& expression) {
        Value value = evaluate(expression.value.get());
        switch (expression.op.type) {
            case SUB:
                if (auto item = std::get_if<int64_t>(&value.data)) return Value(-*item);
                return Value(-number(value));
            case ROOT: return Value(std::sqrt(number(value)));
            case ADDONE: return applyOperator(ADD, value, Value(int64_t{1}));
            case SUBONE: return applyOperator(SUB, value, Value(int64_t{1}));
            case NOT: return Value(!truthy(value));
            default: throw std::runtime_error("Unsupported SAL unary operator");
        }
    }

    Value evaluateCall(const CallExpr& expression) {
        if (expression.funcName.originalTxt == "print") {
            if (expression.args.size() != 1) throw std::runtime_error("SAL print expects one argument");
            print(display(evaluate(expression.args.front().get())));
            return {};
        }
        const std::string lengthSuffix = ".length";
        if (expression.args.empty() && expression.funcName.originalTxt.size() > lengthSuffix.size() &&
            expression.funcName.originalTxt.ends_with(lengthSuffix)) {
            const std::string name = expression.funcName.originalTxt.substr(
                0, expression.funcName.originalTxt.size() - lengthSuffix.size());
            Value value = variable(name);
            if (auto text = std::get_if<std::string>(&value.data)) {
                return Value(static_cast<int64_t>(text->size()));
            }
            if (auto list = std::get_if<Value::List>(&value.data)) {
                return Value(static_cast<int64_t>(list->size()));
            }
            throw std::runtime_error("length() requires a string or list");
        }
        auto classDefinition = classes.find(expression.funcName.originalTxt);
        if (classDefinition != classes.end()) {
            if (!expression.args.empty()) throw std::runtime_error("SAL class constructors do not accept arguments yet");
            return Value(std::make_shared<Object>(Object{classDefinition->first}));
        }
        throw std::runtime_error("Unknown SAL function: " + expression.funcName.originalTxt);
    }

    Value evaluateAsyncMap(const AsyncMapExpr& expression) const {
        Newt initialContext = *this;
        Value source = initialContext.evaluate(expression.source.get());
        auto items = std::get_if<Value::List>(&source.data);
        if (!items) throw std::runtime_error("async map source must be a list");
        if (items->empty()) return Value(Value::List{});

        Value::List results;
        std::mutex resultsMutex;
        Newt sampleContext = initialContext;
        sampleContext.assign(expression.iterator.originalTxt, items->front());
        if (typeName(items->front()) != "null") {
            sampleContext.assign(expression.iterator.originalTxt + "$dynamic", items->front());
            sampleContext.assign(expression.iterator.originalTxt + "$" + typeName(items->front()), items->front());
        }
        results.push_back(sampleContext.evaluate(expression.operation.get()));

        const unsigned int logicalWorkers = std::max(1u, std::thread::hardware_concurrency());
        const unsigned int workerCount = std::max(1u, logicalWorkers * 4u / 5u);
        std::atomic_size_t nextIndex{1};
        std::exception_ptr failure;
        std::mutex failureMutex;
        std::vector<std::thread> workers;
        const std::size_t count = items->size();
        for (unsigned int workerIndex = 0; workerIndex < workerCount && workerIndex + 1 < count; ++workerIndex) {
            workers.emplace_back([&, workerIndex] {
                (void)workerIndex;
                while (true) {
                    const std::size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (index >= count) return;
                    {
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (failure) return;
                    }
                    try {
                        Newt itemContext = initialContext;
                        const Value& item = (*items)[index];
                        itemContext.assign(expression.iterator.originalTxt, item);
                        itemContext.assign(expression.iterator.originalTxt + "$dynamic", item);
                        itemContext.assign(expression.iterator.originalTxt + "$" + typeName(item), item);
                        Value result = itemContext.evaluate(expression.operation.get());
                        std::lock_guard<std::mutex> lock(resultsMutex);
                        results.push_back(std::move(result));
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure) failure = std::current_exception();
                    }
                }
            });
        }
        for (auto& worker : workers) worker.join();
        if (failure) std::rethrow_exception(failure);
        return Value(std::move(results));
    }

    Flow execute(const Stmt* statement) {
        if (!statement || dynamic_cast<const NullStmt*>(statement)) return {};
        if (auto item = dynamic_cast<const VarDeclStmt*>(statement)) {
            Value value = evaluate(item->init.get());
            if (!item->type.originalTxt.empty() && item->type.originalTxt != "dyn") {
                value = cast(std::move(value), item->type.originalTxt);
            }
            assign(item->name.originalTxt, std::move(value));
            return {};
        }
        if (auto item = dynamic_cast<const ExprStmt*>(statement)) {
            evaluate(item->expr.get());
            return {};
        }
        if (auto item = dynamic_cast<const IfStmt*>(statement)) {
            return executeBlock(truthy(evaluate(item->condition.get())) ? item->thenBody : item->elseBody);
        }
        if (auto item = dynamic_cast<const WhileStmt*>(statement)) {
            if (item->shader.gpuExecuted) return {};
            if (executeParallelWhile(*item)) return {};
            while (truthy(evaluate(item->condition.get()))) {
                Flow flow = executeBlock(item->body);
                if (flow.kind == FlowKind::RETURN) return flow;
                if (flow.kind == FlowKind::BREAK) break;
            }
            return {};
        }
        if (dynamic_cast<const BreakStmt*>(statement)) return {FlowKind::BREAK, {}};
        if (dynamic_cast<const ContinueStmt*>(statement)) return {FlowKind::CONTINUE, {}};
        if (auto item = dynamic_cast<const ReturnStmt*>(statement)) return {FlowKind::RETURN, evaluate(item->body.get())};
        if (auto item = dynamic_cast<const ClassDefStmt*>(statement)) {
            classes[item->name.originalTxt] = item;
            return {};
        }
        if (dynamic_cast<const FuncDefStmt*>(statement)) return {};
        if (dynamic_cast<const ImportStmt*>(statement)) throw std::runtime_error("SAL imports are not supported by the interpreter yet");
        throw std::runtime_error("Unsupported SAL statement node");
    }

    Flow executeBlock(const std::vector<std::unique_ptr<Stmt>>& statements) {
        for (const auto& statement : statements) {
            Flow flow = execute(statement.get());
            if (flow.kind != FlowKind::NORMAL) return flow;
        }
        return {};
    }

    static bool isParallelValueExpression(const Expr* expression,
                                          const std::string& inductionVariable) {
        if (!expression || dynamic_cast<const NullExpr*>(expression) ||
            dynamic_cast<const LiteralExpr*>(expression)) return true;
        if (dynamic_cast<const VarExpr*>(expression)) return true;
        if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
            auto collection = dynamic_cast<const VarExpr*>(index->target.get());
            auto itemIndex = dynamic_cast<const VarExpr*>(index->index.get());
            return collection && itemIndex && itemIndex->name.originalTxt == inductionVariable;
        }
        if (auto binary = dynamic_cast<const BinOpExpr*>(expression)) {
            if (binary->op.type == ASSIGN || binary->op.type == ADDASGN ||
                binary->op.type == SUBASGN || binary->op.type == MULTASGN ||
                binary->op.type == DIVASGN) return false;
            return isParallelValueExpression(binary->left.get(), inductionVariable) &&
                   isParallelValueExpression(binary->right.get(), inductionVariable);
        }
        if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
            return isParallelValueExpression(unary->value.get(), inductionVariable);
        }
        if (auto cast = dynamic_cast<const CastExpr*>(expression)) {
            return isParallelValueExpression(cast->value.get(), inductionVariable);
        }
        if (auto test = dynamic_cast<const TypeIsExpr*>(expression)) {
            return isParallelValueExpression(test->value.get(), inductionVariable);
        }
        return false;
    }

    bool executeParallelWhile(const WhileStmt& loop) {
        auto condition = dynamic_cast<const BinOpExpr*>(loop.condition.get());
        auto induction = condition && condition->op.type == LESS
            ? dynamic_cast<const VarExpr*>(condition->left.get()) : nullptr;
        if (!induction || loop.body.empty()) return false;

        const std::string inductionName = induction->name.originalTxt;
        auto initial = variables.find(inductionName);
        if (initial == variables.end() || !std::holds_alternative<int64_t>(initial->second.data)) return false;
        Value boundValue;
        try {
            boundValue = evaluate(condition->right.get());
        } catch (const std::exception&) {
            return false;
        }
        if (!std::holds_alternative<int64_t>(boundValue.data)) return false;
        const int64_t begin = std::get<int64_t>(initial->second.data);
        const int64_t end = std::get<int64_t>(boundValue.data);
        if (begin < 0 || end <= begin || end - begin > 1000000) return false;

        std::unordered_set<std::string> outputNames;
        const BinOpExpr* counterUpdate = nullptr;
        for (const auto& statement : loop.body) {
            auto expression = dynamic_cast<const ExprStmt*>(statement.get());
            auto binary = expression ? dynamic_cast<const BinOpExpr*>(expression->expr.get()) : nullptr;
            if (!binary || binary->op.type != ASSIGN) return false;
            if (auto target = dynamic_cast<const VarExpr*>(binary->left.get())) {
                if (target->name.originalTxt != inductionName || counterUpdate) return false;
                auto increment = dynamic_cast<const BinOpExpr*>(binary->right.get());
                auto incrementTarget = increment ? dynamic_cast<const VarExpr*>(increment->left.get()) : nullptr;
                auto amount = increment ? dynamic_cast<const LiteralExpr*>(increment->right.get()) : nullptr;
                if (!increment || increment->op.type != ADD || !incrementTarget ||
                    incrementTarget->name.originalTxt != inductionName || !amount ||
                    amount->value.type != INTLIT || amount->value.originalTxt != "1") return false;
                counterUpdate = binary;
                continue;
            }
            auto target = dynamic_cast<const IndexExpr*>(binary->left.get());
            auto collection = target ? dynamic_cast<const VarExpr*>(target->target.get()) : nullptr;
            auto itemIndex = target ? dynamic_cast<const VarExpr*>(target->index.get()) : nullptr;
            if (!collection || !itemIndex || itemIndex->name.originalTxt != inductionName ||
                !isParallelValueExpression(binary->right.get(), inductionName)) return false;
            auto output = variables.find(collection->name.originalTxt);
            auto outputList = output == variables.end() ? nullptr : std::get_if<Value::List>(&output->second.data);
            if (!outputList || static_cast<std::size_t>(end) > outputList->size()) return false;
            outputNames.insert(collection->name.originalTxt);
        }
        if (!counterUpdate || outputNames.empty()) return false;

        struct IndexedWrite {
            std::string output;
            std::size_t index;
            Value value;
        };
        std::vector<IndexedWrite> writes;
        std::mutex writesMutex;
        auto executeIteration = [&](int64_t value) {
            Newt isolated = *this;
            isolated.assign(inductionName, Value(value));
            Flow result = isolated.executeBlock(loop.body);
            if (result.kind != FlowKind::NORMAL) {
                throw std::runtime_error("Parallel SAL loop body attempted control flow");
            }
            std::vector<IndexedWrite> iterationWrites;
            for (const auto& outputName : outputNames) {
                auto changed = isolated.variables.find(outputName);
                auto original = variables.find(outputName);
                auto changedList = changed == isolated.variables.end()
                    ? nullptr : std::get_if<Value::List>(&changed->second.data);
                auto originalList = original == variables.end()
                    ? nullptr : std::get_if<Value::List>(&original->second.data);
                if (!changedList || !originalList) throw std::runtime_error("Parallel output must remain a list");
                iterationWrites.push_back({outputName, static_cast<std::size_t>(value),
                                           (*changedList)[static_cast<std::size_t>(value)]});
            }
            std::lock_guard<std::mutex> lock(writesMutex);
            writes.insert(writes.end(), std::make_move_iterator(iterationWrites.begin()),
                          std::make_move_iterator(iterationWrites.end()));
        };

        const auto sampleStart = std::chrono::steady_clock::now();
        try {
            executeIteration(begin);
        } catch (const std::exception&) {
            return false;
        }
        const auto sampleElapsed = std::chrono::steady_clock::now() - sampleStart;
        (void)sampleElapsed;

        const unsigned int logicalWorkers = std::max(1u, std::thread::hardware_concurrency());
        const unsigned int workerCount = std::max(1u, logicalWorkers * 4u / 5u);
        std::atomic_int64_t next{begin + 1};
        std::exception_ptr failure;
        std::mutex failureMutex;
        std::vector<std::thread> workers;
        for (unsigned int workerIndex = 0; workerIndex < workerCount && begin + workerIndex + 1 < end;
             ++workerIndex) {
            workers.emplace_back([&] {
                while (true) {
                    const int64_t iteration = next.fetch_add(1, std::memory_order_relaxed);
                    if (iteration >= end) return;
                    {
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (failure) return;
                    }
                    try {
                        executeIteration(iteration);
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(failureMutex);
                        if (!failure) failure = std::current_exception();
                        return;
                    }
                }
            });
        }
        for (auto& worker : workers) worker.join();
        if (failure) std::rethrow_exception(failure);

        for (const auto& write : writes) {
            auto& output = std::get<Value::List>(variables.at(write.output).data);
            output[write.index] = write.value;
        }
        assign(inductionName, Value(end));
        return true;
    }

    public:
    void runAST(const std::vector<std::unique_ptr<Stmt>>& ast) {
        variables.clear();
        classes.clear();
        for (const auto& statement : ast) {
            if (auto classDefinition = dynamic_cast<const ClassDefStmt*>(statement.get())) {
                classes[classDefinition->name.originalTxt] = classDefinition;
            }
        }
        Flow flow = executeBlock(ast);
        if (flow.kind != FlowKind::NORMAL) throw std::runtime_error("Unexpected control flow at SAL top level");
    }
};