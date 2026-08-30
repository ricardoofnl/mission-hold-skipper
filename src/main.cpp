#include <cstdio>
#include <string>

#include <windows.h>

#include "config.hpp"
#include "game/bindings.hpp"
#include "ginput.hpp"
#include "log.hpp"

#ifndef MHS_VERSION
#define MHS_VERSION "dev"
#endif

namespace {

std::string ModulePathWithExtension(HMODULE module, const char* extension) {
    char        buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(module, buffer, MAX_PATH);
    std::string path(buffer, length);
    if (const auto dot = path.find_last_of('.'); dot != std::string::npos) {
        path.erase(dot);
    }
    return path + extension;
}

// two copies in one process fight over the same call sites, and the loser sees
// them already patched and reports the game as unsupported
bool AnotherCopyLoaded() {
    char name[64]{};
    std::snprintf(name, sizeof(name), "MissionHoldSkipper-%lu", GetCurrentProcessId());
    static HANDLE guard = CreateMutexA(nullptr, TRUE, name);
    return guard == nullptr || GetLastError() == ERROR_ALREADY_EXISTS;
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

    MHS_LOG_INFO("mission-hold-skipper %s loading, built for %s %s", MHS_VERSION,
                 mhs::game::kGameName, mhs::game::kGameVersion);

    if (!mhs::Cfg().enabled) {
        MHS_LOG_INFO("disabled by config, no hooks installed");
        return TRUE;
    }
    if (AnotherCopyLoaded()) {
        MHS_LOG_ERROR("another copy of this plugin is already loaded in this process, "
                      "keep only one .asi and delete the rest");
        return TRUE;
    }
    if (!mhs::game::VersionMatches()) {
        MHS_LOG_ERROR("this build only supports %s %s, staying inactive",
                      mhs::game::kGameName, mhs::game::kGameVersion);
        return TRUE;
    }

    MHS_LOG_INFO("version check ok, screen %.0fx%.0f", mhs::game::ScreenWidth(),
                 mhs::game::ScreenHeight());
    if (const char* ginput = mhs::ginput::LoadedModuleName()) {
        MHS_LOG_INFO("%s detected, pad input arrives through it", ginput);
    }
    if (mhs::game::InstallHooks()) {
        MHS_LOG_INFO("hooks installed, hold %u ms to skip", mhs::Cfg().holdMs);
    }
    return TRUE;
}
