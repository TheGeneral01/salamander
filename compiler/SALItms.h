/* =================================================================================================== */
/*                                                                                                     */
/*  Module: SALItms.h                                                                                  */
/*  Description: Defines SAL tokens, operators, and source-file data structures.                       */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem; // A (basically) global alias for the filesystem.

enum SalTknType {
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COLON,
    COMMA,
    DOT,
    AT,
    BAR,

    POW,
    DIVD,
    MULT,
    ADD,
    SUB,
    ASSIGN,
    LESS,
    GREATER,
    NEQUAL,
    EQUAL,
    ROOT,
    MULTASGN,
    DIVASGN,
    ADDASGN,
    SUBASGN,
    LESSEQ,
    GREATEREQ,
    ADDONE,
    SUBONE,

    IDENTIFIER,
    INTLIT,
    FLOATLIT,
    STRLIT,
    FSTRLIT,

    IF,
    OR,
    AND,
    DEF,
    DYN,
    FOR,
    NOT,
    TRY,

    SELF,
    TRUE,
    TYPE,
    VOID,
    ELSE,
    LIST,
    INDENT,
    DEDENT,
    ASYNC,
    BREAK,
    CLASS,
    FALSE,
    LOCAL,
    RAISE,
    WHILE,
    STRING,
    RETURN,

    ACTIVE,
    EXCEPT,
    STATIC,

    IMPORT,
    FINALLY,

    CONTINUE,

    NAMESPACE,

    NEWLINE // A sub for the main indent.
};

struct SALTKN {
    SalTknType type;
    std::string originalTxt;
    int line, col, length;
    std::vector<SALTKN> subTkns;
    
    /**
     * @brief Compares if two tokens are one of the same.
     * @param other The other token to compare to.
     * @details We only compare the literal file text, since if they are the same then 99% of the time then they are the same thing.
     * In the cases where it is nested classes, we do not check using this for those, therefore this covers all cases where needed.
     */
    bool operator==(const SALTKN& other) {
        return this->originalTxt == other.originalTxt;
    }
};

SALTKN makeToken(SalTknType type, std::string text, int line, int col, int length = -1) {
    SALTKN returnTkn;
    returnTkn.col = col;
    if (length == -1) {returnTkn.length = text.size();} else {returnTkn.length = length;}
    returnTkn.line = line;
    returnTkn.originalTxt = text;
    returnTkn.type = type;
    return returnTkn;
}

struct RawSalFile {
    std::vector<SALTKN> lexedFile; // A flat stream of tokens to feed into the AST.
    fs::path filepath;
};

const std::unordered_map<std::string, SalTknType> SalOperators = {
    {"!=", SalTknType::NEQUAL},
    {"!/", SalTknType::ROOT},
    {"*/", SalTknType::MULTASGN},
    {"/=", SalTknType::DIVASGN},
    {"+=", SalTknType::ADDASGN},
    {"-=", SalTknType::SUBASGN},
    {"<=", SalTknType::LESSEQ},
    {">=", SalTknType::GREATEREQ},
    {"++", SalTknType::ADDONE},
    {"--", SalTknType::SUBONE},
    {"^", SalTknType::POW},
    {"+", SalTknType::ADD},
    {"-", SalTknType::SUB},
    {"*", SalTknType::MULT},
    {"/", SalTknType::DIVD},
    {"=", SalTknType::ASSIGN},
    {"<", SalTknType::LESS},
    {">", SalTknType::GREATER},
    {"==", SalTknType::EQUAL}
};

const std::unordered_map<std::string, SalTknType> SalDelimiters = {
    {";", SalTknType::NEWLINE}, // TODO
    {"(", SalTknType::LPAREN}, // TODO
    {")", SalTknType::RPAREN}, // TODO
    {"{", SalTknType::LBRACE}, // TODO
    {"}", SalTknType::RBRACE}, // TODO
    {"[", SalTknType::LBRACKET}, // TODO
    {"]", SalTknType::RBRACKET}, // TODO
    {":", SalTknType::COLON}, // TODO
    {",", SalTknType::COMMA}, // TODO
    {".", SalTknType::DOT}, // TODO
    {"@", SalTknType::AT}, // TODO
    {"|", SalTknType::BAR}, // TODO
};

