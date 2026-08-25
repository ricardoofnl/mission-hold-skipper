#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

#include <windows.h>

namespace mhs::log {

enum Level { kDebug, kInfo, kWarn, kError };

inline std::string& Path() {
    static std::string path;
    return path;
}

inline Level& MinLevel() {
    static Level level = kInfo;
    return level;
}

inline void SetMinLevel(const std::string& name) {
    if (name == "debug") {
        MinLevel() = kDebug;
    } else if (name == "warn") {
        MinLevel() = kWarn;
    } else if (name == "error") {
        MinLevel() = kError;
    } else {
        MinLevel() = kInfo;
    }
}

inline void Init(HMODULE self) {
    char       buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(self, buffer, MAX_PATH);
    std::string path(buffer, length);
    const auto  dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        path.erase(dot);
    }
    Path() = path + ".log";

    if (FILE* file = std::fopen(Path().c_str(), "w")) {
        std::fclose(file);
    }
}

inline void Write(Level level, const char* format, ...) {
    if (level < MinLevel() || Path().empty()) {
        return;
    }

    static const char* names[]{"debug", "info", "warn", "error"};

    FILE* file = std::fopen(Path().c_str(), "a");
    if (!file) {
        return;
    }
    std::fprintf(file, "[%s] ", names[level]);

    va_list args;
    va_start(args, format);
    std::vfprintf(file, format, args);
    va_end(args);

    std::fputc('\n', file);
    std::fclose(file);
}

} // namespace mhs::log

#define MHS_LOG_DEBUG(...) ::mhs::log::Write(::mhs::log::kDebug, __VA_ARGS__)
#define MHS_LOG_INFO(...) ::mhs::log::Write(::mhs::log::kInfo, __VA_ARGS__)
#define MHS_LOG_WARN(...) ::mhs::log::Write(::mhs::log::kWarn, __VA_ARGS__)
#define MHS_LOG_ERROR(...) ::mhs::log::Write(::mhs::log::kError, __VA_ARGS__)
