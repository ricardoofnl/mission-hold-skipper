#include <cstdint>
#include <cstring>
#include <string>

#include <windows.h>

#include "config.hpp"
#include "ginput.hpp"
#include "hold_skip.hpp"
#include "hook.hpp"
#include "log.hpp"
#include "sa/addresses.hpp"
#include "sa/game.hpp"

#ifndef MHS_VERSION
#define MHS_VERSION "dev"
#endif

namespace {

namespace addr = mhs::sa::addr;

bool __cdecl HookedIsSkipButtonPressed() {
    mhs::hold_skip::TickOncePerFrame();
    // vanilla auto skips while the game is in the background, keep that
    return mhs::hold_skip::ConsumeCompleted() || !mhs::sa::IsForeground();
}

// another input mod may replace the same function, and the last one to load wins
void VerifySkipHook() {
    const auto* at = reinterpret_cast<const std::uint8_t*>(addr::IsCutsceneSkipButtonBeingPressed);
    if (at[0] == 0xE9) {
        std::int32_t relative{};
        std::memcpy(&relative, at + 1, sizeof(relative));
        const auto target = addr::IsCutsceneSkipButtonBeingPressed + 5 + static_cast<std::uintptr_t>(relative);
        if (target == reinterpret_cast<std::uintptr_t>(&HookedIsSkipButtonPressed)) {
            return;
        }
    }
    MHS_LOG_WARN("something replaced the skip button hook, hold to skip is inactive");
}

void __cdecl HookedCHudDraw() {
    mhs::sa::CHudDraw();

    // by the first frame every other .asi has had its turn at patching, and
    // GInput, which must never be touched from DllMain, is up
    static bool verified = false;
    if (!verified) {
        verified = true;
        VerifySkipHook();
        if (auto* pad = mhs::ginput::Pad()) {
            MHS_LOG_INFO("GInput API in use, version 0x%06X, pad connected %d",
                         pad->GetVersion(), pad->IsPadConnected() ? 1 : 0);
        }
    }

    mhs::hold_skip::TickOncePerFrame();
    mhs::hold_skip::Draw();
}

std::string ModulePathWithExtension(HMODULE module, const char* extension) {
    char        buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
    std::string path(buffer, length);
    if (const auto dot = path.find_last_of('.'); dot != std::string::npos) {
        path.erase(dot);
    }
    return path + extension;
}

bool Readable(std::uintptr_t at, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(at), &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    if (info.State != MEM_COMMIT) {
        return false;
    }
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return at + size <= end;
}

struct Signature {
    const char*         what;
    std::uintptr_t      at;
    const std::uint8_t* bytes;
    std::size_t         size;
};

bool VersionMatches() {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    if (base != 0x400000) {
        MHS_LOG_ERROR("unexpected image base 0x%08X, expected 0x00400000", base);
        return false;
    }

    static const std::uint8_t kSkipButton[]{0x6A, 0x00, 0xE8, 0x59, 0x9E, 0x06, 0x00, 0x83};
    static const std::uint8_t kScriptProcess[]{0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xD8, 0x00};
    static const std::uint8_t kHudDraw[]{0x80, 0x3D, 0x88, 0x30, 0xA4, 0x00, 0x01, 0x0F};
    static const std::uint8_t kHudDrawCall[]{0xE8, 0xDC, 0x15, 0x05, 0x00};

    const Signature signatures[]{
        {"IsCutsceneSkipButtonBeingPressed", addr::IsCutsceneSkipButtonBeingPressed, kSkipButton, sizeof(kSkipButton)},
        {"CRunningScript::Process", addr::CRunningScript_Process, kScriptProcess, sizeof(kScriptProcess)},
        {"CHud::Draw", addr::CHud_Draw, kHudDraw, sizeof(kHudDraw)},
        {"Render2dStuff call to CHud::Draw", addr::Render2dStuff_CallCHudDraw, kHudDrawCall, sizeof(kHudDrawCall)},
    };

    for (const auto& signature : signatures) {
        if (!Readable(signature.at, signature.size)) {
            MHS_LOG_ERROR("cannot read %s at 0x%08X", signature.what, signature.at);
            return false;
        }
        if (!mhs::hook::BytesMatch(signature.at, signature.bytes, signature.size)) {
            MHS_LOG_ERROR("byte mismatch for %s at 0x%08X", signature.what, signature.at);
            return false;
        }
    }
    return true;
}

void Install() {
    if (!mhs::hook::MakeJmp(addr::IsCutsceneSkipButtonBeingPressed, &HookedIsSkipButtonPressed)) {
        MHS_LOG_ERROR("failed to hook IsCutsceneSkipButtonBeingPressed");
        return;
    }
    if (!mhs::hook::RedirectCall(addr::Render2dStuff_CallCHudDraw, &HookedCHudDraw)) {
        MHS_LOG_ERROR("failed to redirect the CHud::Draw call");
        return;
    }
    MHS_LOG_INFO("hooks installed, hold %u ms to skip", mhs::Cfg().holdMs);
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    DisableThreadLibraryCalls(module);

    mhs::log::Init(module);
    mhs::LoadConfig(ModulePathWithExtension(module, ".ini"));
    mhs::log::SetMinLevel(mhs::Cfg().logLevel);

    MHS_LOG_INFO("mission-hold-skipper %s loading", MHS_VERSION);

    if (!mhs::Cfg().enabled) {
        MHS_LOG_INFO("disabled by config, no hooks installed");
        return TRUE;
    }
    if (!VersionMatches()) {
        MHS_LOG_ERROR("this build only supports gta_sa.exe v1.0 US, staying inactive");
        return TRUE;
    }

    MHS_LOG_INFO("version check ok, screen %.0fx%.0f", mhs::sa::ScreenWidth(), mhs::sa::ScreenHeight());
    if (GetModuleHandleA("GInputSA.asi")) {
        MHS_LOG_INFO("GInputSA detected, pad input arrives through it");
    }
    Install();
    return TRUE;
}
