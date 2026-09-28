/* =================================================================================================== */
/*                                                                                                     */
/*  Module: CppBlockRuntime.h                                                                          */
/*  Description: Compiles and runs standalone C++ blocks emitted from SAL sources.                      */
/*                                                                                                     */
/*  A cpp { ... } block becomes one translation unit that is compiled with g++ into its own             */
/*  executable. SAL variables named in the block signature are handed in through a binary file          */
/*  (SAL_CPP_INPUT) and the declared outputs are read back out of a result file (SAL_CPP_OUTPUT),       */
/*  which gives blocks two-way access to SAL variables without sharing an address space.                */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "stdlib/salrt.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/**
 * @brief One SAL variable exposed to a cpp block.
 * @details Inputs carry the current SAL value. Outputs carry either the current SAL value or the
 *          default for their declared type when the variable does not exist yet.
 */
struct CppBlockBinding {
    std::string name;
    std::string type;
    sal::Value value;
};

class CppBlockRuntime {
public:
    /**
     * @brief Compiles the no-signature cpp blocks into a single shared dispatch binary.
     * @param sources The raw C++ captured between each block's braces, in declaration order.
     * @param bound For each source, whether the block declares SAL variables. Only unbound blocks
     *          are pre-compiled here, because a bound block's list element type depends on the
     *          runtime value and cannot be known at startup.
     * @return The compiled dispatch binary and a map from each block's source to its stable id
     *          inside that binary. The id is derived from the block's translation unit, so the
     *          same block always maps to the same id across runs and the binary is cached on disk.
     */
    struct Prepared {
        std::filesystem::path binaryPath;
        std::map<std::string, std::string> blockIds;
    };

    Prepared prepare(const std::vector<std::string>& sources,
                     const std::vector<bool>& bound) const;

    /**
     * @brief Compiles (when needed) and runs a cpp block, returning the values it wrote back.
     * @param source The raw C++ captured between the block's braces.
     * @param userArgs The command-line arguments the SAL program was launched with, forwarded to
     *          the block verbatim so it can read them through sal::CommandArgs.
     * @param inputs Variables that must already exist in SAL, seeded with their current value.
     * @param outputs Variables that may be created if SAL does not have them yet.
     * @return Every bound variable after the block ran, keyed by SAL variable name.
     */
    std::map<std::string, sal::Value> execute(const std::string& source,
                                               const std::vector<std::string>& userArgs,
                                               const std::vector<CppBlockBinding>& inputs,
                                               const std::vector<CppBlockBinding>& outputs) const;

    /**
     * @brief Runs a single block out of a shared dispatch binary.
     * @param blockId The stable id of the block within the prepared binary.
     * @param binaryPath The dispatch binary produced by prepare().
     * @param userArgs The command-line arguments the SAL program was launched with, forwarded to
     *          the block verbatim so it can read them through sal::CommandArgs.
     * @param inputs Variables that must already exist in SAL, seeded with their current value.
     * @param outputs Variables that may be created if SAL does not have them yet.
     * @return Every bound variable after the block ran, keyed by SAL variable name.
     */
    std::map<std::string, sal::Value> execute(const std::string& blockId,
                                              const std::filesystem::path& binaryPath,
                                              const std::vector<std::string>& userArgs,
                                              const std::vector<CppBlockBinding>& inputs,
                                              const std::vector<CppBlockBinding>& outputs) const;

    /**
     * @brief The value an output starts at when the SAL variable does not exist yet.
     */
    static sal::Value defaultFor(const std::string& salType);

private:
    struct Variable {
        std::string name;
        std::string cppType;
        std::string accessor;
        sal::Value value;
    };

    struct BlockUnit {
        std::string source;
        std::string directives;
        std::string body;
        std::vector<Variable> variables;
    };

