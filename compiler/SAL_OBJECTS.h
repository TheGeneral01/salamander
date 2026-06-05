#pragma once
#include <vector>
#include <string>

enum token_types {
    // Identifiers
    T_LINK,
    T_PYTHON,
    T_ASSEMBLY,
    T_INIT,

    // Single char symbols: (DELIMITERS)
    T_LPAREN,
    T_RPAREN,
    T_LBRACE,
    T_RBRACE,
    T_LBRACKET,
    T_RBRACKET,
    T_COLON,
    T_COMMA,
    T_DOT,
    T_AT,
    T_BAR,

    // OPERATORS:
    T_POWER,
    T_DIV,
    T_MULT,
    T_ADD,
    T_SUB,
    T_ASSIGN,
    T_LESS,
    T_GREATER,
    T_DESTRUCT,
    // 2 char operators:
    T_NOT_EQUAL,
    T_ROOT,
    T_MULT_ASSIGN,
    T_DIV_ASSIGN,
    T_ADD_ASSIGN,
    T_SUB_ASSIGN,
    T_LESS_EQUAL,
    T_GREATER_EQUAL,
    T_ADD_ONE,
    T_SUB_ONE,
    T_IS_EQUAL,

    // Literals:
    T_IDENTIFIER,
    T_INT_LITERAL,
    T_FLOAT_LITERAL,
    T_STRING_LITERAL,

    // Keywords:
    T_IF,
    T_OR,
    T_AND,
    T_DEF,
    T_DYN,
    T_FOR,
    T_NOT,
    T_TRY,

    T_SELF,
    T_TRUE,
    T_BOOL,
    T_VOID,
    T_ELSE,
    T_INDENT,
    T_DEDENT,
    T_ASYNC,
    T_BREAK,
    T_CLASS,
    T_CONST,
    T_FALSE,
    T_LOCAL,
    T_RAISE,
    T_WHILE,
    T_RETURN,

    T_EXCEPT,
    T_STRUCT,

    T_INCLUDE,
    T_FINALLY,

    T_CONTINUE,
    T_OVERRIDE,

    T_NAMESPACE,
    T_PROTECTED,

    // Parser token things, not actually represented in the file.
    T_NEWLINE, // To separate statements
    T_ERROR,
};

struct SAL_TOKEN {
    token_types token_type;
    std::string original_text;
    int line, col, length;
};

struct SAL_FILE {
    std::string filename;
    std::vector<std::vector<SAL_TOKEN>> lexed_file;
    __int64 lexed_filesize; // The size of the lexed file in bytes. (enough)
    std::vector<SAL_FILE*> linked_files;
    std::vector<SAL_TOKEN> tokens;
};

const std::vector<std::pair<std::string, token_types>> SAL_OPERATORS = {
    {"!=", token_types::T_NOT_EQUAL}, // TODO
    {"!/", token_types::T_ROOT}, // TODO
    {"*=", token_types::T_MULT_ASSIGN}, // TODO
    {"/=", token_types::T_DIV_ASSIGN}, // TODO
    {"+=", token_types::T_ADD_ASSIGN}, // TODO
    {"-=", token_types::T_SUB_ASSIGN}, // TODO
    {"<=", token_types::T_LESS_EQUAL}, // TODO
    {">=", token_types::T_GREATER_EQUAL}, // TODO
    {"++", token_types::T_ADD_ONE},// TODO
    {"--", token_types::T_SUB_ONE}, // TODO
    {"!", token_types::T_POWER}, // TODO
    {"/", token_types::T_DIV}, // TODO
    {"*", token_types::T_MULT}, // TODO
    {"+", token_types::T_ADD}, // TODO
    {"-", token_types::T_SUB}, // TODO
    {"=", token_types::T_ASSIGN}, // TODO
    {"<", token_types::T_LESS}, // TODO
    {">", token_types::T_GREATER}, // TODO
    {"~", token_types::T_DESTRUCT}, // TODO
    {"==", token_types::T_IS_EQUAL}, // TODO
};

const std::vector<std::pair<std::string, token_types>> SAL_DELIMITERS = {
    {";", token_types::T_NEWLINE}, // TODO
    {"(", token_types::T_LPAREN}, // TODO
    {")", token_types::T_RPAREN}, // TODO
    {"{", token_types::T_LBRACE}, // TODO
    {"}", token_types::T_RBRACE}, // TODO
    {"[", token_types::T_LBRACKET}, // TODO
    {"]", token_types::T_RBRACKET}, // TODO
    {":", token_types::T_COLON}, // TODO
    {",", token_types::T_COMMA}, // TODO
    {".", token_types::T_DOT}, // TODO
    {"@", token_types::T_AT}, // TODO
    {"|", token_types::T_BAR}, // TODO
};

