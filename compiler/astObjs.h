#pragma once

#include "SALItms.h"
#include <vector>
#include <string>
#include <memory>

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct Expr : ASTNode {};

struct NullExpr : Expr {};

struct LiteralExpr : Expr {
    SALTKN value; // Floats, strings, etc...
};

struct VarExpr : Expr {
    SALTKN name;
};

struct BinOpExpr : Expr {
    std::unique_ptr<Expr> left;
    SALTKN op;
    std::unique_ptr<Expr> right;
};

struct UnOpExpr : Expr {
    std::unique_ptr<Expr> value;
    SALTKN op;
};

struct CallExpr : Expr {
    std::vector<std::unique_ptr<Expr>> args;
    SALTKN funcName;
};

struct ListExpr : Expr {
    SALTKN type;
    std::vector<std::unique_ptr<Expr>> itms;
};

struct CastExpr : Expr {
    std::unique_ptr<Expr> value;
    SALTKN targetType;
};

struct TypeIsExpr : Expr {
    std::unique_ptr<Expr> value;
    SALTKN checkedType;
};

// Statement nodes
struct Stmt : ASTNode {};

struct NullStmt : Stmt {};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> body;
};

struct ImportStmt : Stmt {
    std::vector<SALTKN> directory;
    SALTKN alias;
};

struct BreakStmt : Stmt {};

struct ContinueStmt : Stmt {};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct VarDeclStmt : Stmt {
    std::vector<SalTknType> attributes;
    SALTKN type;
    SALTKN name;
    std::unique_ptr<Expr> init = std::make_unique<NullExpr>();  // the assigned value. Starts as null.
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> thenBody;
    std::vector<std::unique_ptr<Stmt>> elseBody;
};

struct GLSL_shader {
    // The main shader for data within while loops.
    std::vector<uint32_t> SPIRV_SHADER;

};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> body;
    GLSL_shader shader;
};

struct Parameter {
    SALTKN name;
    SALTKN type;  // "float", "int", "list", etc
    bool is_reference;  // pass by reference?
};

struct FuncDefStmt : Stmt {
    SALTKN name;
    std::vector<Parameter> params;
    SALTKN returnType;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<SalTknType> attributes;
};

struct ClassDefStmt : Stmt {
    SALTKN name;
    SALTKN inheritance;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<SalTknType> attributes;
};

struct TypeGuard {
    std::unique_ptr<Expr> value;
    SALTKN checkedType;
    bool isCondition = false;
    bool negate = false;
};

struct TypeDispatchCase {
    std::vector<TypeGuard> guards;
    std::vector<std::unique_ptr<Stmt>> statements;
    std::unique_ptr<Expr> value;
};

struct TypeDispatchExpr : Expr {
    std::vector<TypeDispatchCase> cases;
};

const std::vector<SalTknType> SAL_BINOP_MATH_OPERATORS = {
    MULTASGN,
    DIVASGN,
    ADDASGN,
    SUBASGN,
    POW,
    DIVD,
    MULT,
    MULT,
    ADD,
    SUB, // Is both a unary and binary op, just depends on when and where.
    LESS,
    GREATER,
    NEQUAL,
    LESSEQ,
    GREATEREQ,
    OR,
    AND,
    EQUAL,
    NOT,
};

const std::vector<SalTknType> SAL_UNOP_MATH_OPERATORS = {
    ROOT,
    ADDONE,
    SUBONE,
    SUB,
};

inline bool isSpec(SalTknType type) {
    switch(type) {
        case ASYNC:
        case LOCAL:
            return true;
        default:
            return false;
    }
}

inline bool isUnaryMathOperator(SalTknType type) {
    for (const auto& item : SAL_UNOP_MATH_OPERATORS) {
        if (item == type) return true;
    }
    return false;
}

inline bool isBinaryMathOperator(SalTknType type) {
    for (const auto& item : SAL_BINOP_MATH_OPERATORS) {
        if (item == type) return true;
    }
    return false;
}

inline bool isDelimiter(SalTknType type) {
    switch(type) {
        case(LPAREN):
        case(LBRACE):
        case(LBRACKET):
        case(RBRACE):
        case(RPAREN):
        case(RBRACKET):
            return true;
        default:
            return false;
    }
}