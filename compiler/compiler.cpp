#include "compiler.h"
#include "lexer.h"
#include "parser.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>

//#define NDEBUG - May be useful for later.

/*
Information:

The Compiler for SAL automatically outputs the program as a C function, which is then fully compiled into ASM from the GCC compiler. This is so I can preserve my sanity wihout
having to program different compiler types for different types of ASM. While I could theoretically compile to all types of ASM given I know their ambiguities, I will not for now,
since I'm still somewhat unfamiliar with the full extent of ASM.
*/

enum compiler_flags {
    WITH_COMMENTS, // Includes some comments from the compiler, as well as some of the USR comments to help debug.
    COMPILE_ASM // Compile to ASM instead of C.
};

/**
 * @brief Determines whether a string ends with a certain value or not.
 * @param mainStr The string to check for the ending.
 * @param suffix The string to check if it's at the end of mainStr.
 */
bool endsWith(const std::string& mainStr, const std::string& suffix) {
    if (mainStr.size() < suffix.size()) {
        return false;
    }

    // Get the starting iterator for the end section of mainStr
    auto mainStart = mainStr.end() - suffix.size();

    // Compare characters one by one, ignoring case
    return std::equal(mainStart, mainStr.end(), suffix.begin(), suffix.end(),
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        }
    );
}

/**
 * @brief The main function to start compiling a file based on the file input, and flags.
 * @param argc The number of flags provided.
 * @param argv The list of flags.
 */
int main(int argc, char* argv[]) {
    assert(argc >= 2 && "Must provide at least one argument for compilation!"); // Make sure we have more than 2 arguments. 1 for this file, 1 for file to compile.
    const char* filename = argv[1]; // The filename is always the second argument. (Not the first, since the first is compiler.exe itself.)
    // Now we need to determine whether the input a .sal file or not.
    assert(endsWith(filename, acceptable_suffix) && "File must be a .SAL file!");
    SAL_FILE master_file;
    master_file.filename = filename;
    lexer(master_file); // Send the master file off to the lexer, which finds all the other files for us.
    // Now we need to parse the file.
    pratt_parse(master_file);
}