    static std::string shellQuote(const std::string& text);
    static std::string compilerCommand();
    static std::string configuredFlags(const char* name);
    static std::string extractDirectives(const std::string& source, std::string& body);
    static std::string cppIdentifier(const std::string& name);
    static std::pair<std::string, std::string> resolveType(const std::string& salType,
                                                           const sal::Value& value);
    static std::pair<std::string, std::string> listType(const sal::Value& value);
    static std::vector<Variable> collectVariables(const std::vector<CppBlockBinding>& inputs,
                                                  const std::vector<CppBlockBinding>& outputs);
    static std::string buildBlockFunction(const BlockUnit& block);
    static std::string buildStandaloneUnit(const BlockUnit& block);
    static std::string buildDispatchUnit(const std::vector<BlockUnit>& blocks);
    static std::string blockIdFor(const BlockUnit& block);
    static std::filesystem::path includeRoot();
    static std::filesystem::path createRunDirectory();
    static std::string readFile(const std::filesystem::path& path);
    static void writeFile(const std::filesystem::path& path, const std::string& contents);
    static std::filesystem::path compileUnit(const std::string& unit, const std::string& prefix);
};

/* --------------------------------------------------------------------------------------------------- */
/*  Command helpers                                                                                    */
/* --------------------------------------------------------------------------------------------------- */

inline std::string CppBlockRuntime::shellQuote(const std::string& text) {
    std::string quoted = "'";
    for (char value : text) {
        if (value == '\'') quoted += "'\\''";
        else quoted += value;
    }
    return quoted + "'";
}

inline std::string CppBlockRuntime::compilerCommand() {
    const char* configured = std::getenv("CXX");
    return configured != nullptr && *configured != '\0' ? configured : "g++";
}

inline std::string CppBlockRuntime::configuredFlags(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::string(" ") + value : std::string{};
}

/* --------------------------------------------------------------------------------------------------- */
/*  Source preparation                                                                                 */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Moves preprocessor lines out of a block body.
 * @details #include and friends must sit at file scope, but a block writes them wherever it likes,
 *          so every line whose first non-space character is '#' is lifted to the top of the unit.
 */
inline std::string CppBlockRuntime::extractDirectives(const std::string& source, std::string& body) {
    std::string directives;
    std::size_t position = 0;
    while (position < source.size()) {
        const std::size_t end = source.find('\n', position);
        const std::size_t lineEnd = end == std::string::npos ? source.size() : end;
        std::string line = source.substr(position, lineEnd - position);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line[first] == '#') {
            directives += line.substr(first) + '\n';
        } else {
            body += line;
            body += '\n';
        }
        position = end == std::string::npos ? source.size() : end + 1;
    }
    return directives;
}

/**
 * @brief Validates that a SAL variable name can be used verbatim as a C++ identifier.
 */
inline std::string CppBlockRuntime::cppIdentifier(const std::string& name) {
    static const std::set<std::string> reserved = {
        "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch", "char",
        "class", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
        "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
        "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for",
        "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
        "noexcept", "not", "nullptr", "operator", "or", "private", "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof",
        "static", "static_assert", "static_cast", "struct", "switch", "template", "this",
        "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union",
        "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor",
    };

    const bool valid = !name.empty() &&
        (std::isalpha(static_cast<unsigned char>(name.front())) != 0 || name.front() == '_') &&
        std::all_of(name.begin(), name.end(), [](unsigned char value) {
            return std::isalnum(value) != 0 || value == '_';
        });
    if (!valid) {
        throw std::runtime_error("cpp block cannot bind SAL variable '" + name +
                                 "' because it is not a valid C++ identifier");
    }
    if (reserved.find(name) != reserved.end()) {
        throw std::runtime_error("cpp block cannot bind SAL variable '" + name +
                                 "' because it is a C++ keyword; rename the SAL variable");
    }
    return name;
}

/**
 * @brief Chooses the C++ type and input accessor used for a bound SAL variable.
 */
inline std::pair<std::string, std::string> CppBlockRuntime::resolveType(const std::string& salType,
                                                                       const sal::Value& value) {
    if (salType == "int") return {"long long", "getInt"};
    if (salType == "float") return {"double", "getFloat"};
    if (salType == "bool") return {"bool", "getBool"};
    if (salType == "string" || salType == "str") return {"std::string", "getString"};
    if (salType == "list") return listType(value);
    if (salType.empty() || salType == "dyn") {
        switch (value.kind) {
            case sal::Value::Kind::Int: return {"long long", "getInt"};
            case sal::Value::Kind::Float: return {"double", "getFloat"};
            case sal::Value::Kind::Bool: return {"bool", "getBool"};
            case sal::Value::Kind::String: return {"std::string", "getString"};
            case sal::Value::Kind::List: return listType(value);
            case sal::Value::Kind::Null: return {"sal::Value", "getValue"};
        }
    }
    throw std::runtime_error("cpp block cannot bind SAL type '" + salType +
                             "'; supported types are int, float, bool, string, list, and dyn");
}

