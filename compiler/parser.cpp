#include "parser.h"
#include <string>

/*
The parser will need to do a few things:
First, flatten the vector that it gets from lexing the file into a readable string. We can introduce a new token reserved for the parser here, that just acts as a separator of statements.
To input that separator, we should just look at when the line changes, or when a semicolon is found.

Second, our parser needs to verify that our data is in the correct order. We can do this by verifying that our rules are being followed.
Our rules can be defined as a series of classes, which 

*/

// Okay, so I'm going to try to implement a custom AST structure.
// Don't know how well it's going to work, but guess it's time to find out.

bool is_type(token_types input_type) {
    switch(input_type) {
        case T_DYN:      return true;
        case T_BOOL:     return true;
        case T_INT_LITERAL: return true;
        case T_FLOAT_LITERAL: return true;
        case T_STRING_LITERAL: return true;
        case T_VOID: return true;
        case T_ASYNC: return true;
        case T_LOCAL: return true;
        default: return false;
    }
}

class create_AST {
    public:
    virtual SAL_TOKEN get_content() const = 0;
    std::vector<std::unique_ptr<create_AST>> build_AST(std::vector<SAL_TOKEN> input_tokens) {
        // Take all the AST nodes and push them together into one large list.
        int cursor = 0;
        while(cursor < input_tokens.size()) {
            auto& cur_token = input_tokens[cursor];
            if (cur_token.token_type == token_types::T_IDENTIFIER) {
                // Skip over it for now, since this is likely for an operator or something.
                goto end_AST_node;
            }
            if (is_type(cur_token.token_type)) {
                std::vector<SAL_TOKEN> identifying_tokens;
                while(is_type(cur_token.token_type) && cursor < input_tokens.size()) {
                    identifying_tokens.push_back(cur_token);
                    goto end_AST_node;
                }
            }
            if (is_operator(cur_token.token_type)) {
                // We have to first check if it's a one or two sided operator. Theres fewer one sided operators so we can just if else that.
                if (
                    cur_token.token_type == token_types::T_DESTRUCT ||
                    cur_token.token_type == token_types::T_SUB_ONE ||
                    cur_token.token_type == token_types::T_ADD_ONE ||
                    cur_token.token_type == token_types::T_ROOT
                ) {

                } else {

                }
            }

            end_AST_node:
            cursor++;
        }
    }
    protected:
    int current_token;
    std::vector<SAL_TOKEN> flat_token_list;
    std::vector<create_AST*> AST_NODES;
    SAL_TOKEN peek() {return flat_token_list[current_token+1];}
    SAL_TOKEN current() {return flat_token_list[current_token];}
    private:
};
class function_declaration : create_AST {
    public:
    function_declaration() {

    }
    SAL_TOKEN get_content() const override {

    }
    private:
    std::vector<std::unique_ptr<create_AST*>> sub_nodes;
};
class RH_operator : create_AST {
    public:
    RH_operator() {

    }
    SAL_TOKEN get_content() const override {
        
    }

    private:

};


void pratt_parse(SAL_FILE input_file) {
    // From here, we can use the lexed_file vector to navigate our file.
    // First, we need to flatten our tokens into a list.
    int parser_line = 0;
    int parser_cursor = 0;
    std::vector<SAL_TOKEN> flattened_lexed_file;
    int delimiter_count = 0;

    while(parser_line < input_file.lexed_file.size()) {
        const auto current_row = input_file.lexed_file[parser_line];

        if (0 < input_file.lexed_file[parser_line].size()) {
            while(parser_cursor < current_row.size()) {
                // Check if current token is a ;
                // First look for our newline delimiter, ;.
                SAL_TOKEN cur_token = current_row[parser_cursor];
                flattened_lexed_file.push_back(cur_token);
                switch(cur_token.token_type) {
                    case(token_types::T_LBRACE): delimiter_count++;
                    case(token_types::T_LBRACKET): delimiter_count++;
                    case(token_types::T_LPAREN): delimiter_count++;
                    case(token_types::T_RBRACE): delimiter_count--;
                    case(token_types::T_RBRACKET): delimiter_count--;
                    case(token_types::T_RPAREN): delimiter_count--;
                }
                parser_cursor++;
            }
            if (delimiter_count == 0) {
                SAL_TOKEN newline_token;
                newline_token.length = 0;
                newline_token.col = flattened_lexed_file[flattened_lexed_file.size()-1].col + flattened_lexed_file[flattened_lexed_file.size()-1].length;
                newline_token.line = parser_line;
                newline_token.original_text = ";";
                newline_token.token_type = token_types::T_NEWLINE;
                flattened_lexed_file.push_back(newline_token);
            }
            parser_line++;
            parser_cursor = 0;
            // Always add one at the end of the line, since newlines are literally newlines.
        } else {
            // Skip empty rows.
            parser_line++;
            parser_cursor = 0;
        }
    }

    print_tokens(flattened_lexed_file);

    // Now we can create our main AST object.

    // Now we can start adding content.
}