#include "parser.h"
#include <string>
#include <memory>

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

bool is_expression(std::vector<SAL_TOKEN> input_tokens) { // If a list of data is an expression.

}

bool is_binary_operator() {

}

bool is_unary_operator() {

}

class create_AST {
    public:
    virtual std::vector<SAL_TOKEN> get_content() const = 0;
};

class function_declaration : public create_AST {
    public:
    /**
     * @brief Creates a
     */
    function_declaration() {

    }
    std::vector<SAL_TOKEN> get_content() const override {

    }
    private:
    std::vector<std::unique_ptr<create_AST*>> sub_nodes;
};
class declaration : public create_AST {
    public:
    std::vector<SAL_TOKEN> get_content() const override {}
    declaration(std::vector<SAL_TOKEN> type_tokens, SAL_TOKEN ) {}
    private:
};
class unary_operator : public create_AST { // Operator for functions like !/, ++, etc...
    public:
    /**
     * @brief Creates an operator function with the operator's function token, like !/, ++, etc...
     */
    unary_operator(SAL_TOKEN in_operator_function, SAL_TOKEN in_value) {operator_function = in_operator_function; value = in_value;}
    std::vector<SAL_TOKEN> get_content() const override {

    }

    private:
    SAL_TOKEN operator_function;
    SAL_TOKEN value;
};

class binary_operator : public create_AST { // Operator for functions like !, +, etc...
    public:
    std::vector<SAL_TOKEN> get_content() const override {

    }
    binary_operator(SAL_TOKEN first_value, SAL_TOKEN operator_function, SAL_TOKEN second_value) 
    : operator_funct(operator_function),first_val(first_value),second_val(second_value) {}

    private:
    SAL_TOKEN operator_funct;
    SAL_TOKEN first_val;
    SAL_TOKEN second_val;
};
class expression : public create_AST {
    public:
    std::vector<SAL_TOKEN> get_content() const override  {
        
    }
    expression(std::vector<SAL_TOKEN> input_tokens) {

    }
    private:
    std::vector<create_AST*> AST_nodes;
};
class parse_AST : public create_AST {
    public:
    std::vector<SAL_TOKEN> get_content() const override {}

    std::vector<std::unique_ptr<create_AST>> build_AST(SAL_FILE input_file) {
        // Take all the AST nodes and push them together into one large list.
        while(cursor < input_file.tokens.size()) {
            auto& cur_token = input_file.tokens[cursor];
            if (cur_token.token_type == token_types::T_IDENTIFIER) {
                // Skip over it for now, since this is likely for an operator or something.
                goto end_AST_node;
            }
            if (cur_token.token_type == token_types::T_LINK) {
                // Now we have to link the file. The only following argument can be an identifier, otherwise throw an error.
                if (peek().token_type != token_types::T_IDENTIFIER) {
                    std::cout << "Expected a file identifier for link statement. @" << input_file.filename << ":" << cur_token.line << ":" << cur_token.col;
                }
                // Now we have to create an entirely new file.
                
            }
            if (is_type(cur_token.token_type)) {
                std::vector<SAL_TOKEN> identifying_tokens;
                while(is_type(cur_token.token_type) && cursor < input_file.tokens.size()) {
                    identifying_tokens.push_back(cur_token);
                    goto end_AST_node;
                }
                // Now we have to figure out is this an identifier or a function?

            }
            if (is_operator(cur_token.token_type)) {
                // Now we have to collect all of the tokens within this expression.
                std::vector<SAL_TOKEN> expression_operators;
                while(is_expression(expression_operators)) {
                    expression_operators.push_back(cur_token);
                    cursor++;
                }
                expression new_expression();
                goto end_AST_node;
            }

            end_AST_node:
            cursor++;
        }
    }
    protected:
    int cursor;
    std::vector<SAL_TOKEN> flat_token_list;
    std::vector<create_AST*> AST_NODES;
    SAL_TOKEN peek() {return flat_token_list[cursor+1];}
    SAL_TOKEN current() {return flat_token_list[cursor];}
    SAL_TOKEN prev() {return flat_token_list[cursor-1];}
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
    // Add our tokens.
    input_file.tokens = flattened_lexed_file;
    // Now start parsing the file.
    parse_AST new_AST;
    new_AST.build_AST(input_file);


    // Now we can create our main AST object.

    // Now we can start adding content.
}