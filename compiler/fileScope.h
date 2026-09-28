/* =================================================================================================== */
/*                                                                                                     */
/*  Module: fileScope.h                                                                                */
/*  Description: Scope tracking for type-checking with support for member access chains.               */
/*  Date Last Updated: 9/25/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "SALItms.h"
#include "astObjs.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

struct parsedSalFile {
    RawSalFile file;
    std::vector<std::unique_ptr<Stmt>> fileAST;

    parsedSalFile() = default;
    parsedSalFile(parsedSalFile&&) = default;
    parsedSalFile& operator=(parsedSalFile&&) = default;
    parsedSalFile(const parsedSalFile&) = delete;
    parsedSalFile& operator=(const parsedSalFile&) = delete;
};

struct ScopeLayer {
    int depth;
    std::unordered_map<std::string, VarDeclStmt*> variables;
    ScopeLayer* parent;
};

class Scope {
private:
    ScopeLayer* currentLayer = nullptr;
    ScopeLayer* globalLayer = nullptr;  // Keep reference to global
    std::unordered_map<std::string, ClassDefStmt*> classDefinitions;
    
    // Split "instance.member.field" into ["instance", "member", "field"]
    std::vector<std::string> splitChain(const std::string& chain) {
        std::vector<std::string> parts;
        std::string current;
        
        for (char c : chain) {
            if (c == '.') {
                if (current.empty()) return {};
                parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        
        if (current.empty()) return {};
        parts.push_back(current);
        
        return parts;
    }
    
    // Find a member variable in a class definition
    VarDeclStmt* findMemberInClass(ClassDefStmt* classDef, const std::string& memberName,
                                   std::unordered_set<ClassDefStmt*>& visited) {
        if (!classDef || !visited.insert(classDef).second) return nullptr;
        
        for (auto& stmt : classDef->body) {
            if (auto varDecl = dynamic_cast<VarDeclStmt*>(stmt.get())) {
                if (varDecl->name.originalTxt == memberName) {
                    return varDecl;
                }
            }
        }

        const std::string& parentName = classDef->inheritance.originalTxt;
        if (!parentName.empty()) {
            return findMemberInClass(getClassDef(parentName), memberName, visited);
        }
        
        return nullptr;
    }

    VarDeclStmt* findMemberInClass(ClassDefStmt* classDef, const std::string& memberName) {
        std::unordered_set<ClassDefStmt*> visited;
        return findMemberInClass(classDef, memberName, visited);
    }
    
    // Get the class definition for a type name
    ClassDefStmt* getClassDef(const std::string& typeName) {
        auto classDef = classDefinitions.find(typeName);
        return classDef == classDefinitions.end() ? nullptr : classDef->second;
    }
    
    // Check if a type is a built-in type
    bool isBuiltinType(const std::string& typeName) {
        return typeName == "int" || typeName == "float" || 
               typeName == "bool" || typeName == "string";
    }
    
public:
    Scope() {
        globalLayer = new ScopeLayer();
        globalLayer->depth = 0;
        globalLayer->parent = nullptr;
        currentLayer = globalLayer;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    // Declare a global variable (at file scope)
    void declareGlobal(const std::string& name, VarDeclStmt* var) {
        if (globalLayer) {
            globalLayer->variables[name] = var;
        }
    }

    // Get all global variables
    const std::unordered_map<std::string, VarDeclStmt*>& getGlobals() const {
        return globalLayer->variables;
    }

    // Check if a variable is global
    bool isGlobal(const std::string& name) {
        return globalLayer && globalLayer->variables.find(name) != globalLayer->variables.end();
    }
    
    ~Scope() {
        // Clean up all layers
        ScopeLayer* layer = currentLayer;
        while (layer) {
            ScopeLayer* parent = layer->parent;
            delete layer;
            layer = parent;
        }
    }
    
    void registerClass(const std::string& name, ClassDefStmt* classDef) {
        classDefinitions[name] = classDef;
    }
    
    void enterScope() {
        auto newLayer = new ScopeLayer();
        newLayer->parent = currentLayer;
        newLayer->depth = (currentLayer ? currentLayer->depth + 1 : 0);
        currentLayer = newLayer;
    }
    
    void exitScope() {
        if (currentLayer && currentLayer->parent) {
            ScopeLayer* oldLayer = currentLayer;
            currentLayer = currentLayer->parent;
            delete oldLayer;
        }
    }
    
    void declareVariable(const std::string& name, VarDeclStmt* var) {
        if (currentLayer) {
            currentLayer->variables[name] = var;
        }
    }
    
    // Resolve a member access chain: "instance.member.field"
    // Returns the VarDeclStmt of the final member, or nullptr if invalid
    VarDeclStmt* resolveMemberChain(const std::string& chain) {
        auto parts = splitChain(chain);
        
        if (parts.empty()) {
            return nullptr;
        }
        
        // First part: variable lookup
        std::string varName = parts[0];
        VarDeclStmt* var = lookupVariable(varName);
        if (!var) {
            return nullptr;  // Variable not found
        }
        
        // If only one part, return the variable itself
        if (parts.size() == 1) {
            return var;
        }
        
        // Walk the chain
        std::string currentTypeName = var->type.originalTxt;
        
        for (size_t i = 1; i < parts.size(); i++) {
            std::string memberName = parts[i];
            
            // Current type must be a class (not built-in)
            if (isBuiltinType(currentTypeName)) {
                return nullptr;  // Can't access member of built-in type
            }
            
            // Look up the class definition
            ClassDefStmt* classDef = getClassDef(currentTypeName);
            if (!classDef) {
                return nullptr;  // Class not defined
            }
            
            // Find the member in the class
            VarDeclStmt* member = findMemberInClass(classDef, memberName);
            if (!member) {
                return nullptr;  // Member not found in class
            }
            
            // Update current type for next iteration
            currentTypeName = member->type.originalTxt;
            
            // On last iteration, return the member
            if (i == parts.size() - 1) {
                return member;
            }
        }
        
        return nullptr;
    }
    
    VarDeclStmt* lookupVariable(const std::string& name) {
        ScopeLayer* layer = currentLayer;
        while (layer) {
            auto variable = layer->variables.find(name);
            if (variable != layer->variables.end()) return variable->second;
            layer = layer->parent;
        }
        return nullptr;
    }
    
    bool isVisible(const std::string& name) {
        return lookupVariable(name) != nullptr;
    }
    
    // Check if a chain is accessible: "instance.member.field"
    bool isMemberChainValid(const std::string& chain) {
        return resolveMemberChain(chain) != nullptr;
    }
};

// Index a single file's declarations, then validate member-access expressions.
inline bool checkFileScope(const std::vector<std::unique_ptr<Stmt>>& fileAST) {
    Scope scope;

    auto registerClasses = [&](auto&& self, const std::vector<std::unique_ptr<Stmt>>& statements) -> void {
        for (const auto& stmt : statements) {
            if (auto classDef = dynamic_cast<ClassDefStmt*>(stmt.get())) {
                scope.registerClass(classDef->name.originalTxt, classDef);
                self(self, classDef->body);
            } else if (auto funcDef = dynamic_cast<FuncDefStmt*>(stmt.get())) {
                self(self, funcDef->body);
            } else if (auto ifStmt = dynamic_cast<IfStmt*>(stmt.get())) {
                self(self, ifStmt->thenBody);
                self(self, ifStmt->elseBody);
            } else if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt.get())) {
                self(self, whileStmt->body);
            }
        }
    };
    registerClasses(registerClasses, fileAST);

    for (const auto& stmt : fileAST) {
        if (auto varDecl = dynamic_cast<VarDeclStmt*>(stmt.get())) {
            scope.declareGlobal(varDecl->name.originalTxt, varDecl);
        }
    }

    auto checkExpr = [&](auto&& self, const Expr* expr) -> bool {
        if (!expr) return true;
        if (auto varExpr = dynamic_cast<const VarExpr*>(expr)) {
            const std::string& name = varExpr->name.originalTxt;
            if (name.find('.') != std::string::npos) {
                return scope.isMemberChainValid(name);
            }
        } else if (auto binOp = dynamic_cast<const BinOpExpr*>(expr)) {
            return self(self, binOp->left.get()) && self(self, binOp->right.get());
        } else if (auto index = dynamic_cast<const IndexExpr*>(expr)) {
            return self(self, index->target.get()) && self(self, index->index.get());
        } else if (auto map = dynamic_cast<const AsyncMapExpr*>(expr)) {
            if (!self(self, map->source.get())) return false;
            scope.enterScope();
            VarDeclStmt iterator;
            iterator.name = map->iterator;
            iterator.type = makeToken(DYN, "dyn", map->iterator.line, map->iterator.col);
            scope.declareVariable(iterator.name.originalTxt, &iterator);
            const bool valid = self(self, map->operation.get());
            scope.exitScope();
            return valid;
        } else if (auto unOp = dynamic_cast<const UnOpExpr*>(expr)) {
            return self(self, unOp->value.get());
        } else if (auto call = dynamic_cast<const CallExpr*>(expr)) {
            for (const auto& arg : call->args) {
                if (!self(self, arg.get())) return false;
            }
        } else if (auto list = dynamic_cast<const ListExpr*>(expr)) {
            for (const auto& item : list->itms) {
                if (!self(self, item.get())) return false;
            }
        }
        return true;
    };

    auto checkStatements = [&](auto&& self, const std::vector<std::unique_ptr<Stmt>>& statements) -> bool {
        for (const auto& stmt : statements) {
            if (auto varDecl = dynamic_cast<VarDeclStmt*>(stmt.get())) {
                if (!checkExpr(checkExpr, varDecl->init.get())) return false;
                scope.declareVariable(varDecl->name.originalTxt, varDecl);
            } else if (auto funcDef = dynamic_cast<FuncDefStmt*>(stmt.get())) {
                scope.enterScope();
                std::vector<std::unique_ptr<VarDeclStmt>> parameters;
                for (const auto& param : funcDef->params) {
                    auto variable = std::make_unique<VarDeclStmt>();
                    variable->type = param.type;
                    variable->name = param.name;
                    scope.declareVariable(variable->name.originalTxt, variable.get());
                    parameters.push_back(std::move(variable));
                }
                bool valid = self(self, funcDef->body);
                scope.exitScope();
                if (!valid) return false;
            } else if (auto classDef = dynamic_cast<ClassDefStmt*>(stmt.get())) {
                scope.enterScope();
                bool valid = self(self, classDef->body);
                scope.exitScope();
                if (!valid) return false;
            } else if (auto ifStmt = dynamic_cast<IfStmt*>(stmt.get())) {
                if (!checkExpr(checkExpr, ifStmt->condition.get())) return false;
                scope.enterScope();
                bool valid = self(self, ifStmt->thenBody);
                scope.exitScope();
                if (!valid) return false;
                scope.enterScope();
                valid = self(self, ifStmt->elseBody);
                scope.exitScope();
                if (!valid) return false;
            } else if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt.get())) {
                if (!checkExpr(checkExpr, whileStmt->condition.get())) return false;
                scope.enterScope();
                bool valid = self(self, whileStmt->body);
                scope.exitScope();
                if (!valid) return false;
            } else if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt.get())) {
                if (!checkExpr(checkExpr, returnStmt->body.get())) return false;
            } else if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt.get())) {
                if (!checkExpr(checkExpr, exprStmt->expr.get())) return false;
            }
        }
        return true;
    };

    return checkStatements(checkStatements, fileAST);
}

inline bool checkFileScope(const parsedSalFile& file) {
    return checkFileScope(file.fileAST);
}