/**
 * @brief Picks a concrete std::vector<T> for a list, falling back to sal::List when it is heterogeneous.
 */
inline std::pair<std::string, std::string> CppBlockRuntime::listType(const sal::Value& value) {
    if (value.kind != sal::Value::Kind::List || value.items.empty()) {
        return {"std::vector<long long>", "getIntList"};
    }
    bool allInt = true;
    bool allNumeric = true;
    bool allBool = true;
    bool allString = true;
    for (const auto& item : value.items) {
        if (item.kind != sal::Value::Kind::Int) allInt = false;
        if (item.kind != sal::Value::Kind::Int && item.kind != sal::Value::Kind::Float) allNumeric = false;
        if (item.kind != sal::Value::Kind::Bool) allBool = false;
        if (item.kind != sal::Value::Kind::String) allString = false;
    }
    if (allInt) return {"std::vector<long long>", "getIntList"};
    if (allNumeric) return {"std::vector<double>", "getFloatList"};
    if (allBool) return {"std::vector<bool>", "getBoolList"};
    if (allString) return {"std::vector<std::string>", "getStringList"};
    return {"sal::List", "getList"};
}

inline sal::Value CppBlockRuntime::defaultFor(const std::string& salType) {
    if (salType == "int") return sal::Value(static_cast<long long>(0));
    if (salType == "float") return sal::Value(0.0);
    if (salType == "bool") return sal::Value(false);
    if (salType == "string" || salType == "str") return sal::Value(std::string{});
    if (salType == "list") return sal::Value(std::vector<sal::Value>{});
    return sal::Value{};
}
/* --------------------------------------------------------------------------------------------------- */
/*  Variable collection                                                                                */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Merges the declared inputs and outputs into one ordered set of C++ variables.
 * @details A name that appears on both sides is declared once, using its input value, and written back.
 */
inline std::vector<CppBlockRuntime::Variable> CppBlockRuntime::collectVariables(
    const std::vector<CppBlockBinding>& inputs,
    const std::vector<CppBlockBinding>& outputs) {
    std::vector<Variable> variables;
    std::map<std::string, std::size_t> positions;

    // Every bound name is materialised and written back, so a block that mutates an input makes that
    // change visible to SAL without the signature having to name the variable a second time.
    auto add = [&](const CppBlockBinding& binding) {
        const std::string name = cppIdentifier(binding.name);
        const std::pair<std::string, std::string> resolved = resolveType(binding.type, binding.value);
        auto found = positions.find(name);
        if (found == positions.end()) {
            positions.emplace(name, variables.size());
            Variable variable;
            variable.name = name;
            variable.cppType = resolved.first;
            variable.accessor = resolved.second;
            variable.value = binding.value;
            variables.push_back(std::move(variable));
            return;
        }
        const Variable& existing = variables[found->second];
        if (existing.cppType != resolved.first) {
            throw std::runtime_error("cpp block binds SAL variable '" + name +
                                     "' as both " + existing.cppType + " and " + resolved.first);
        }
    };

    for (const auto& binding : inputs) add(binding);
    for (const auto& binding : outputs) add(binding);
    return variables;
}

/* --------------------------------------------------------------------------------------------------- */
/*  Code generation                                                                                    */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Builds the standalone function for a single block inside the shared dispatch unit.
 * @details The function takes no arguments: SAL variables are read from SAL_CPP_INPUT and written
 *          back through SAL_CPP_OUTPUT, and command-line/stdin access is wired by the dispatch
 *          main before calling the function. The block id is derived from the function name so the
 *          same block always maps to the same id across runs.
 */
inline std::string CppBlockRuntime::buildBlockFunction(const BlockUnit& block) {
    std::string unit;
    if (!block.directives.empty()) unit += block.directives;

    unit += "\nstatic void sal_cpp_block_" + blockIdFor(block) + "() {\n";
    if (!block.variables.empty()) {
        unit += "    sal::Input sal_input;\n";
        for (const auto& variable : block.variables) {
            unit += "    " + variable.cppType + " " + variable.name + " = sal_input." +
                    variable.accessor + "(\"" + variable.name + "\");\n";
        }
    }

    unit += "    {\n";
    unit += block.body;
    unit += "    }\n";

    if (!block.variables.empty()) {
        unit += "    sal::ResultWriter sal_output;\n";
        for (const auto& variable : block.variables) {
            unit += "    sal_output.emit(\"" + variable.name + "\", " + variable.name + ");\n";
        }
        unit += "    sal_output.commit();\n";
    }
    unit += "}\n";
    return unit;
}

