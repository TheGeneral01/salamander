/* =================================================================================================== */
/*                                                                                                     */
/*  Module: sal.cpp                                                                                    */
/*  Description: The main compilation file for SAL.                                                    */
/*  Date Last Updated: 9/23/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#include "compiler/lexer.h"
#include "compiler/parser.h"
#include "compiler/fileScope.h"
#include "compiler/internal/Eft.h"
#include "compiler/internal/GpuScheduler.h"
#include "compiler/internal/Newt.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

fs::path globalCompilerPath("");

// Valid compiler flags for the SAL compiler specifically, not the g++ compiler.
const std::vector<std::string> validCompilerFlags = {
    "run",
    "--noGPU",
    "--noThread",
    "--JIT",
    "--debug",
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

class compiler {
    public:
    RawSalFile file;
    std::vector<std::unique_ptr<Stmt>> finalAST;
    Newt interpreter;

    void compile(const fs::path& filepath) {
        file.filepath = filepath;

        lexer sourceLexer;
        file.lexedFile = sourceLexer.lexFile(file.filepath);

        parser sourceParser(file.lexedFile);
        auto parsedAST = sourceParser.parse();
        if (!checkFileScope(parsedAST)) {
            throw std::runtime_error("File scope validation failed");
        }

        Preprocessor preprocessor;
        preprocessor.flattenAST(std::move(parsedAST));
        finalAST = std::move(preprocessor.processedAST);
        GpuScheduler gpuScheduler;
        gpuScheduler.run(finalAST);
        interpreter.runAST(finalAST);
    }

};

/**
 * @brief The input for compilation from the terminmal
 * @param argc The number of arguments.
 * @param argv The list of arguments.
 */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        throw std::runtime_error("No file provided for compilation");
    }
    const char* filename = argv[1]; // The filename is always the second argument. (Not the first, since the first is ./sal itself.)
    // Now we need to determine whether the input a .sal file or not.
    if (!endsWith(filename, ".sal")) {
        throw std::runtime_error("Invalid file provided");
    }

    try {
        compiler salCompiler;
        salCompiler.compile(fs::path(filename));
    } catch (const std::exception& error) {
        std::cerr << "SAL compilation failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}