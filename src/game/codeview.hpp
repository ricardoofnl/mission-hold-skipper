#pragma once

#include <cstddef>
#include <cstdint>

#include <windows.h>

namespace mhs::codeview {

namespace detail {
inline const IMAGE_NT_HEADERS32* Headers() {
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleA(nullptr));
    if (!base) {
        return nullptr;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
}
} // namespace detail

// the executable's own code section, the only range a call scan may look at
inline bool Code(std::uintptr_t& start, std::size_t& size) {
    const auto* nt = detail::Headers();
    if (!nt) {
        return false;
    }
    start = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)) + nt->OptionalHeader.BaseOfCode;
    size  = nt->OptionalHeader.SizeOfCode;
    return size != 0;
}

// enough to tell two game builds apart in a log
inline bool Identity(std::uint32_t& imageSize, std::uint32_t& timestamp) {
    const auto* nt = detail::Headers();
    if (!nt) {
        return false;
    }
    imageSize = nt->OptionalHeader.SizeOfImage;
    timestamp = nt->FileHeader.TimeDateStamp;
    return true;
}

} // namespace mhs::codeview