const std::vector<std::pair<std::string, token_types>> SAL_ONE_CHAR_OBJECTS = {
    {"!", token_types::T_POWER}, // TODO
    {"/", token_types::T_DIV}, // TODO
    {"*", token_types::T_MULT}, // TODO
    {"+", token_types::T_ADD}, // TODO
    {"-", token_types::T_SUB}, // TODO
    {"=", token_types::T_ASSIGN}, // TODO
    {"<", token_types::T_LESS}, // TODO
    {">", token_types::T_GREATER}, // TODO
    {"~", token_types::T_DESTRUCT}, // TODO

    {";", token_types::T_NEWLINE}, // TODO
    {"(", token_types::T_LPAREN}, // TODO
    {")", token_types::T_RPAREN}, // TODO
    {"{", token_types::T_LBRACE}, // TODO
    {"}", token_types::T_RBRACE}, // TODO
    {"[", token_types::T_LBRACKET}, // TODO
    {"]", token_types::T_RBRACKET}, // TODO
    {":", token_types::T_COLON}, // TODO
    {",", token_types::T_COMMA}, // TODO
    {".", token_types::T_DOT}, // TODO
    {"@", token_types::T_AT}, // TODO
    {"|", token_types::T_BAR} // TODO
};

const std::vector<std::pair<std::string, token_types>> SAL_TWO_CHAR_OBJECTS = {
    {"!=", token_types::T_NOT_EQUAL}, // TODO
    {"!/", token_types::T_ROOT}, // TODO
    {"*=", token_types::T_MULT_ASSIGN}, // TODO
    {"/=", token_types::T_DIV_ASSIGN}, // TODO
    {"+=", token_types::T_ADD_ASSIGN}, // TODO
    {"-=", token_types::T_SUB_ASSIGN}, // TODO
    {"<=", token_types::T_LESS_EQUAL}, // TODO
    {">=", token_types::T_GREATER_EQUAL}, // TODO
    {"or", token_types::T_OR}, // TODO
    {"++", token_types::T_ADD_ONE},// TODO
    {"--", token_types::T_SUB_ONE}, // TODO
    {"if", token_types::T_IF},
    {"==", token_types::T_IS_EQUAL},
};

const std::vector<std::pair<std::string, token_types>> SAL_THREE_CHAR_OBJECTS = {
    {"and", token_types::T_AND},
    {"def", token_types::T_DEF},
    {"dyn", token_types::T_DYN},
    {"for", token_types::T_FOR},
    {"int", token_types::T_INT_LITERAL},
    {"not", token_types::T_NOT},
    {"try", token_types::T_TRY},
    // #TODO: ADD PACK FOLD EXPRESSIONS
    // #TODO: ADD NEW T_KEYWORD?
};

const std::vector<std::pair<std::string, token_types>> SAL_FOUR_CHAR_OBJECTS = {
    {"link", token_types::T_LINK},
    {"self", token_types::T_SELF},
    {"true", token_types::T_TRUE},
    {"void", token_types::T_VOID},
    {"else", token_types::T_ELSE},
    {"bool", token_types::T_BOOL},
};

const std::vector<std::pair<std::string, token_types>> SAL_FIVE_CHAR_OBJECTS = {
    {"async", token_types::T_ASYNC},
    {"break", token_types::T_BREAK},
    {"class", token_types::T_CLASS},
    {"const", token_types::T_CONST},
    {"false", token_types::T_FALSE},
    {"float", token_types::T_FLOAT_LITERAL},
    {"local", token_types::T_LOCAL},
    {"raise", token_types::T_RAISE},
    {"while", token_types::T_WHILE},
};

const std::vector<std::pair<std::string, token_types>> SAL_SIX_CHAR_OBJECTS = {
    {"except", token_types::T_EXCEPT},
    {"struct", token_types::T_STRUCT},
    {"__py__", token_types::T_PYTHON},
    {"return", token_types::T_RETURN},
};

const std::vector<std::pair<std::string, token_types>> SAL_SEVEN_CHAR_OBJECTS = {
    {"finally", token_types::T_FINALLY},
    {"__asm__", token_types::T_ASSEMBLY},
    {"include", token_types::T_INCLUDE}
};

const std::vector<std::pair<std::string, token_types>> SAL_EIGHT_CHAR_OBJECTS = {
    {"continue", token_types::T_CONTINUE},
    {"override", token_types::T_OVERRIDE},
    {"__init__", token_types::T_INIT}
};

