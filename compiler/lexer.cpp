// Why tf did i put header guards in a cpp file

//The only reason this is written in C++ and not C is because IDK the weird C mannerisms, so deal with it until I get around to it. Thank you :D

#include "lexer.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cassert>

// experimental:



/**
 * @brief Checks if a string is a delimiter.
 * @param inpit_text The text to be checked.
 */
bool is_delimiter(std::string_view input_text) {
    for (auto item : SAL_DELIMITERS) {
        if (item.first == input_text) {
            return true;
        }
    }
    return false; // Returns false otherwise.
}

/**
 * @brief Checks if a string is an operator.
 * @param input_text The text to be checked.
 */
bool is_operator(std::string_view input_text) {
    for (auto item : SAL_OPERATORS) {
        if (item.first == input_text) {
            return true;
        }
    }
    return false; // Returns false otherwise.
}

bool is_operator_or_delimiter(std::string_view input_text) {
    if (is_operator(input_text) || is_delimiter(input_text)) {return true;}
    return false; // Return false otherwise.
}

bool is_operator_or_delimiter(std::string_view input_text, std::string exclude) {
    if (input_text == exclude) {return false;} // We found the excluded thing, so pass over it.
    if (is_operator(input_text) || is_delimiter(input_text)) {return true;}
    return false; // Return false otherwise.
}

bool has_content(std::string_view input_text, char exclude) {
    for (auto item : input_text) {
        if (item == exclude) {return false;}
        if (!std::isspace(item)) {return true;}
    }
    return false;
}


/**
 * @brief Takes the input string for a filename, parses it into tokens, and outputs it as an struct containing necessary data.
 * @param input_file The SAL_FILE to be parsed into tokens.
 */
