/* =================================================================================================== */
/*                                                                                                     */
/*  Module: printTkn.h                                                                                 */
/*  Description: Prints out a token stream.                                                            */
/*  Date Last Updated: 9/21/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01 && Gemini 3.6 Flash                                         */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*  AI_USAGE = true, since I don't want to manually type this out.                                     */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include <iostream>
#include <vector>
#include <string_view>
#include "SALItms.h"

inline constexpr std::string_view salTknTypeToString(SalTknType type) {
    switch (type) {
        // Delimiters & Single-char symbols
        case SalTknType::LPAREN:     return "LPAREN";
        case SalTknType::RPAREN:     return "RPAREN";
        case SalTknType::LBRACE:     return "LBRACE";
        case SalTknType::RBRACE:     return "RBRACE";
        case SalTknType::LBRACKET:   return "LBRACKET";
        case SalTknType::RBRACKET:   return "RBRACKET";
        case SalTknType::COLON:      return "COLON";
        case SalTknType::COMMA:      return "COMMA";
        case SalTknType::DOT:        return "DOT";
        case SalTknType::AT:         return "AT";
        case SalTknType::BAR:        return "BAR";

        // Operators & Compound Operators
        case SalTknType::POW:        return "POW";
        case SalTknType::DIVD:       return "DIVD";
        case SalTknType::MULT:       return "MULT";
        case SalTknType::ADD:        return "ADD";
        case SalTknType::SUB:        return "SUB";
        case SalTknType::ASSIGN:     return "ASSIGN";
        case SalTknType::LESS:       return "LESS";
        case SalTknType::GREATER:    return "GREATER";
        case SalTknType::NEQUAL:     return "NEQUAL";
        case SalTknType::EQUAL:      return "EQUAL";
        case SalTknType::ROOT:       return "ROOT";
        case SalTknType::MULTASGN:   return "MULTASGN";
        case SalTknType::DIVASGN:    return "DIVASGN";
        case SalTknType::ADDASGN:    return "ADDASGN";
        case SalTknType::SUBASGN:    return "SUBASGN";
        case SalTknType::LESSEQ:     return "LESSEQ";
        case SalTknType::GREATEREQ:  return "GREATEREQ";
        case SalTknType::ADDONE:     return "ADDONE";
        case SalTknType::SUBONE:     return "SUBONE";

        // Literals & Identifiers
        case SalTknType::IDENTIFIER: return "IDENTIFIER";
        case SalTknType::INTLIT:     return "INTLIT";
        case SalTknType::FLOATLIT:   return "FLOATLIT";
        case SalTknType::STRLIT:     return "STRLIT";
        case SalTknType::FSTRLIT:    return "FSTRLIT";

        // Keywords
        case SalTknType::IF:         return "IF";
        case SalTknType::OR:         return "OR";
        case SalTknType::AND:        return "AND";
        case SalTknType::DEF:        return "DEF";
        case SalTknType::DYN:        return "DYN";
        case SalTknType::FOR:        return "FOR";
        case SalTknType::NOT:        return "NOT";
        case SalTknType::TRY:        return "TRY";
        case SalTknType::SELF:       return "SELF";
        case SalTknType::TRUE:       return "TRUE";
        case SalTknType::TYPE:       return "TYPE";
        case SalTknType::VOID:       return "VOID";
        case SalTknType::ELSE:       return "ELSE";
        case SalTknType::LIST:       return "LIST";
        case SalTknType::INDENT:     return "INDENT";
        case SalTknType::DEDENT:     return "DEDENT";
        case SalTknType::ASYNC:      return "ASYNC";
        case SalTknType::BREAK:      return "BREAK";
        case SalTknType::CLASS:      return "CLASS";
        case SalTknType::FALSE:      return "FALSE";
        case SalTknType::LOCAL:      return "LOCAL";
        case SalTknType::RAISE:      return "RAISE";
        case SalTknType::WHILE:      return "WHILE";
        case SalTknType::STRING:     return "STRING";
        case SalTknType::RETURN:     return "RETURN";
        case SalTknType::ACTIVE:     return "ACTIVE";
        case SalTknType::EXCEPT:     return "EXCEPT";
        case SalTknType::STATIC:     return "STATIC";
        case SalTknType::IMPORT:     return "IMPORT";
        case SalTknType::FINALLY:    return "FINALLY";
        case SalTknType::CONTINUE:   return "CONTINUE";
        case SalTknType::NAMESPACE:  return "NAMESPACE";
        case SalTknType::NEWLINE:    return "NEWLINE";
    }
    return "UNKNOWN";
}

inline void printLexer(const std::vector<SALTKN>& tokens) {
    std::cout << "=================== LEXER TOKENS (" << tokens.size() << ") ===================\n";
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& tkn = tokens[i];
        std::cout << "[" << i << "] "
                  << "Type: " << salTknTypeToString(tkn.type)
                  << " | Text: '" << tkn.originalTxt << "'"
                  << " | Loc: " << tkn.line+1 << ":" << tkn.col
                  << " | Len: " << tkn.length << "\n";
    }
    std::cout << "=========================================================\n";
}