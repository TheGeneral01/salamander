/* =================================================================================================== */
/*                                                                                                     */
/*  Module: parser.h                                                                                   */
/*  Description: Parses the given token stream into a valid AST.                                       */
/*  Date Last Updated: 9/23/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "SALItms.h"
#include "astObjs.h"
#include <iostream>
#include <vector>
#include <string>

bool isOpeningDelimiter(SalTknType type) {
    return type == LPAREN || type == LBRACE || type == LBRACKET;
}

bool isClosingDelimiter(SalTknType type) {
    return type == RPAREN || type == RBRACE || type == RBRACKET;
}

bool isBlockMarker(SalTknType type) {
    return type == INDENT || type == DEDENT;
}

class parser {
    public:

    std::vector<SALTKN> fixTkns(std::vector<SALTKN> tokens) {
        // A pass to affix all tokens with identifiers together.
        // We need to merge all tokens like my.token.thing into a single
        // identifier.
        std::vector<SALTKN> result = {};
        std::size_t i = 0;

        while (i<tokens.size()) {
            if (tokens[i].type == IDENTIFIER) {
                SALTKN combined = tokens[i];
                i++;
                
                while (i+1 < tokens.size()
                && tokens[i].type == DOT &&
                tokens[i+1].type == IDENTIFIER)
                {
                    combined.originalTxt += ".";
                    combined.originalTxt += tokens[i+1].originalTxt;
                    i += 2;
                }

                result.push_back(combined);
            } else {
                result.push_back(tokens[i]);
                i++;
            }
        }

        return result;
    }

    public:
    parser(const std::vector<SALTKN>& tokens) :
    tokens(fixTkns(tokens)), pos(0) {}

    std::vector<std::unique_ptr<Stmt>> parse() {
        std::vector<std::unique_ptr<Stmt>> stmts;
        while (!isAtEnd()) {
            auto stmt = parseStmt();
            if (stmt) stmts.push_back(std::move(stmt));
        }
        return stmts;
    }

    private:
    std::vector<std::unique_ptr<ImportStmt>> includedFiles;
    std::vector<SALTKN> tokens;
    int pos = 0;

    const SALTKN& cur() { return tokens[pos]; }
    const SALTKN& prev() { return tokens[pos-1]; }
    const SALTKN& peek(std::size_t offset = 0) const { return tokens[pos + offset]; }
    SALTKN adv() { return tokens[pos++]; }
    bool chk(SalTknType type) { return !isAtEnd() && cur().type == type;}
    bool chkNext(SalTknType type, size_t offset = 1) const {
        return pos + offset < tokens.size() && tokens[pos + offset].type == type;
    }
    bool mtch(SalTknType type) {
        if (chk(type)) { adv(); return true; }
        return false;
    }
    bool isAtEnd() const { return pos >= tokens.size(); }
    void skip(SalTknType type) { if (chk(type)) adv(); }

    void skipIndent() {
        while (!isAtEnd() && (chk(INDENT) || chk(DEDENT))) adv();
    }

    bool hasDot(const std::string& s) const {
        return s.find('.') != std::string::npos;
    }

    /**
     * @brief Parses the main statement expressions from the tokens.
     * @details Gets the given functions from other function nodes.
     */
    std::unique_ptr<Stmt> parseStmt() {
        skipIndent();
        if (isAtEnd()) return nullptr;
        if (chk(NEWLINE)) {
            adv();
            return nullptr;
        }

        std::vector<SALTKN> specs;
        while (!isAtEnd() && isSpec(cur().type)) {
            specs.push_back(adv());
        }

        if (!specs.empty()) {
            if (mtch(CLASS)) {
                auto cls = parseClassDecl();
                for (const auto& spec : specs) cls->attributes.push_back(spec.type);
                return cls;
            }

            if (mtch(DEF)) {
                auto func = parseFuncDef();
                for (const auto& spec : specs) func->attributes.push_back(spec.type);
                return func;
            }

            auto decl = parseVarDecl();
            for (const auto& spec : specs) decl->attributes.push_back(spec.type);
            return decl;
        }

        if (mtch(IMPORT)) return parseImportStmt();
        if (mtch(DEF)) return parseFuncDef();
        if (mtch(CLASS)) return parseClassDecl();
        if (mtch(IF)) return parseIf();
        if (mtch(WHILE)) return parseWhile();
        if (mtch(RETURN)) return parseReturnStmt();
        if (mtch(BREAK)) return parseBreak();
        if (mtch(CONTINUE)) return parseContinue();

        // Function Calls
        if (chk(IDENTIFIER) && chkNext(LPAREN)) {
            auto expr = parseExpr();
            if (!expr) return nullptr;
            skip(NEWLINE);
            auto stmt = std::make_unique<ExprStmt>();
            stmt->expr = std::move(expr);
            return stmt;
        }

        // Member reassignment or access.
        if (chk(IDENTIFIER) && hasDot(cur().originalTxt)) {
            auto expr = parseExpr();
            if (!expr) return nullptr;
            skip(NEWLINE);
            auto stmt = std::make_unique<ExprStmt>();
            stmt->expr = std::move(expr);
            return stmt;
        }

        // Type declaration via types or a custom type.
        if ((chk(TYPE) || chk(IDENTIFIER)) && chkNext(IDENTIFIER)) {
            return parseVarDecl();
        }

        // Dynamic declarations.
        if (chk(DYN)) {
            return parseVarDecl();
        }

        // General reassignments.
        auto expr = parseExpr();
        if (!expr) return nullptr;
        skip(NEWLINE);
        auto stmt = std::make_unique<ExprStmt>();
        stmt->expr = std::move(expr);
        return stmt;
    }

    std::unique_ptr<ReturnStmt> parseReturnStmt() {
        auto returnStmt = std::make_unique<ReturnStmt>();
        returnStmt->body = parseExpr();
        return returnStmt;
    }

    std::unique_ptr<BreakStmt> parseBreak() {
        return std::make_unique<BreakStmt>();
    }

    std::unique_ptr<ContinueStmt> parseContinue() {
        return std::make_unique<ContinueStmt>();
    }

    /**
     * @brief Parses a class declaration.
     * @details Parses a class into a returned ClassDefStmt given the current position of pos.
     */
    std::unique_ptr<ClassDefStmt> parseClassDecl() {
        auto classStmt = std::make_unique<ClassDefStmt>();

        // First we expect an identifier for the class name.
        classStmt->name = adv();

        // Check for inheritance.
        if (mtch(LPAREN)) {
            // Should be an identifier.
            classStmt->inheritance = adv();
            mtch(RPAREN);
        }
        mtch(COLON); // Skip past the :
        mtch(INDENT);

        // Now we are in the body.
        while (!chk(DEDENT) && !isAtEnd()) {
            auto statement = parseStmt();
            if (statement) classStmt->body.push_back(std::move(statement));
        }
        mtch(DEDENT);

        return classStmt;
    }

    /**
     * @brief Parses a given function
     * @details Emits a function stmt when given a function declaration based on the current position
     * that we are looking at our tokens in our list.
     */
    std::unique_ptr<FuncDefStmt> parseFuncDef() {
        auto funcStmt = std::make_unique<FuncDefStmt>();

        // First get the type.
        funcStmt->returnType = adv();

        // Now get the name.
        funcStmt->name = adv();

        mtch(LPAREN);
        while (!mtch(RPAREN)) {
            Parameter param;
            param.type = adv();
            param.name = adv();
            funcStmt->params.push_back(param);
            mtch(COMMA); // Skip the comma.
        }
        mtch(COLON);
        mtch(INDENT);

        while (!chk(DEDENT) && !isAtEnd()) {
            auto statement = parseStmt();
            if (statement) funcStmt->body.push_back(std::move(statement));
        }
        mtch(DEDENT);

        return funcStmt;
    }

    // Get the order needed for pratt parsing.
    int getPrecedence(SalTknType type) {
        switch (type) {
            case ASSIGN: return 0;
            case ADD: case SUB: return 1;
            case MULT: case DIVD: return 2;
            case POW: return 3;
            default: return 0;
        }
    }

    std::unique_ptr<Expr> parseExpr() {
        return parsePrecedence(0);
    }

    std::unique_ptr<Expr> parsePrecedence(int minPrec) {
        auto left = parseUnary();

        while (!isAtEnd() && (isBinaryMathOperator(cur().type) || chk(ASSIGN))) {
            int prec = getPrecedence(cur().type);
            if (prec < minPrec) break;

            auto op = adv();
            auto right = parseUnary();

            auto binop = std::make_unique<BinOpExpr>();
            binop->left = std::move(left);
            binop->op = op;
            binop->right = std::move(right);
            left = std::move(binop);
        }

        return left;
    }

    std::unique_ptr<Expr> parseUnary() {
        if (isUnaryMathOperator(cur().type)) {
            auto op = adv();
            auto value = parsePrimary();
            auto unop = std::make_unique<UnOpExpr>();
            unop->op = op;
            unop->value = std::move(value);
            return unop;
        }
        return parsePrimary();
    }

    std::unique_ptr<Expr> parsePrimary() {
        skipIndent();

        // Parse literal expressions.                      Special cases for FSTRLIT are handled in scope checking and codegen.
        if (chk(INTLIT) || chk(FLOATLIT) || chk(STRLIT) || chk(FSTRLIT) ||
            chk(TRUE) || chk(FALSE)) {
            auto lit = std::make_unique<LiteralExpr>();
            lit->value = adv();
            return lit;
        }

        // Parse function calls.
        if (chk(IDENTIFIER) && chkNext(LPAREN)) {
            auto func = std::make_unique<CallExpr>();
            func->funcName = adv();
            mtch(LPAREN);
            if (!chk(RPAREN)) {
                while (!isAtEnd()) {
                    func->args.push_back(parseExpr());
                    if (!mtch(COMMA)) break;
                }
            }
            mtch(RPAREN);
            return func;
        }

        // Parse lists.
        if (chk(LBRACKET)) {
            auto list = std::make_unique<ListExpr>();
            adv();
            while (!isAtEnd() && !chk(RBRACKET)) {
                if (chk(COMMA)) { adv(); continue; }
                list->itms.push_back(parseExpr());
                if (chk(COMMA)) adv();
            }
            mtch(RBRACKET);
            return list;
        }

        // Parse variable declarations.
        if (chk(IDENTIFIER)) {
            auto var = std::make_unique<VarExpr>();
            var->name = adv();
            return var;
        }

        // Parse general expressions for ops and whatnot.
        if (chk(LPAREN)) {
            adv();
            auto expr = parseExpr();
            mtch(RPAREN);
            return expr;
        }

        // Parse general expressions for ops and whatnot.
        if (chk(LBRACE)) {
            adv();
            auto expr = parseExpr();
            mtch(RBRACE);
            return expr;
        }

        throw std::runtime_error("Unexpected token '" + cur().originalTxt +
        "' at line " + std::to_string(cur().line + 1) +
        ", col " + std::to_string(cur().col + 1));
    }

    std::unique_ptr<VarDeclStmt> parseVarDecl() {
        auto decl = std::make_unique<VarDeclStmt>();

        if (chk(DYN) || chk(TYPE) || chk(IDENTIFIER)) {
            decl->type = adv();
        }

        if (!chk(IDENTIFIER)) {
            throw std::runtime_error("Expected variable name at: "
            + std::to_string(cur().line) + std::string(":") + std::to_string(cur().col));
        }

        SALTKN nameTkn = adv();
        nameTkn.type = IDENTIFIER;

        if (mtch(ASSIGN)) {
            decl->init = parseExpr();
        }

        decl->name = nameTkn;
        skip(NEWLINE);
        return decl;
    }

    std::unique_ptr<ImportStmt> parseImportStmt() {
        auto importStatement = std::make_unique<ImportStmt>();
        importStatement->directory.push_back(adv());
        includedFiles.push_back(std::move(importStatement));
        return importStatement;
    }

    std::unique_ptr<IfStmt> parseIf() {
        auto condition = parseExpr();
        mtch(COLON);
        mtch(INDENT);

        std::vector<std::unique_ptr<Stmt>> thenBody;
        while (!chk(DEDENT) && !isAtEnd()) {
            auto stmt = parseStmt();
            if (stmt) thenBody.push_back(std::move(stmt));
        }
        mtch(DEDENT);

        std::vector<std::unique_ptr<Stmt>> elseBody;
        if (mtch(ELSE)) {
            mtch(COLON);
            mtch(INDENT);
            while (!chk(DEDENT) && !isAtEnd()) {
                auto stmt = parseStmt();
                if (stmt) elseBody.push_back(std::move(stmt));
            }
            mtch(DEDENT);
        }

        auto ifStmt = std::make_unique<IfStmt>();
        ifStmt->condition = std::move(condition);
        ifStmt->thenBody = std::move(thenBody);
        ifStmt->elseBody = std::move(elseBody);
        return ifStmt;
    }

    std::unique_ptr<WhileStmt> parseWhile() {
        auto condition = parseExpr();
        mtch(COLON);
        mtch(INDENT);

        std::vector<std::unique_ptr<Stmt>> body;
        while (!chk(DEDENT) && !isAtEnd()) {
            auto stmt = parseStmt();
            if (stmt) body.push_back(std::move(stmt));
        }
        mtch(DEDENT);

        auto whileStmt = std::make_unique<WhileStmt>();
        whileStmt->condition = std::move(condition);
        whileStmt->body = std::move(body);
        return whileStmt;
    }

    public:
    void printBreak(const BreakStmt* stmt, int indent = 0) {
        (void)stmt;
        std::cout << std::string(indent * 2, ' ') << "BreakStmt" << std::endl;
    }

    void printContinue(const ContinueStmt* stmt, int indent = 0) {
        (void)stmt;
        std::cout << std::string(indent * 2, ' ') << "ContinueStmt" << std::endl;
    }

    void printAst(const std::unique_ptr<Stmt>& stmt, int indent = 0) {
        if (!stmt) return;
        std::string p(indent * 2, ' ');

        if (auto v = dynamic_cast<VarDeclStmt*>(stmt.get())) {
            std::cout << p << "VarDecl: " << v->name.originalTxt << " (" << v->type.originalTxt << ")" << std::endl;
            if (v->init) printExpr(v->init, indent + 1);
        } else if (auto i = dynamic_cast<IfStmt*>(stmt.get())) {
            std::cout << p << "If" << std::endl;
            std::cout << p << "  Condition:" << std::endl;
            printExpr(i->condition, indent + 2);
            std::cout << p << "  Then:" << std::endl;
            for (auto& s : i->thenBody) printAst(s, indent + 2);
            if (!i->elseBody.empty()) {
                std::cout << p << "  Else:" << std::endl;
                for (auto& s : i->elseBody) printAst(s, indent + 2);
            }
        } else if (auto w = dynamic_cast<WhileStmt*>(stmt.get())) {
            std::cout << p << "While" << std::endl;
            std::cout << p << "  Condition:" << std::endl;
            printExpr(w->condition, indent + 2);
            std::cout << p << "  Body:" << std::endl;
            for (auto& s : w->body) printAst(s, indent + 2);
        } else if (auto f = dynamic_cast<FuncDefStmt*>(stmt.get())) {
            std::cout << p << "FuncDef: " << f->name.originalTxt << std::endl;
            for (auto& pr : f->params)
                std::cout << p << "  Param: " << pr.name.originalTxt << " (" << pr.type.originalTxt << ")" << std::endl;
            std::cout << p << "  Body:" << std::endl;
            for (auto& s : f->body) printAst(s, indent + 2);
        } else if (auto c = dynamic_cast<ClassDefStmt*>(stmt.get())) {
            std::cout << p << "ClassDef: " << c->name.originalTxt << std::endl;
            for (auto& b : c->body) printAst(b, indent + 1);
        } else if (auto r = dynamic_cast<ReturnStmt*>(stmt.get())) {
            std::cout << p << "ReturnStmt:" << std::endl;
            if (r->body) printExpr(r->body, indent + 1);
        } else if (auto b = dynamic_cast<BreakStmt*>(stmt.get())) {
            printBreak(b, indent);
        } else if (auto c = dynamic_cast<ContinueStmt*>(stmt.get())) {
            printContinue(c, indent);
        } else if (auto e = dynamic_cast<ExprStmt*>(stmt.get())) {
            std::cout << p << "ExprStmt:" << std::endl;
            printExpr(e->expr, indent + 1);
        }
    }

    void printExpr(const std::unique_ptr<Expr>& expr, int indent = 0) {
        if (!expr) return;
        std::string p(indent * 2, ' ');

        if (auto l = dynamic_cast<LiteralExpr*>(expr.get())) {
            std::cout << p << "Literal: " << l->value.originalTxt << std::endl;
        } else if (auto v = dynamic_cast<VarExpr*>(expr.get())) {
            std::cout << p << "Var: " << v->name.originalTxt << std::endl;
        } else if (auto list = dynamic_cast<ListExpr*>(expr.get())) {
            std::cout << p << "List:" << std::endl;
            for (const auto& item : list->itms) printExpr(item, indent + 1);
        } else if (auto b = dynamic_cast<BinOpExpr*>(expr.get())) {
            std::cout << p << "BinOp: " << b->op.originalTxt << std::endl;
            std::cout << p << "  Left:" << std::endl;
            printExpr(b->left, indent + 2);
            std::cout << p << "  Right:" << std::endl;
            printExpr(b->right, indent + 2);
        } else if (auto u = dynamic_cast<UnOpExpr*>(expr.get())) {
            std::cout << p << "UnOp: " << u->op.originalTxt << std::endl;
            printExpr(u->value, indent + 1);
        } else if (auto c = dynamic_cast<CallExpr*>(expr.get())) {
            std::cout << p << "Call: " << c->funcName.originalTxt << std::endl;
            for (const auto& item : c->args) printExpr(item, indent + 1);
        }
    }

    std::vector<std::string> getIncludeFiles() {
        std::vector<std::string> filenames;
        for (const auto& item : includedFiles) {
            if (item->directory.empty()) continue;
            std::string filename = item->directory[0].originalTxt;
            for (char& c : filename) if (c == '.') c = '/';
            filename += ".sal";
            filenames.push_back(filename);
        }
        return filenames;
    }
};