/**
 * @brief Builds a standalone translation unit for a single bound block.
 * @details A bound block's list element type depends on the runtime value, so it cannot be pre-compiled
 *          at startup alongside the no-signature blocks. This unit is a pure function of the block
 *          text and bound names/types, which is what lets the compiled binary be cached on disk.
 */
inline std::string CppBlockRuntime::buildStandaloneUnit(const BlockUnit& block) {
    std::string unit;
    if (!block.directives.empty()) unit += block.directives;

    unit += "#include \"compiler/internal/stdlib/salrt.h\"\n";
    unit += "#include \"compiler/internal/stdlib/print.h\"\n";
    unit += "#include <cstdlib>\n";
    unit += "#include <iostream>\n";
    unit += "#include <string>\n";
    unit += "#include <vector>\n";
    unit += "\n";

    unit += "\nint main(int argc, char** argv) {\n";
    unit += "    sal::CommandArgs::install(argc > 2 ? argc - 2 : 0, argc > 2 ? argv + 2 : nullptr);\n";
    if (!block.variables.empty()) {
        unit += "    sal::Input sal_input;\n";
        for (const auto& variable : block.variables) {
            unit += "    " + variable.cppType + " " + variable.name + " = sal_input." +
                    variable.accessor + "(\"" + variable.name + "\");\n";
        }
    }

    unit += "    {\n";
    unit += block.body;
    unit += "    }\n";

    if (!block.variables.empty()) {
        unit += "    sal::ResultWriter sal_output;\n";
        for (const auto& variable : block.variables) {
            unit += "    sal_output.emit(\"" + variable.name + "\", " + variable.name + ");\n";
        }
        unit += "    sal_output.commit();\n";
    }
    unit += "    return 0;\n";
    unit += "}\n";
    return unit;
}

/**
 * @brief Builds the shared dispatch translation unit for every block.
 * @details One executable replaces one-per-block binaries, so a program pays the compile cost once.
 *          The dispatch main reads a block id from argv[1], forwards argc/argv and stdin to the
 *          block, then dispatches to the matching static function.
 */
inline std::string CppBlockRuntime::buildDispatchUnit(const std::vector<BlockUnit>& blocks) {
    std::string unit;
    unit += "#include \"compiler/internal/stdlib/salrt.h\"\n";
    unit += "#include \"compiler/internal/stdlib/print.h\"\n";
    unit += "#include <cstdlib>\n";
    unit += "#include <iostream>\n";
    unit += "#include <string>\n";
    unit += "#include <vector>\n";
    unit += "\n";

    for (const auto& block : blocks) {
        unit += buildBlockFunction(block);
    }

    unit += "\nint main(int argc, char** argv) {\n";
    unit += "    // argv[0]=dispatch binary, argv[1]=block id, argv[2..]=user args forwarded by SAL.\n";
    unit += "    sal::CommandArgs::install(argc > 2 ? argc - 2 : 0, argc > 2 ? argv + 2 : nullptr);\n";
    unit += "    if (argc < 2) {\n";
    unit += "        std::cerr << \"sal dispatch: missing block id\\n\";\n";
    unit += "        return 2;\n";
    unit += "    }\n";
    unit += "    const std::string blockId = argv[1];\n";
    unit += "    const char* inputPath = std::getenv(\"SAL_CPP_INPUT\");\n";
    unit += "    const char* outputPath = std::getenv(\"SAL_CPP_OUTPUT\");\n";
    unit += "    if (inputPath == nullptr || outputPath == nullptr) {\n";
    unit += "        std::cerr << \"sal dispatch: missing SAL_CPP_INPUT/SAL_CPP_OUTPUT\\n\";\n";
    unit += "        return 2;\n";
    unit += "    }\n";
    unit += "    (void)inputPath;\n";
    unit += "    (void)outputPath;\n";

    for (const auto& block : blocks) {
        unit += "    if (blockId == \"" + blockIdFor(block) + "\") {\n";
        unit += "        sal_cpp_block_" + blockIdFor(block) + "();\n";
        unit += "        return 0;\n";
        unit += "    }\n";
    }

    unit += "    std::cerr << \"sal dispatch: unknown block id '\" << blockId << \"'\\n\";\n";
    unit += "    return 2;\n";
    unit += "}\n";
    return unit;
}

