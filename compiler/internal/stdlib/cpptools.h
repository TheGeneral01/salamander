/* =================================================================================================== */
/*                                                                                                     */
/*  Module: cpptools.h                                                                                 */
/*  Description: Provides typed C++ file operations for SAL and native C++ blocks.                     */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpptools {

struct FileHandle {
    std::fstream stream;
    bool binary = false;
    bool readable = false;
    bool writable = false;

    ~FileHandle() {
        if (stream.is_open()) stream.close();
    }
};

using File = std::shared_ptr<FileHandle>;

inline File open(const std::string& path, const std::string& mode) {
    auto file = std::make_shared<FileHandle>();
    std::ios::openmode flags = std::ios::in;
    if (mode == "r" || mode == "rt") {
        file->readable = true;
    } else if (mode == "rb") {
        file->readable = true;
        file->binary = true;
        flags |= std::ios::binary;
    } else if (mode == "w" || mode == "wt") {
        flags = std::ios::out | std::ios::trunc;
        file->writable = true;
    } else if (mode == "wb") {
        flags = std::ios::out | std::ios::trunc | std::ios::binary;
        file->writable = true;
        file->binary = true;
    } else if (mode == "a" || mode == "at") {
        flags = std::ios::out | std::ios::app;
        file->writable = true;
    } else if (mode == "ab") {
        flags = std::ios::out | std::ios::app | std::ios::binary;
        file->writable = true;
        file->binary = true;
    } else if (mode == "r+") {
        flags = std::ios::in | std::ios::out;
        file->readable = true;
        file->writable = true;
    } else if (mode == "r+b" || mode == "rb+") {
        flags = std::ios::in | std::ios::out | std::ios::binary;
        file->readable = true;
        file->writable = true;
        file->binary = true;
    } else if (mode == "w+") {
        flags = std::ios::in | std::ios::out | std::ios::trunc;
        file->readable = true;
        file->writable = true;
    } else if (mode == "w+b" || mode == "wb+") {
        flags = std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary;
        file->readable = true;
        file->writable = true;
        file->binary = true;
    } else {
        throw std::runtime_error("Unsupported cpptools file mode: " + mode);
    }
    file->stream.open(path, flags);
    if (!file->stream.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    return file;
}

inline void requireOpen(const File& file) {
    if (!file || !file->stream.is_open()) throw std::runtime_error("File handle is closed");
}

inline std::string readLine(File file) {
    requireOpen(file);
    if (!file->readable) throw std::runtime_error("File was not opened for reading");
    std::string line;
    if (!std::getline(file->stream, line)) return {};
    return line;
}

inline bool eof(File file) {
    requireOpen(file);
    return file->stream.eof();
}

inline std::string readText(File file) {
    requireOpen(file);
    if (!file->readable) throw std::runtime_error("File was not opened for reading");
    return std::string(std::istreambuf_iterator<char>(file->stream),
                       std::istreambuf_iterator<char>());
}

inline void writeText(File file, const std::string& text) {
    requireOpen(file);
    if (!file->writable) throw std::runtime_error("File was not opened for writing");
    file->stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file->stream) throw std::runtime_error("Failed to write text to file");
}

inline std::vector<uint8_t> readBytes(File file, std::size_t count) {
    requireOpen(file);
    if (!file->readable || !file->binary) throw std::runtime_error("File must be opened in binary read mode");
    std::vector<uint8_t> bytes(count);
    file->stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(count));
    bytes.resize(static_cast<std::size_t>(file->stream.gcount()));
    return bytes;
}

inline void writeBytes(File file, const std::vector<uint8_t>& bytes) {
    requireOpen(file);
    if (!file->writable || !file->binary) throw std::runtime_error("File must be opened in binary write mode");
    file->stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file->stream) throw std::runtime_error("Failed to write bytes to file");
}

inline void close(File file) {
    requireOpen(file);
    file->stream.close();
}

inline bool exists(const std::string& path) {
    return std::filesystem::exists(path);
}

inline int64_t size(const std::string& path) {
    return static_cast<int64_t>(std::filesystem::file_size(path));
}

inline bool remove(const std::string& path) {
    return std::filesystem::remove(path);
}

inline void rename(const std::string& from, const std::string& to) {
    std::filesystem::rename(from, to);
}

} // namespace cpptools
