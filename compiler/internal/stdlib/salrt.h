/* =================================================================================================== */
/*                                                                                                     */
/*  Module: salrt.h                                                                                    */
/*  Description: Runtime support shared by the SAL interpreter and generated C++ blocks.                */
/*                                                                                                     */
/*  Generated cpp { ... } blocks are standalone translation units. They receive SAL variables          */
/*  through a binary input file and return SAL variables through a binary result file, so that         */
/*  data crosses the SAL/C++ boundary without any text parsing on either side.                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace sal {

/* --------------------------------------------------------------------------------------------------- */
/*  Dynamic value                                                                                      */
/* --------------------------------------------------------------------------------------------------- */

struct Value;

/**
 * @brief Renders a value the way SAL's print statement would.
 */
inline std::string displayValue(const Value& value);

/**
 * @brief A dynamically typed value mirroring the SAL runtime's value model.
 * @details Blocks that need to work with heterogeneous lists use sal::List and the as* accessors.
 *          Homogeneous lists are handed to blocks as concrete std::vector<T> instead.
 */
struct Value {
    enum class Kind { Null, Int, Float, Bool, String, List };

    Kind kind = Kind::Null;
    long long integer = 0;
    double floating = 0.0;
    bool boolean = false;
    std::string text;
    std::vector<Value> items;

    Value() = default;

    template <typename T,
              typename std::enable_if<std::is_integral<T>::value &&
                                      !std::is_same<T, bool>::value, int>::type = 0>
    Value(T value) : kind(Kind::Int), integer(static_cast<long long>(value)) {}

    template <typename T,
              typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    Value(T value) : kind(Kind::Float), floating(static_cast<double>(value)) {}

    Value(bool value) : kind(Kind::Bool), boolean(value) {}
    Value(const char* value) : kind(Kind::String), text(value) {}
    Value(std::string value) : kind(Kind::String), text(std::move(value)) {}
    Value(std::vector<Value> value) : kind(Kind::List), items(std::move(value)) {}

    long long asInt() const {
        switch (kind) {
            case Kind::Int: return integer;
            case Kind::Float: return static_cast<long long>(floating);
            case Kind::Bool: return boolean ? 1 : 0;
            case Kind::String:
                try { return std::stoll(text); }
                catch (const std::exception&) { throw std::runtime_error("Cannot read string '" + text + "' as an int"); }
            default: throw std::runtime_error("Cannot read a list as an int");
        }
    }

    double asFloat() const {
        switch (kind) {
            case Kind::Float: return floating;
            case Kind::Int: return static_cast<double>(integer);
            case Kind::Bool: return boolean ? 1.0 : 0.0;
            case Kind::String:
                try { return std::stod(text); }
                catch (const std::exception&) { throw std::runtime_error("Cannot read string '" + text + "' as a float"); }
            default: throw std::runtime_error("Cannot read a list as a float");
        }
    }

    bool asBool() const {
        switch (kind) {
            case Kind::Bool: return boolean;
            case Kind::Int: return integer != 0;
            case Kind::Float: return floating != 0.0;
            case Kind::String: return text == "True" || text == "true";
            default: throw std::runtime_error("Cannot read a list as a bool");
        }
    }

    std::string asString() const {
        if (kind == Kind::String) return text;
        return displayValue(*this);
    }

    const std::vector<Value>& asList() const {
        if (kind != Kind::List) throw std::runtime_error("Cannot read a scalar as a list");
        return items;
    }

    std::vector<Value>& asList() {
        if (kind != Kind::List) throw std::runtime_error("Cannot read a scalar as a list");
        return items;
    }

    std::size_t size() const { return items.size(); }
    bool empty() const { return items.empty(); }
    Value& operator[](std::size_t index) { return items[index]; }
    const Value& operator[](std::size_t index) const { return items[index]; }
    void push_back(Value value) { kind = Kind::List; items.push_back(std::move(value)); }

    // Numeric and string contexts resolve through these conversions, which keeps dynamic values
    // usable in ordinary arithmetic without any explicit casts. operator bool is explicit so that
    // overload resolution for `long long total; total += value;` stays unambiguous.
    operator double() const { return asFloat(); }
    operator std::string() const { return asString(); }
    explicit operator bool() const { return asBool(); }
};

