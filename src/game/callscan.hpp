#pragma once

#include <cstddef>
#include <cstdint>

namespace mhs::callscan {

struct Result {
    std::size_t    count{0};
    std::uintptr_t at{0};
};

// III and VC hide the pieces we patch inside bigger functions, so the call site
// is searched for instead of hardcoded
inline Result FindCalls(const std::uint8_t* code, std::size_t size,
                        std::uintptr_t codeAddress, std::uintptr_t callee) {
    Result result{};
    if (!code || size < 5) {
        return result;
    }
    for (std::size_t i = 0; i + 5 <= size; ++i) {
        if (code[i] != 0xE8) {
            continue;
        }
        std::int32_t relative{};
        for (int b = 0; b < 4; ++b) {
            relative |= static_cast<std::int32_t>(code[i + 1 + b]) << (8 * b);
        }
        const auto here   = codeAddress + i;
        const auto target = here + 5 + static_cast<std::uintptr_t>(relative);
        if (target != callee) {
            continue;
        }
        if (result.count == 0) {
            result.at = here;
        }
        ++result.count;
    }
    return result;
}

} // namespace mhs::callscan
