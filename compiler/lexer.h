/* =================================================================================================== */
/*                                                                                                     */
/*  Module: lexer.h                                                                                    */
/*  Description: Lexes the given file into it's tokens.                                                */
/*  Date Last Updated: 9/21/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once
#include "SALItms.h"
#include "utils/printTkn.h"
#include <vector>
#include <string>
#include <SALItms.h>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <stdexcept>

class lexer {
    std::vector<std::string> lddFile = {};
    std::vector<SALTKN> tokens;
    int line = 0, col = 0;
    public:
    lexer() = default;
    /**
     * @brief Lexes the current given file.
     * @param filepath The path to parse.
     */
    lexer(fs::path filepath) {
        // First load the file one line at a time.
        std::ifstream strFile(filepath);

        if (!strFile.is_open()) {
            throw std::runtime_error("Failed to open file"); // #TODO: Add the file name here eventually.
        }

        std::string curLine; // Push this to the class list holder..
        while(std::getline(strFile, curLine)) {
            lddFile.push_back(curLine);
        }

        lexFile();
    }

    // References to current, past, and next data.
    bool EOL() {return EoF() || col >= lddFile[line].length();}
    bool EoF() {return line >= lddFile.size();}
    bool chk() {
        if (EOL() || EoF()) return true;
        return false;
    }
    void dingLine() {line++; col = 0;}
    const char& curChar() {return lddFile[line][col];}
    char getNxtChar() {
        if (EoF() || col + 1 >= lddFile[line].length()) return '\0';
        return lddFile[line][col + 1];
    }
    char nxtChar() {
        if (EoF()) return '\0';
        char cur(lddFile[line][col++]);
        if (EOL()) dingLine();
        return cur;
    }
    bool isChar(char txt) {return curChar() == txt;}
    std::string getTxt(int length) {
        if (EoF()) return "";
        return lddFile[line].substr(col, length);
    }
    std::string goToSpace() {
        std::string text = "";
        while (!chk() && !std::isspace(static_cast<unsigned char>(curChar())) &&
               !SalDelimiters.contains(getTxt(1)) &&
               !SalOperators.contains(getTxt(1)) &&
               !SalOperators.contains(getTxt(2))) {
            text += nxtChar();
        }
        return text;
    }
    void goToNxt() {
        while (!chk() && std::isspace(static_cast<unsigned char>(curChar()))) nxtChar();
    }
    bool isStr(std::string text) {return !EoF() && getTxt(text.size()) == text;}
    bool ifStrAdv(std::string text) {    
        if (isStr(text)) {col += text.size(); return true;}
        return false;
    }
    int indentCtr = 0;

    /**
     * @brief Lexes the given filepath.
     * @param filepath An std::filesystem::path path to the file.
     * @return Returns a vector of SALTKN.
     */
    std::vector<SALTKN> lexFile() {
        while (!EoF()) {
            if (EOL()) {
                dingLine();
                continue;
            }
            if (col == 0) { // Check for indentation.
                // Count the indents.
                int trackedIndents = 0;
                while (ifStrAdv("    ") || ifStrAdv("\t")) {
                    trackedIndents++;
                }
                for (int i = abs(trackedIndents - indentCtr); i > 0; i--) {
                    // We need to emit an indent token per each indent/dedent.
                    if ((trackedIndents - indentCtr) > 0) {
                        // Indent token.
                        tokens.push_back(makeToken(INDENT, "    ", line, 0, 4));
                    } else {
                        // Dedent token.
                        tokens.push_back(makeToken(DEDENT, "    ", line, 0, 4));
                    }
                }
                indentCtr = trackedIndents;
            }

            if (EOL()) {
                dingLine();
                continue;
            }

            // First move past whitespace.
            while (!chk() && std::isspace(static_cast<unsigned char>(curChar()))) nxtChar();
            if (isChar('#')) dingLine(); // Skips everything else from this line and moves on.
            if (chk()) continue; // Skip back to the beginning of this while loop and move on.
            int tokenLine = line;
            int tokenCol = col;

            // Now we need to check if it's a string.
            if (isChar('f') || isChar('F')) {
                // now we need to check that this is in fact a string and not the beginning of something else.
                goToNxt(); // Move to the next time we see something.
                if (isChar('"')) {
                    makeFormattedString();
                    continue;
                } // Otherwise we just move on, and it skips this other else if statement anyways.
            } else if (isChar('"')) {
                // We don't skip spaces in here, since they are a part of the string.
                makeString();
                continue;
            }

            // Now we need to check for floats and ints.
            if (curChar() == '.' || std::isdigit(curChar())) {
                // Add a check for the . to be sure it's not an identifier following this. We needed to do that there to capture this.s
                if (curChar() == '.') {
                    if (std::isdigit(getNxtChar())) {
                        makeNum(true);
                        continue;
                    }
                } else {
                    // Normal number operation.
                    makeNum();
                    continue;
                }
            }

            // Now check for operators
            bool handledOperator = false;
            for (int i = largest_operator_size; i > 0; i--) {
                if (SalOperators.contains(getTxt(i))) {
                    // we found the operator.
                    auto op = SalOperators.find(getTxt(i));
                    tokens.push_back(makeToken(op->second, getTxt(i), tokenLine, tokenCol, i));
                    ifStrAdv(getTxt(i)); // Quick way to skip past the operator.
                    handledOperator = true;
                    break;
                }
            }
            if (handledOperator) continue;
            
            // Check for delimiters.
            if (SalDelimiters.contains(getTxt(1))) { // It's a stupid hack but it works. Don't judge me. I hate casting chars.
                auto delim = SalDelimiters.find(getTxt(1)); // Because Char doesn't work.
                tokens.push_back(makeToken(delim->second, delim->first, tokenLine, tokenCol, 1));
                nxtChar();
                continue;
            }

            // Must be a keyword or identifier.
            // First check for keywords.
            std::string txt = goToSpace();
            bool isKeyword = false;
            if (!txt.empty() && txt.length() <= lists_by_size.size()) { // A quick way to see if it's a keyword, or at least, help with efficiency.
                // First check by the matching size.
                for (const auto& item : lists_by_size[txt.length()-1]) {
                    if (item.first == txt) {
                        tokens.push_back(makeToken(item.second, txt, tokenLine, tokenCol, txt.size()));
                        isKeyword = true;
                        break;
                    }
                }
            }
            if (!txt.empty() && !isKeyword) {
                // Must be an identifier if it was not a keyword.
                tokens.push_back(makeToken(IDENTIFIER, txt, tokenLine, tokenCol, txt.length()));
            }
        }
        printLexer(tokens);
        return tokens;
    }

    void makeNum(bool seenDot = false) {
        int tokenLine = line;
        int tokenCol = col;
        std::string number = "";
        while(std::isdigit(curChar()) || (curChar() == '.')) {
            if (curChar() == '.' && seenDot) throw std::runtime_error("Improper use of a floating point number!");
            if (curChar() == '.') seenDot = true;
            number += nxtChar();
        }
        if (seenDot) {
            tokens.push_back(makeToken(FLOATLIT, number, tokenLine, tokenCol, number.length()));
        } else {
            tokens.push_back(makeToken(INTLIT, number, tokenLine, tokenCol, number.length()));
        }
    }

    void makeString() {
        int tokenLine = line;
        int tokenCol = col;
        auto lookForStrChar = nxtChar(); // We start off on the ", so skip that. Make note of what to look for though.
        std::string collectedStr = "";
        while(curChar() != lookForStrChar) collectedStr += nxtChar();
        nxtChar(); // Skip the ending ' or "
        tokens.push_back(makeToken(STRLIT, collectedStr, tokenLine, tokenCol, collectedStr.size()));
    }

    void makeFormattedString() {
        // We need to add all tokens within the {}s.
        int tokenLine = line;
        int tokenCol = col;
        auto tkn = makeToken(FSTRLIT, "", tokenLine, tokenCol);
        auto lookForStrChar = nxtChar(); // Get cur token and move on. We start at the "
        std::string collectedStr = "";
        while (curChar() != lookForStrChar) {
            if (curChar() == '{') {
                auto inTkns = getFormatTkns();
                tkn.subTkns.insert(tkn.subTkns.end(), inTkns.begin(), inTkns.end()); // Merge the two lists together.
            } // We would end up back in the string, so keep adding into collected str.
            collectedStr += nxtChar();
        }
        // Now that we have all of our data, we output our fstring token after some modifications.
        tkn.length = collectedStr.length(); // A close enough approximation, since it doesn't really matter anyways.
        tkn.originalTxt = collectedStr;
        tokens.push_back(tkn);
    }

    std::vector<SALTKN> getFormatTkns() {
        nxtChar(); // We start on the { so skip that.
        std::string collectedStr = "";
        while(curChar() != '}') collectedStr += nxtChar();
        nxtChar(); // Move past the '}' before leaving.
        // Now we are going to make a lexer class here.
        lexer tempLexer;
        return tempLexer.lexString(collectedStr); // Parse the current string and leave.
    }

    /**
     * @brief Lexes the given filepath.
     * @param filepath An std::filesystem::path path to the file.
     * @return Returns a vector of SALTKN.
     */
    std::vector<SALTKN> lexFile(fs::path filepath) {
        // First load the file one line at a time.
        std::ifstream strFile(filepath);

        if (!strFile.is_open()) {
            throw std::runtime_error("Failed to open file"); // #TODO: Add the error name here eventually.
        }

        std::string curLine; // Push this to the class list holder..
        while(std::getline(strFile, curLine)) {
            lddFile.push_back(curLine);
        }

        return lexFile();
    }

    // Shortcut for lexing a given string.
    std::vector<SALTKN> lexString(std::string text) {
        lddFile = {text};

        return lexFile();
    }
};

/**
 * @brief Lexes the current given file into a flat token stream.
 * @param inputFile The file to input.
 * @return void
 * @details none
 */
void lexFile(RawSalFile& inputFile) {
    lexer newLexer;
    inputFile.lexedFile = newLexer.lexFile(inputFile.filepath);
}