/**
 * @brief A stable identifier for a block, derived from its translation unit so the same block
 *          always maps to the same id across runs and the binary is cacheable on disk.
 */
inline std::string CppBlockRuntime::blockIdFor(const BlockUnit& block) {
    return std::to_string(std::hash<std::string>{}(block.directives + block.body));
}

/* --------------------------------------------------------------------------------------------------- */
/*  Filesystem helpers                                                                                 */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Finds the directory that contains the SAL headers a generated block includes.
 * @details Derived from this header's own path so blocks compile no matter which directory the
 *          interpreter was started from. SAL_CPP_INCLUDE_DIR overrides it.
 */
inline std::filesystem::path CppBlockRuntime::includeRoot() {
    std::vector<std::filesystem::path> candidates;
    const char* configured = std::getenv("SAL_CPP_INCLUDE_DIR");
    if (configured != nullptr && *configured != '\0') candidates.emplace_back(configured);

    std::filesystem::path header(__FILE__);
    if (header.is_relative()) header = std::filesystem::current_path() / header;
    header = header.lexically_normal();
    candidates.push_back(header.parent_path().parent_path().parent_path());
    candidates.push_back(std::filesystem::current_path());

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::exists(candidate / "compiler/internal/stdlib/salrt.h", error)) {
            return candidate;
        }
    }
    return std::filesystem::current_path();
}

inline std::filesystem::path CppBlockRuntime::createRunDirectory() {
    static std::atomic_uint64_t sequence{0};
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const std::filesystem::path candidate =
            base / ("sal_cpp_run_" + std::to_string(sequence.fetch_add(1)));
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error)) return candidate;
    }
    throw std::runtime_error("Could not create a temporary directory for a cpp block");
}

inline std::string CppBlockRuntime::readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

inline void CppBlockRuntime::writeFile(const std::filesystem::path& path,
                                       const std::string& contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not write generated cpp block: " + path.string());
    output << contents;
    output.close();
    if (!output) throw std::runtime_error("Could not write generated cpp block: " + path.string());
}

/* --------------------------------------------------------------------------------------------------- */
/*  Execution                                                                                          */
/* --------------------------------------------------------------------------------------------------- */

inline std::filesystem::path CppBlockRuntime::compileUnit(const std::string& unit, const std::string& prefix) {
    std::error_code cacheError;
    const std::filesystem::path cacheDirectory =
        std::filesystem::temp_directory_path() / "sal_cpp_cache";
    std::filesystem::create_directories(cacheDirectory, cacheError);
    if (cacheError) throw std::runtime_error("Could not create the cpp block cache directory");

    const std::string key = std::to_string(std::hash<std::string>{}(unit));
    const std::filesystem::path sourcePath = cacheDirectory / (prefix + "_" + key + ".cpp");
    const std::filesystem::path binaryPath = cacheDirectory / (prefix + "_" + key);
    const std::filesystem::path errorPath = cacheDirectory / (prefix + "_" + key + ".err");

    if (!std::filesystem::exists(binaryPath) || readFile(sourcePath) != unit) {
        static std::atomic_uint64_t buildSequence{0};
        const std::filesystem::path temporaryBinary = cacheDirectory /
            (prefix + "_" + key + ".tmp." + std::to_string(buildSequence.fetch_add(1)));
        writeFile(sourcePath, unit);

        const std::string compile = compilerCommand() + " -std=c++20 -O2" +
            configuredFlags("SAL_CPP_CXXFLAGS") +
            " -I" + shellQuote(includeRoot().string()) +
            " " + shellQuote(sourcePath.string()) +
            " -o " + shellQuote(temporaryBinary.string()) +
            configuredFlags("SAL_CPP_LDFLAGS") +
            " 2> " + shellQuote(errorPath.string());
        const int status = std::system(compile.c_str());
        if (status != 0) {
            std::string diagnostics = readFile(errorPath);
            if (diagnostics.size() > 4000) diagnostics.resize(4000);
            std::error_code removeError;
            std::filesystem::remove(temporaryBinary, removeError);
            throw std::runtime_error("cpp block failed to compile:\n" + diagnostics);
        }

        std::error_code renameError;
        std::filesystem::rename(temporaryBinary, binaryPath, renameError);
        if (renameError && !std::filesystem::exists(binaryPath)) {
            std::error_code removeError;
            std::filesystem::remove(temporaryBinary, removeError);
            throw std::runtime_error("Could not store the compiled cpp binary: " + renameError.message());
        }
        std::error_code leftoverError;
        std::filesystem::remove(temporaryBinary, leftoverError);
    }
    return binaryPath;
}

