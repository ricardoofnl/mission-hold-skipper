#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <windows.h>

namespace mhs::hook {

inline bool WriteBytes(std::uintptr_t at, const void* src, std::size_t n) {
    auto* dst = reinterpret_cast<void*>(at);
    DWORD  previous{};
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &previous)) {
        return false;
    }
    std::memcpy(dst, src, n);
    VirtualProtect(dst, n, previous, &previous);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return true;
}

inline bool BytesMatch(std::uintptr_t at, const std::uint8_t* expected, std::size_t n) {
    return std::memcmp(reinterpret_cast<const void*>(at), expected, n) == 0;
}

// replaces the function entry, the original becomes unreachable
inline bool MakeJmp(std::uintptr_t at, const void* target) {
    std::uint8_t patch[5]{0xE9};
    const auto   rel = reinterpret_cast<std::uintptr_t>(target) - (at + 5);
    std::memcpy(patch + 1, &rel, 4);
    return WriteBytes(at, patch, sizeof(patch));
}

inline bool RedirectCall(std::uintptr_t at, const void* target) {
    if (*reinterpret_cast<const std::uint8_t*>(at) != 0xE8) {
        return false;
    }
    std::uint8_t patch[5]{0xE8};
    const auto   rel = reinterpret_cast<std::uintptr_t>(target) - (at + 5);
    std::memcpy(patch + 1, &rel, 4);
    return WriteBytes(at, patch, sizeof(patch));
}

} // namespace mhs::hook