const std::vector<std::pair<std::string, token_types>> SAL_NINE_CHAR_OBJECTS = {
    {"namespace", token_types::T_NAMESPACE},
    {"protected", token_types::T_PROTECTED},
};

const int largest_operator_size = 2; // The size of the largest operator, so the lexer knows how far to look ahead.
// So + = 1, but ++ = 2. Pretty straightforward.
// Operators like "or", "and", or even "concatenate" should be separated by spaces. This is mostly for expressions.

// Allows us to sort quickly based on the index.
// TODO: Hardcode this for memory efficiency (or check if the C++ compiler already does that.)
const std::vector<std::vector<std::pair<std::string, token_types>>> lists_by_size = {
    SAL_ONE_CHAR_OBJECTS,
    SAL_TWO_CHAR_OBJECTS,
    SAL_THREE_CHAR_OBJECTS,
    SAL_FOUR_CHAR_OBJECTS,
    SAL_FIVE_CHAR_OBJECTS,
    SAL_SIX_CHAR_OBJECTS,
    SAL_SEVEN_CHAR_OBJECTS,
    SAL_EIGHT_CHAR_OBJECTS,
    SAL_NINE_CHAR_OBJECTS
};

#include <iostream>
// This function was made with AI since I'm NOT writing all of that out. Thanks for understanding :/
// It's only really used for debugging anyways.
// Converts the enum value into a readable string format "[T_...]"
/**
 * @brief Converts the token type to a string.
 * @param type The type to be converted.
 */
inline std::string token_type_to_string(token_types type) {
    switch (type) {
        // Identifiers & Specials
        case T_LINK:          return "[T_LINK]"; // Links other .SAL files.
        case T_PYTHON:        return "[T_PYTHON]"; // Allows for inline python (eventually)
        case T_ASSEMBLY:      return "[T_ASSEMBLY]"; // Allows for inline assembly
        case T_INIT:          return "[T_INIT]"; // The main entry function like int main() or __self__

        // Delimiters
        case T_LPAREN:        return "[T_LPAREN]"; // (
        case T_RPAREN:        return "[T_RPAREN]"; // )
        case T_LBRACE:        return "[T_LBRACE]"; // {
        case T_RBRACE:        return "[T_RBRACE]"; // }
        case T_LBRACKET:      return "[T_LBRACKET]"; // [
        case T_RBRACKET:      return "[T_RBRACKET]"; // ]
        case T_COLON:         return "[T_COLON]"; // :
        case T_COMMA:         return "[T_COMMA]"; // ,
        case T_DOT:           return "[T_DOT]"; // .
        case T_AT:            return "[T_AT]"; // @
        case T_BAR:           return "[T_BAR]"; // |

        // Operators
        case T_POWER:         return "[T_POWER]"; // !
        case T_DIV:           return "[T_DIV]"; // /
        case T_MULT:          return "[T_MULT]"; // *
        case T_ADD:           return "[T_ADD]"; // +
        case T_SUB:           return "[T_SUB]"; // -
        case T_ASSIGN:        return "[T_ASSIGN]"; // =
        case T_LESS:          return "[T_LESS]"; // <
        case T_GREATER:       return "[T_GREATER]"; // >
        case T_DESTRUCT:      return "[T_DESTRUCT]"; // ~
        
        // 2-Char Operators
        case T_NOT_EQUAL:     return "[T_NOT_EQUAL]"; // !=
        case T_ROOT:          return "[T_ROOT]"; // !/
        case T_MULT_ASSIGN:   return "[T_MULT_ASSIGN]"; // *=
        case T_DIV_ASSIGN:    return "[T_DIV_ASSIGN]"; // /=
        case T_ADD_ASSIGN:    return "[T_ADD_ASSIGN]"; // +=
        case T_SUB_ASSIGN:    return "[T_SUB_ASSIGN]"; // -=
        case T_LESS_EQUAL:    return "[T_LESS_EQUAL]"; // <=
        case T_GREATER_EQUAL: return "[T_GREATER_EQUAL]"; // >=
        case T_ADD_ONE:       return "[T_ADD_ONE]"; // ++
        case T_SUB_ONE:       return "[T_SUB_ONE]"; // --
        case T_IS_EQUAL:      return "[T_IS_EQUAL]"; // ==

        // Literals
        case T_IDENTIFIER:    return "[T_IDENTIFIER]"; // Identifiers for function names and whatnot.
        case T_INT_LITERAL:   return "[T_INT_LITERAL]"; // 1,2,3...
        case T_FLOAT_LITERAL: return "[T_FLOAT_LITERAL]"; // .123, etc...
        case T_STRING_LITERAL:return "[T_STRING_LITERAL]"; // "Hello World!"

        // Keywords
        case T_IF:            return "[T_IF]"; // If
        case T_OR:            return "[T_OR]"; // Or
        case T_AND:           return "[T_AND]"; // And
        case T_DEF:           return "[T_DEF]"; // Define a function
        case T_DYN:           return "[T_DYN]"; // Define a dynamic variable or function.
        case T_FOR:           return "[T_FOR]"; // For int i in ...
        case T_NOT:           return "[T_NOT]"; // !=, not, etc...
        case T_TRY:           return "[T_TRY]"; // Try, catch, etc...
        case T_SELF:          return "[T_SELF]";
        case T_BOOL:          return "[T_BOOL]";
        case T_TRUE:          return "[T_TRUE]";
        case T_VOID:          return "[T_VOID]";
        case T_ELSE:          return "[T_ELSE]";
        case T_ASYNC:         return "[T_ASYNC]";
        case T_BREAK:         return "[T_BREAK]";
        case T_CLASS:         return "[T_CLASS]";
        case T_CONST:         return "[T_CONST]";
        case T_FALSE:         return "[T_FALSE]";
        case T_LOCAL:         return "[T_LOCAL]";
        case T_RAISE:         return "[T_RAISE]";
        case T_WHILE:         return "[T_WHILE]";
        case T_INDENT:        return "[T_INDENT]";
        case T_DEDENT:        return "[T_DEDENT]";
        case T_RETURN:        return "[T_RETURN]";
        case T_EXCEPT:        return "[T_EXCEPT]";
        case T_STRUCT:        return "[T_STRUCT]";
        case T_FINALLY:       return "[T_FINALLY]";
        case T_CONTINUE:      return "[T_CONTINUE]";
        case T_OVERRIDE:      return "[T_OVERRIDE]";
        case T_NAMESPACE:     return "[T_NAMESPACE]";
        case T_PROTECTED:     return "[T_PROTECTED]"; // Cannot leave it's current namespace

        case T_NEWLINE:       return "[T_NEWLINE]";

        default:              return "[UNKNOWN_TOKEN]";
    }
}