using List = std::vector<Value>;

inline std::string displayValue(const Value& value) {
    switch (value.kind) {
        case Value::Kind::Int: return std::to_string(value.integer);
        case Value::Kind::Float: return std::to_string(value.floating);
        case Value::Kind::Bool: return value.boolean ? "True" : "False";
        case Value::Kind::String: return value.text;
        case Value::Kind::List: {
            std::string result = "[";
            for (std::size_t index = 0; index < value.items.size(); ++index) {
                if (index != 0) result += ", ";
                if (value.items[index].kind == Value::Kind::String) {
                    result += "\"" + value.items[index].text + "\"";
                } else {
                    result += displayValue(value.items[index]);
                }
            }
            return result + "]";
        }
        default: return "null";
    }
}

inline std::ostream& operator<<(std::ostream& stream, const Value& value) {
    stream << displayValue(value);
    return stream;
}
/* --------------------------------------------------------------------------------------------------- */
/*  Binary serialization                                                                               */
/* --------------------------------------------------------------------------------------------------- */

// Every integer is written big-endian so a result file is identical regardless of the machine that
// produced it. Message layout: "SALR", u32 entry count, then { u32 name length, name, value } records.
const char SAL_MAGIC[4] = {'S', 'A', 'L', 'R'};

inline void putByte(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

inline void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

inline void putU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }
}

inline void putText(std::vector<std::uint8_t>& out, const std::string& text) {
    putU32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

inline std::uint8_t getByte(const std::vector<std::uint8_t>& in, std::size_t& position) {
    if (position >= in.size()) throw std::runtime_error("Truncated SAL cpp result payload");
    return in[position++];
}

inline std::uint32_t getU32(const std::vector<std::uint8_t>& in, std::size_t& position) {
    std::uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
        value = (value << 8) | static_cast<std::uint32_t>(getByte(in, position));
    }
    return value;
}

inline std::uint64_t getU64(const std::vector<std::uint8_t>& in, std::size_t& position) {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8) | static_cast<std::uint64_t>(getByte(in, position));
    }
    return value;
}

inline std::string getText(const std::vector<std::uint8_t>& in, std::size_t& position) {
    const std::uint32_t length = getU32(in, position);
    if (position + length > in.size()) throw std::runtime_error("Truncated SAL cpp result string");
    std::string text(reinterpret_cast<const char*>(in.data() + position), length);
    position += length;
    return text;
}

inline void encodeValue(std::vector<std::uint8_t>& out, const Value& value) {
    switch (value.kind) {
        case Value::Kind::Null: putByte(out, 'n'); break;
        case Value::Kind::Int:
            putByte(out, 'i');
            putU64(out, static_cast<std::uint64_t>(value.integer));
            break;
        case Value::Kind::Float: {
            putByte(out, 'f');
            std::uint64_t bits = 0;
            std::memcpy(&bits, &value.floating, sizeof(bits));
            putU64(out, bits);
            break;
        }
        case Value::Kind::Bool:
            putByte(out, 'b');
            putByte(out, value.boolean ? 1u : 0u);
            break;
        case Value::Kind::String:
            putByte(out, 's');
            putText(out, value.text);
            break;
        case Value::Kind::List:
            putByte(out, 'l');
            putU32(out, static_cast<std::uint32_t>(value.items.size()));
            for (const auto& item : value.items) encodeValue(out, item);
            break;
    }
}

inline Value decodeValue(const std::vector<std::uint8_t>& in, std::size_t& position) {
    const char tag = static_cast<char>(getByte(in, position));
    switch (tag) {
        case 'n': return Value{};
        case 'i': return Value(static_cast<long long>(getU64(in, position)));
        case 'f': {
            const std::uint64_t bits = getU64(in, position);
            double number = 0.0;
            std::memcpy(&number, &bits, sizeof(number));
            return Value(number);
        }
        case 'b': return Value(getByte(in, position) != 0);
        case 's': return Value(getText(in, position));
        case 'l': {
            const std::uint32_t count = getU32(in, position);
            std::vector<Value> items;
            items.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index) items.push_back(decodeValue(in, position));
            return Value(std::move(items));
        }
        default: throw std::runtime_error(std::string("Unknown SAL cpp value tag '") + tag + "'");
    }
}

