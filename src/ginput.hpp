#pragma once

#include <cstdint>

#include <windows.h>

// GInput ships GInputAPI.h for modders. only the vtable layout and the export
// ordinal are reproduced here, none of their code
namespace mhs::ginput {

class IPad {
public:
    virtual ~IPad()                                    = default;
    virtual bool  IsPadConnected() const               = 0;
    virtual bool  HasPadInHands() const                = 0;
    virtual int   GetVersion() const                   = 0;
    virtual void* SendEvent(int event, void* param)    = 0;
    virtual void* SendConstEvent(int event, void* param) const = 0;
};

constexpr int kFetchGeneralSettings = 6;

#pragma pack(push, 4)
struct GeneralSettings {
    std::uint32_t cbSize;
    bool          disableOnFocusLost : 1;
    bool          vibration : 1;
    bool          cheatsFromPad : 1;
    bool          guideLaunchesOverlay : 1;
    bool          applyMissionSpecificFixes : 1;
    bool          applyGxtFixes : 1;
    bool          playStationButtons : 1;
    bool          mapPadOneToPadTwo : 1;
    bool          freeAim : 1;
};
#pragma pack(pop)

inline constexpr const char* kModules[]{"GInputIII.asi", "GInputVC.asi", "GInputSA.asi"};

// which GInput build is loaded, null when none is
inline const char* LoadedModuleName() {
    for (const char* name : kModules) {
        if (GetModuleHandleA(name)) {
            return name;
        }
    }
    return nullptr;
}

// never call this from DllMain, GInput may not be initialised yet
inline IPad* Pad() {
    static IPad* cached  = nullptr;
    static bool  resolved = false;
    if (resolved) {
        return cached;
    }
    resolved = true;

    const char* name = LoadedModuleName();
    if (!name) {
        return nullptr;
    }
    HMODULE module = GetModuleHandleA(name);
    if (!module) {
        return nullptr;
    }
    using Export = IPad*(__cdecl*)();
    auto* entry  = reinterpret_cast<Export>(GetProcAddress(module, reinterpret_cast<const char*>(1)));
    if (!entry) {
        return nullptr;
    }
    cached = entry();
    return cached;
}

inline bool PlayStationButtons() {
    IPad* pad = Pad();
    if (!pad) {
        return false;
    }
    GeneralSettings settings{};
    settings.cbSize = sizeof(settings);
    pad->SendConstEvent(kFetchGeneralSettings, &settings);
    return settings.playStationButtons;
}

} // namespace mhs::ginput