// Helper function to print out a whole list of parsed tokens cleanly
/**
 * @brief Prints the tokens out into the terminal, used for debugging.
 * @param tokens A list of tokens to print out. Must be flattened.
 */
inline void print_tokens(const std::vector<SAL_TOKEN>& tokens) {
    for (const auto& token : tokens) {
        std::cout << "Line " << token.line+1 
                  << " Col " << token.col << ": "
                  << token_type_to_string(token.token_type) 
                  << " -> \"" << token.original_text << "\"" 
                  << std::endl;
        
    }
}


/**
 * @brief Checks if a string is a delimiter.
 * @param input_text The text to be checked.
 */
inline bool is_delimiter(std::string_view input_text) {
    for (auto item : SAL_DELIMITERS) {
        if (item.first == input_text) {
            return true;
        }
    }
    return false; // Returns false otherwise.
}

/**
 * @brief Checks if the input token type is a delimiter.
 * @param input_token_type The type of token to check against.
 */
inline bool is_delimiter(token_types input_token_type) {
    for (auto item : SAL_DELIMITERS) {
        if (item.second == input_token_type) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if a string is an operator.
 * @param input_text The text to be checked.
 */
inline bool is_operator(std::string_view input_text) {
    for (auto item : SAL_OPERATORS) {
        if (item.first == input_text) {
            return true;
        }
    }
    return false; // Returns false otherwise.
}


/**
 * @brief Checks if the type is an operator.
 * @param input_token_type The type of the token to check against.
 */
inline bool is_operator(token_types input_token_type) {
    for (auto item : SAL_OPERATORS) {
        if (input_token_type == item.second) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if a string is an operator or delimiter.
 * @param input_text The text to check.
 */
inline bool is_operator_or_delimiter(std::string_view input_text) {
    if (is_operator(input_text) || is_delimiter(input_text)) {return true;}
    return false; // Return false otherwise.
}

/**
 * @brief Checks if a string is an operator or delimiter.
 * @param input_text The text to check for.
 * @param exclude Text to exclude from the search.
 */
inline bool is_operator_or_delimiter(std::string_view input_text, std::string exclude) {
    if (input_text == exclude) {return false;} // We found the excluded thing, so pass over it.
    if (is_operator(input_text) || is_delimiter(input_text)) {return true;}
    return false; // Return false otherwise.
}

/**
 * @brief Checks if a type is an operator or delimiter.
 * @param input_token_type The type of the token to compare against.
 */
inline bool is_operator_or_delimiter(token_types input_token_type) {
    if (is_operator(input_token_type) || is_delimiter(input_token_type)) {return true;}
    return false; // Returns false otherwise.
}