using Entry = std::pair<std::string, Value>;

/**
 * @brief Writes a complete name/value table to disk.
 * @param path The destination file.
 * @param entries The variables to persist.
 */
inline void writeEntries(const std::string& path, const std::vector<Entry>& entries) {
    std::vector<std::uint8_t> payload;
    payload.insert(payload.end(), SAL_MAGIC, SAL_MAGIC + 4);
    putU32(payload, static_cast<std::uint32_t>(entries.size()));
    for (const auto& [name, value] : entries) {
        putText(payload, name);
        encodeValue(payload, value);
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open SAL cpp data file for writing: " + path);
    output.write(reinterpret_cast<const char*>(payload.data()),
                 static_cast<std::streamsize>(payload.size()));
    output.close();
    if (!output) throw std::runtime_error("Could not write SAL cpp data file: " + path);
}

/**
 * @brief Reads a name/value table written by writeEntries.
 * @param path The source file.
 */
inline std::map<std::string, Value> readEntries(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open SAL cpp data file for reading: " + path);
    std::vector<std::uint8_t> payload((std::istreambuf_iterator<char>(input)),
                                      std::istreambuf_iterator<char>());
    if (payload.size() < 8) throw std::runtime_error("Malformed SAL cpp data file: " + path);
    for (int index = 0; index < 4; ++index) {
        if (payload[static_cast<std::size_t>(index)] != static_cast<std::uint8_t>(SAL_MAGIC[index])) {
            throw std::runtime_error("Malformed SAL cpp data file header: " + path);
        }
    }
    std::size_t position = 4;
    const std::uint32_t count = getU32(payload, position);
    std::map<std::string, Value> entries;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::string name = getText(payload, position);
        entries[name] = decodeValue(payload, position);
    }
    return entries;
}

inline std::string environmentPath(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error(std::string("SAL cpp block is missing the environment variable ") + name);
    }
    return std::string(value);
}

/* --------------------------------------------------------------------------------------------------- */
/*  Command-line and stdin access                                                                      */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Exposes the command-line arguments available to a cpp block.
 * @details The dispatch binary forwards every argument that follows the SAL-managed block id,
 *          input path, and output path, so a block sees exactly the args the SAL program was
 *          launched with (minus the SAL interpreter and the .sal filename). Install is called once
 *          by the dispatch main before any block runs; blocks read argc()/argv()/arg() directly.
 */
class CommandArgs {
public:
    static void install(int argc, char** argv) {
        CommandArgs& self = instance();
        self.argc_ = argc;
        self.argv_ = argv;
    }

    static int argc() { return instance().argc_; }

    static char** argv() { return instance().argv_; }

    static std::string arg(int index) {
        const CommandArgs& self = instance();
        if (index < 0 || index >= self.argc_ || self.argv_ == nullptr || self.argv_[index] == nullptr) {
            return std::string{};
        }
        return std::string(self.argv_[index]);
    }

    static std::vector<std::string> all() {
        std::vector<std::string> result;
        const CommandArgs& self = instance();
        for (int index = 0; index < self.argc_; ++index) {
            if (self.argv_ != nullptr && self.argv_[index] != nullptr) {
                result.emplace_back(self.argv_[index]);
            }
        }
        return result;
    }

private:
    static CommandArgs& instance() {
        static CommandArgs args;
        return args;
    }

    CommandArgs() = default;
    int argc_ = 0;
    char** argv_ = nullptr;
};

/**
 * @brief Reads from the terminal stdin available to a cpp block.
 * @details A block that wants interactive or piped input uses these helpers instead of declaring
 *          SAL variables. Nothing is buffered across blocks; each read consumes from the stream.
 */
class Stdin {
public:
    static std::string readLine() {
        std::string line;
        if (!std::getline(std::cin, line)) return std::string{};
        return line;
    }

    static std::string readAll() {
        std::ostringstream buffer;
        buffer << std::cin.rdbuf();
        return buffer.str();
    }

