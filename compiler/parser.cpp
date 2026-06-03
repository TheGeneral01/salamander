#include "parser.h"
#include <string>

/*
The parser will need to do a few things:
First, flatten the vector that it gets from lexing the file into a readable string. We can introduce a new token reserved for the parser here, that just acts as a separator of statements.
To input that separator, we should just look at when the line changes, or when a semicolon is found.

Second, our parser needs to verify that our data is in the correct order. We can do this by verifying that our rules are being followed.
Our rules can be defined as a series of classes, which 

*/

// A node can only have two different values that it pulls from. It can either be yet another node, or actual data.
// So all we need to do is combine those two data points into a single value, usually through a mathematical operation.
// The problem is if we call get_node_value, what should it return from this node?
struct SAL_AST_NODE {
    SAL_AST_NODE* left_node;
    SAL_AST_NODE* right_node;
    // Now, we have to figure out how we should store our data, essentially, what is the univeral format?
    // Well we already have the tokens so just use those.
    
    // This lets us represent the data as a tree of tokens, where their data can be accessed.
    // The AST is built by parsing all the tokens into a flattened list.
};

// There are two main types of tokens, one with operators x op y, and just op y.
// We can say NUL for op y, since it expects nothing to the left
// And LED (left denotation) for X op Y.

void nul_op() { // Right hand only operator

}

void left_op() { // Both right and left hand operators.

}

class declarations {
    virtual void get_function() const = 0;
};
class function : declarations {
    
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

    // Now that our file is flattened into a vector, we can begin parsing it using our rules.
    while(0 < flattened_lexed_file.size()) {
        auto& cur_token = flattened_lexed_file[parser_cursor];
        // Now we need to determine what kind of expressions to do based on the current token we are on.
        // These typically revolve around things like operators (+, -, *, etc...) or declarations (like def myfunct, etc...)
        // First, lets check for declarations.
        // We can make the return of all functions, convienently, by how much to move the cursor.
        if (cur_token.token_type == token_types::T_DYN) {
            // Expect a declaration, now figure out if its a function or object.
            // So we keep going until we find a keyword, which determines if it's a function declaration or a regular object declaration.
            while(flattened_lexed_file[parser_cursor].token_type != token_types::T_LPAREN) {
                if (flattened_lexed_file[parser_cursor].token_type == token_types::T_NEWLINE) {
                    // We now now it is a declaration and not a function.
                    // Leaving this as break for now so it can be pushed as a rough build.
                    break;
                }
            }
        }
        break;
    }
}