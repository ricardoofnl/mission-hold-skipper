#pragma once

#include <string>

namespace mhs {

enum class PromptDevice {
    Auto,
    Keyboard,
    Pad,
};

class DeviceTracker {
public:
    void SetForced(PromptDevice forced) { m_forced = forced; }

    void Update(bool keyboardActive, bool padActive) {
        // keyboard wins a tie, a pad left resting on a stick must not steal the prompt
        if (keyboardActive) {
            m_pad = false;
        } else if (padActive) {
            m_pad = true;
        }
    }

    bool PadPrompt() const {
        switch (m_forced) {
        case PromptDevice::Keyboard: return false;
        case PromptDevice::Pad: return true;
        default: return m_pad;
        }
    }

private:
    PromptDevice m_forced{PromptDevice::Auto};
    bool         m_pad{false};
};

inline const std::string& PickLabel(bool padPrompt, const std::string& keyboard,
                                   const std::string& pad) {
    return padPrompt && !pad.empty() ? pad : keyboard;
}

} // namespace mhs