    static bool hasMore() {
        return std::cin.peek() != std::char_traits<char>::eof();
    }

private:
    Stdin() = default;
};
/* --------------------------------------------------------------------------------------------------- */
/*  Generated-block helpers                                                                            */
/* --------------------------------------------------------------------------------------------------- */

/**
 * @brief Reads the SAL variables handed to a generated block.
 * @details The interpreter writes SAL variables to the file named by SAL_CPP_INPUT before the block
 *          runs, so a block simply declares locals from this table.
 */
class Input {
public:
    Input() : Input(environmentPath("SAL_CPP_INPUT")) {}
    explicit Input(const std::string& path) : entries(readEntries(path)) {}

    Value getValue(const std::string& name) const { return require(name); }
    long long getInt(const std::string& name) const { return require(name).asInt(); }
    double getFloat(const std::string& name) const { return require(name).asFloat(); }
    bool getBool(const std::string& name) const { return require(name).asBool(); }
    std::string getString(const std::string& name) const { return require(name).asString(); }
    List getList(const std::string& name) const { return require(name).asList(); }

    std::vector<long long> getIntList(const std::string& name) const {
        std::vector<long long> result;
        for (const auto& item : require(name).asList()) result.push_back(item.asInt());
        return result;
    }

    std::vector<double> getFloatList(const std::string& name) const {
        std::vector<double> result;
        for (const auto& item : require(name).asList()) result.push_back(item.asFloat());
        return result;
    }

    std::vector<bool> getBoolList(const std::string& name) const {
        std::vector<bool> result;
        for (const auto& item : require(name).asList()) result.push_back(item.asBool());
        return result;
    }

    std::vector<std::string> getStringList(const std::string& name) const {
        std::vector<std::string> result;
        for (const auto& item : require(name).asList()) result.push_back(item.asString());
        return result;
    }

    const Value& require(const std::string& name) const {
        auto found = entries.find(name);
        if (found == entries.end()) {
            throw std::runtime_error("SAL cpp block was not given the variable '" + name + "'");
        }
        return found->second;
    }

private:
    std::map<std::string, Value> entries;
};

/**
 * @brief Collects the variables a generated block writes back to SAL.
 * @details Values are buffered and flushed on commit() (or, failing that, on destruction) so that the
 *          entry count at the head of the payload is known before anything is written.
 */
class ResultWriter {
public:
    ResultWriter() : path(environmentPath("SAL_CPP_OUTPUT")) {}
    explicit ResultWriter(std::string destination) : path(std::move(destination)) {}

    ResultWriter(const ResultWriter&) = delete;
    ResultWriter& operator=(const ResultWriter&) = delete;

    ~ResultWriter() {
        try { commit(); }
        catch (const std::exception&) { /* Destruction must never throw. */ }
    }

    void emit(const std::string& name, long long value) { entries.emplace_back(name, Value(value)); }
    void emit(const std::string& name, double value) { entries.emplace_back(name, Value(value)); }
    void emit(const std::string& name, bool value) { entries.emplace_back(name, Value(value)); }
    void emit(const std::string& name, const std::string& value) { entries.emplace_back(name, Value(value)); }
    void emit(const std::string& name, const char* value) { entries.emplace_back(name, Value(std::string(value))); }
    void emit(const std::string& name, const Value& value) { entries.emplace_back(name, value); }

    template <typename T,
              typename std::enable_if<std::is_integral<T>::value &&
                                      !std::is_same<T, bool>::value, int>::type = 0>
    void emit(const std::string& name, T value) {
        entries.emplace_back(name, Value(static_cast<long long>(value)));
    }

    template <typename T,
              typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    void emit(const std::string& name, T value) {
        entries.emplace_back(name, Value(static_cast<double>(value)));
    }

    template <typename T>
    void emit(const std::string& name, const std::vector<T>& values) {
        std::vector<Value> items;
        items.reserve(values.size());
        for (const T& item : values) items.emplace_back(item);
        entries.emplace_back(name, Value(std::move(items)));
    }

    void commit() {
        if (committed) return;
        writeEntries(path, entries);
        committed = true;
    }

private:
    std::string path;
    std::vector<Entry> entries;
    bool committed = false;
};

} // namespace sal
