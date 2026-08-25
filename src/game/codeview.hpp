#pragma once

#include <cstddef>
#include <cstdint>

#include <windows.h>

namespace mhs::codeview {

// the executable's own code section, the only range a call scan may look at
inline bool Code(std::uintptr_t& start, std::size_t& size) {
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleA(nullptr));
    if (!base) {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    start = reinterpret_cast<std::uintptr_t>(base) + nt->OptionalHeader.BaseOfCode;
    size  = nt->OptionalHeader.SizeOfCode;
    return size != 0;
}

} // namespace mhs::codeview