void lexer(SAL_FILE& input_file) {
    // We are going to read the file line by line-ish, with some hardcoded exceptions.
    std::ifstream file(input_file.filename);
    assert(file.is_open() && "Error when opening file!");
    
    int asdf = 0;
    std::vector<std::string> lines_in_file;
    std::string line; 
    while (std::getline(file, line)) {
        lines_in_file.push_back(line);
        // #TODO: Remove all comments here as an optimization, if the first 2 chars start with #.
        asdf++;
    }
    
    int cur_line = 0;
    int cursor = 0;
    
    // Create the main vector.
    std::vector<std::vector<SAL_TOKEN>> lexed_file;
    lexed_file.resize(lines_in_file.size());

    int indent_counter = 0; // The sum of tab characters or 4 spaces.
    bool seen_content_in_cur_line = false;

    int delimiter_count = 0;
    
    while(cur_line < lines_in_file.size()) {
        // Check if the line is empty.
        if (cursor >= lines_in_file[cur_line].length()) {
            cursor = 0;
            cur_line++;
            seen_content_in_cur_line = false;
            goto end_statement;
        }

        /*
        Indents:
        must occur when content is present AFTER the indent.
        if content is not found, or comment or something, skip to the next line.
        if content is found, determine the amount of indents.
        if the line has no indents, evaluate back to 0.
        */
        
        // Check if the character is empty, if so, then keep going. If it's a tab or 4 spaces, then mark as an indent/dedent
        while(cursor < lines_in_file[cur_line].length() && std::isspace(static_cast<unsigned char>(lines_in_file[cur_line][cursor]))) {
            if (!seen_content_in_cur_line && has_content(lines_in_file[cur_line], '#') && delimiter_count == 0) { // Excludes comments from counting as content.
                int here_indents = 0;
                while(std::isspace(static_cast<unsigned char>(lines_in_file[cur_line][cursor]))) {
                    if (cursor >= lines_in_file[cur_line].length()) {
                        // No content found, skip line.
                        cursor = 0;
                        cur_line++;
                        seen_content_in_cur_line = false;
                        goto end_statement;
                    }
                    if (lines_in_file[cur_line].substr(cursor, 1) == "\t") {
                        here_indents++;
                        cursor++;
                    } else {
                        try {
                            if (cursor + 4 <= lines_in_file[cur_line].length() && lines_in_file[cur_line].substr(cursor, 4) == "    ") {
                                here_indents++;
                                cursor += 4;
                            } else {
                                // What did you do you didn't indent right you moron
                                throw 100;
                            }
                        }
                        catch(int error_num) {
                            std::cout << "Improper indentation detected @" << input_file.filename << ":" << cur_line << ":" << cursor << "\n";
                            std::cout << "Exited with error: " << error_num;
                            std::exit(error_num);
                        }
                    }
                }
                seen_content_in_cur_line = true;
                if (indent_counter != here_indents) {
                    // Now evaluate indents.
                    if (here_indents > indent_counter) {
                        for (int i = here_indents - indent_counter; i > 0; i--) {
                            SAL_TOKEN indent_token;
                            indent_token.col = 0;
                            indent_token.length = 4;
                            indent_token.line = cur_line;
                            indent_token.original_text = "    ";
                            indent_token.token_type = token_types::T_INDENT;
                            lexed_file[cur_line].push_back(indent_token);
                        }
                        indent_counter = here_indents;
                        goto end_statement;
                    }
                    if (here_indents < indent_counter) {
                        for (int i = indent_counter - here_indents; i > 0; i--) {
                            SAL_TOKEN dedent_token;
                            dedent_token.col = 0;
                            dedent_token.length = 4;
                            dedent_token.line = cur_line;
                            dedent_token.original_text = "    ";
                            dedent_token.token_type = token_types::T_DEDENT;
                            lexed_file[cur_line].push_back(dedent_token);
                        }
                        indent_counter = here_indents;
                        goto end_statement;
                    }
                }
            } else {
                cursor++;
            }
        }

        if (indent_counter != 0 && (!std::isspace(lines_in_file[cur_line][0]) || lines_in_file[cur_line].length() == 0)) {
            // Whitespace check is already handled above. A little more unsafe but also more efficient.
            for (int i = indent_counter; i > 0; i--) {
                SAL_TOKEN indent_token;
                indent_token.col = 0;
                indent_token.length = 4;
                indent_token.line = cur_line;
                indent_token.original_text = "    ";
                indent_token.token_type = token_types::T_DEDENT;
                lexed_file[cur_line].push_back(indent_token);
            }
            indent_counter = 0;
        }

        seen_content_in_cur_line = true;
        
        // Check if the cursor is longer than the line length.
        if (cursor >= lines_in_file[cur_line].length()) {
            cursor = 0;
            cur_line++;
            seen_content_in_cur_line = false;
            goto end_statement;
        }
        
        // First check if it's a comment.
        if (lines_in_file[cur_line][cursor] == '#') { 
            cursor = 0;
            cur_line++;
            seen_content_in_cur_line = false;
            goto end_statement;
        }
        
        // Next, check if it's a string.
        if (lines_in_file[cur_line][cursor] == '"') {
            SAL_TOKEN str_token;
            str_token.line = cur_line;
            str_token.col = cursor; 
            std::string found_string = "";

            cursor++; // go past the "
            
            while (cur_line < lines_in_file.size()) {
                if (cursor >= lines_in_file[cur_line].length()) {
                    cur_line++;
                    cursor = 0;
                    found_string += "\n"; 
                    continue;
                }

                if (lines_in_file[cur_line][cursor] == '"') {
                    break; 
                }

                found_string += lines_in_file[cur_line][cursor];
                cursor++;
            }
            
            str_token.length = found_string.length() + 2; // Account for both quotes in length
            str_token.original_text = "\"" + found_string + "\"";
            str_token.token_type = token_types::T_STRING_LITERAL;
            
            cursor++; // Move past the closing quote character
            lexed_file[str_token.line].push_back(str_token); 
            goto end_statement;
        }
        
        // Check if it's an integer or float literal
        if (std::isdigit(static_cast<unsigned char>(lines_in_file[cur_line][cursor]))) { 
            SAL_TOKEN num_token;
            num_token.col = cursor;
            num_token.line = cur_line;
            bool is_float = false;
            std::string num = "";
            
            // Collect the number.
            while (cursor < lines_in_file[cur_line].length() && 
                 (!std::isspace(static_cast<unsigned char>(lines_in_file[cur_line][cursor])) || 
                  is_operator_or_delimiter(std::string_view(&lines_in_file[cur_line][cursor], 1)))) {
                
                // Allow the dot only for floating points; split if it bumps into any other operator
                if (lines_in_file[cur_line][cursor] == '.') {
                    is_float = true;
                } else if (is_operator_or_delimiter(std::string_view(&lines_in_file[cur_line][cursor], 1))) {
                    break;
                }
                
                num += lines_in_file[cur_line][cursor]; 
                cursor++; 
            }
            
            if (is_float) { num_token.token_type = token_types::T_FLOAT_LITERAL; } 
            else { num_token.token_type = token_types::T_INT_LITERAL; }
            
            num_token.length = num.length();
            num_token.original_text = num;
            lexed_file[num_token.line].push_back(num_token);
            goto end_statement;
        }
        
        // Check for operator or delimiter match
        // #TODO: Make this call check for all operators or delimiters. Or just rewrite this as a for loop or something.
        if (is_operator_or_delimiter(std::string_view(&lines_in_file[cur_line][cursor], 1))) {
            std::string str_thingy = "";
            SAL_TOKEN thing_token;
            thing_token.col = cursor;
            thing_token.line = cur_line;

            if (
                lines_in_file[cur_line][cursor] == '{' ||
                lines_in_file[cur_line][cursor] == '[' ||
                lines_in_file[cur_line][cursor] == '('
            ) {
                delimiter_count++;
            }

            if (
                lines_in_file[cur_line][cursor] == '}' ||
                lines_in_file[cur_line][cursor] == ']' ||
                lines_in_file[cur_line][cursor] == ')'
            ) {
                delimiter_count--;
            }

            
            // Always loop backwards from largest_operator_size down to 1
            for (int i = largest_operator_size; i >= 1; i--) {
                if (cursor + i <= lines_in_file[cur_line].length()) {
                    std::string op_txt = lines_in_file[cur_line].substr(cursor, i);
                    std::string_view slice(op_txt.data(), i);
                    if (is_operator_or_delimiter(slice)) {
                        str_thingy = std::string(slice);
                        break; // Found the absolute largest match
                    }
                }
            }
            
            cursor += str_thingy.length();
            
            if (str_thingy.length() > 0 && str_thingy.length() <= lists_by_size.size()) {
                for (auto const& item : lists_by_size[str_thingy.length() - 1]) {
                    if (str_thingy == item.first) { 
                        thing_token.token_type = item.second; 
                        break; 
                    }
                }
            }
            
            thing_token.length = str_thingy.length();
            thing_token.original_text = str_thingy;
            lexed_file[thing_token.line].push_back(thing_token);
            goto end_statement;
        } 
        // Must be a keyword or identifier otherwise.
        else {
            SAL_TOKEN else_token; 
            std::string current_object = "";
            else_token.col = cursor;
            else_token.line = cur_line;
            
            // Collect the total words before an identifier, etc...
            while(cursor < lines_in_file[cur_line].length() && 
                  !std::isspace(static_cast<unsigned char>(lines_in_file[cur_line][cursor])) &&
                  !is_operator_or_delimiter(std::string_view(&lines_in_file[cur_line][cursor], 1)) &&
                  lines_in_file[cur_line][cursor] != '#') {
                current_object += (lines_in_file[cur_line][cursor]);
                cursor++;
            }
            
            else_token.length = current_object.length();
            else_token.original_text = current_object; 
            else_token.token_type = token_types::T_IDENTIFIER;

            if (current_object.length() > 0 && current_object.length() <= lists_by_size.size()) {
                for (auto const& item : lists_by_size[current_object.length() - 1]) {
                    if (item.first == current_object) {
                        else_token.token_type = item.second;
                        break;
                    }
                }
            }
            lexed_file[else_token.line].push_back(else_token);
        }
        end_statement:;
    }
    // Edge case for the last line, where we ensure that dedents are accounted for.
    if (indent_counter != 0) {
        for (int i = indent_counter; i > 0; i--) {
            SAL_TOKEN indent_token;
            indent_token.col = 0;
            indent_token.length = 4;
            indent_token.line = cur_line;
            indent_token.original_text = "    ";
            indent_token.token_type = token_types::T_DEDENT;
            // Make a new line
            lexed_file.push_back({indent_token});
        }
        indent_counter = 0;
    }
    
    //for (auto const& item : lexed_file) {
    //    print_tokens(item);
    //} // Debug printout.

    input_file.lexed_file = lexed_file;

}