inline CppBlockRuntime::Prepared CppBlockRuntime::prepare(const std::vector<std::string>& sources,
                                                          const std::vector<bool>& bound) const {
    Prepared result;
    std::vector<BlockUnit> blocks;
    blocks.reserve(sources.size());

    // Only blocks with no bound SAL variables are pre-compiled here. A bound block's list element
    // type depends on the runtime value and cannot be known at startup, so it falls back to the
    // standalone execute(source,...) path, which is still disk-cached.
    std::map<std::string, std::string> seen;
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const std::string& source = sources[index];
        bool isBound = index < bound.size() ? bound[index] : false;
        if (isBound) continue;

        BlockUnit probe;
        probe.source = source;
        probe.directives = extractDirectives(probe.source, probe.body);
        auto found = seen.find(source);
        if (found != seen.end()) {
            result.blockIds[source] = found->second;
            continue;
        }
        const std::string id = blockIdFor(probe);
        seen[source] = id;
        result.blockIds[source] = id;
        blocks.push_back(std::move(probe));
    }

    if (!blocks.empty()) {
        result.binaryPath = compileUnit(buildDispatchUnit(blocks), "dispatch");
    }
    return result;
}

inline std::map<std::string, sal::Value> CppBlockRuntime::execute(
    const std::string& source,
    const std::vector<std::string>& userArgs,
    const std::vector<CppBlockBinding>& inputs,
    const std::vector<CppBlockBinding>& outputs) const {
    const std::vector<Variable> variables = collectVariables(inputs, outputs);
    BlockUnit block;
    block.source = source;
    block.directives = extractDirectives(block.source, block.body);
    block.variables = variables;

    // A bound block compiles into its own standalone binary on first use; the source is a pure
    // function of the block text and bound names/types, so the same block is cached on disk.
    const std::filesystem::path binaryPath = compileUnit(buildStandaloneUnit(block), "block");
    return execute(blockIdFor(block), binaryPath, userArgs, inputs, outputs);
}

inline std::map<std::string, sal::Value> CppBlockRuntime::execute(
    const std::string& blockId,
    const std::filesystem::path& binaryPath,
    const std::vector<std::string>& userArgs,
    const std::vector<CppBlockBinding>& inputs,
    const std::vector<CppBlockBinding>& outputs) const {
    const std::vector<Variable> variables = collectVariables(inputs, outputs);

    const std::filesystem::path runDirectory = createRunDirectory();
    struct RunDirectoryCleanup {
        std::filesystem::path path;
        ~RunDirectoryCleanup() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{runDirectory};

    const std::filesystem::path inputPath = runDirectory / "input.bin";
    const std::filesystem::path outputPath = runDirectory / "output.bin";

    // Always write the input file, even when the block has no bound variables, so the dispatch
    // main can unconditionally read SAL_CPP_INPUT without having to know the block's signature.
    {
        std::vector<sal::Entry> entries;
        entries.reserve(variables.size());
        for (const auto& variable : variables) {
            entries.emplace_back(variable.name, variable.value);
        }
        sal::writeEntries(inputPath.string(), entries);
    }

    std::string run = "SAL_CPP_INPUT=" + shellQuote(inputPath.string()) +
                      " SAL_CPP_OUTPUT=" + shellQuote(outputPath.string()) +
                      " " + shellQuote(binaryPath.string()) +
                      " " + shellQuote(blockId);
    for (const auto& arg : userArgs) run += " " + shellQuote(arg);
    if (std::system(run.c_str()) != 0) {
        throw std::runtime_error("cpp block execution failed");
    }

    std::map<std::string, sal::Value> result;
    if (std::filesystem::exists(outputPath)) {
        result = sal::readEntries(outputPath.string());
    }
    return result;
}