const std::unordered_map<std::string, SalTknType> SAL_ONE_CHAR_OBJECTS = {
    {"^", SalTknType::POW}, // TODO
    {"/", SalTknType::DIVD}, // TODO
    {"*", SalTknType::MULT}, // TODO
    {"+", SalTknType::ADD}, // TODO
    {"-", SalTknType::SUB}, // TODO
    {"=", SalTknType::ASSIGN}, // TODO
    {"<", SalTknType::LESS}, // TODO
    {">", SalTknType::GREATER}, // TODO

    {";", SalTknType::NEWLINE}, // TODO
    {"(", SalTknType::LPAREN}, // TODO
    {")", SalTknType::RPAREN}, // TODO
    {"{", SalTknType::LBRACE}, // TODO
    {"}", SalTknType::RBRACE}, // TODO
    {"[", SalTknType::LBRACKET}, // TODO
    {"]", SalTknType::RBRACKET}, // TODO
    {":", SalTknType::COLON}, // TODO
    {",", SalTknType::COMMA}, // TODO
    {".", SalTknType::DOT}, // TODO
    {"@", SalTknType::AT}, // TODO
    {"|", SalTknType::BAR} // TODO
};

const std::unordered_map<std::string, SalTknType> SAL_TWO_CHAR_OBJECTS = {
    {"!=", SalTknType::NEQUAL}, // TODO
    {"!/", SalTknType::ROOT}, // TODO
    {"*=", SalTknType::MULTASGN}, // TODO
    {"/=", SalTknType::DIVASGN}, // TODO
    {"+=", SalTknType::ADDASGN}, // TODO
    {"-=", SalTknType::SUBASGN}, // TODO
    {"<=", SalTknType::LESSEQ}, // TODO
    {">=", SalTknType::GREATEREQ}, // TODO
    {"or", SalTknType::OR}, // TODO
    {"++", SalTknType::ADDONE},// TODO
    {"--", SalTknType::SUBONE}, // TODO
    {"if", SalTknType::IF},
    {"==", SalTknType::EQUAL},
};

const std::unordered_map<std::string, SalTknType> SAL_THREE_CHAR_OBJECTS = {
    {"and", SalTknType::AND},
    {"def", SalTknType::DEF},
    {"dyn", SalTknType::DYN},
    {"for", SalTknType::FOR},
    {"int", SalTknType::TYPE},
    {"not", SalTknType::NOT},
    {"try", SalTknType::TRY},
    {"str", SalTknType::TYPE},
    // #TODO: ADD PACK FOLD EXPRESSIONS
    // #TODO: ADD NEW T_KEYWORD?
};

const std::unordered_map<std::string, SalTknType> SAL_FOUR_CHAR_OBJECTS = {
    {"self", SalTknType::SELF},
    {"true", SalTknType::TRUE},
    {"type", SalTknType::TYPE},
    {"void", SalTknType::VOID},
    {"else", SalTknType::ELSE},
};

const std::unordered_map<std::string, SalTknType> SAL_FIVE_CHAR_OBJECTS = {
    {"async", SalTknType::ASYNC},
    {"break", SalTknType::BREAK},
    {"class", SalTknType::CLASS},
    {"false", SalTknType::FALSE},
    {"float", SalTknType::TYPE},
    {"local", SalTknType::LOCAL},
    {"raise", SalTknType::RAISE},
    {"while", SalTknType::WHILE},
};

const std::unordered_map<std::string, SalTknType> SAL_SIX_CHAR_OBJECTS = {
    {"active", SalTknType::ACTIVE},
    {"except", SalTknType::EXCEPT},
    {"static", SalTknType::STATIC},
    {"return", SalTknType::RETURN},
    {"import", SalTknType::IMPORT},
};

const std::unordered_map<std::string, SalTknType>  SAL_SEVEN_CHAR_OBJECTS = {
    {"finally", SalTknType::FINALLY},
};

const std::unordered_map<std::string, SalTknType> SAL_EIGHT_CHAR_OBJECTS = {
    {"continue", SalTknType::CONTINUE},
};

const std::unordered_map<std::string, SalTknType> SAL_NINE_CHAR_OBJECTS = {
    {"namespace", SalTknType::NAMESPACE},
};

const int largest_operator_size = 2; // The side of the largest operator.
// ++ = 2, + = 1. Gets us O(N) Efficiency instead of larger.

// Lets us sort through the list based on the index.
const std::vector<std::unordered_map<std::string, SalTknType>> lists_by_size = {
    SAL_ONE_CHAR_OBJECTS,
    SAL_TWO_CHAR_OBJECTS,
    SAL_THREE_CHAR_OBJECTS,
    SAL_FOUR_CHAR_OBJECTS,
    SAL_FIVE_CHAR_OBJECTS,
    SAL_SIX_CHAR_OBJECTS,
    SAL_SEVEN_CHAR_OBJECTS,
    SAL_EIGHT_CHAR_OBJECTS,
    SAL_NINE_CHAR_OBJECTS,
};