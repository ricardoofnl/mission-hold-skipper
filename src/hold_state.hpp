#pragma once

#include <algorithm>
#include <cstdint>

namespace mhs {

// pure hold timer, no game memory, so it can be exercised in a host test
class HoldState {
public:
    void SetHoldMs(std::uint32_t ms) { m_holdMs = ms == 0 ? 1 : ms; }

    void Update(bool available, bool keyDown, std::uint32_t deltaMs) {
        m_available = available;

        if (!available) {
            Reset();
            return;
        }
        if (!keyDown) {
            Reset();
            return;
        }

        m_heldMs += deltaMs;
        if (!m_latched && m_heldMs >= m_holdMs) {
            m_completed = true;
            m_latched   = true;
        }
    }

    // true once per completed hold, the caller acts on it
    bool ConsumeCompleted() {
        const bool completed = m_completed;
        m_completed          = false;
        return completed;
    }

    float Progress() const {
        return std::min(1.0f, static_cast<float>(m_heldMs) / static_cast<float>(m_holdMs));
    }

    bool Available() const { return m_available; }
    bool Holding() const { return m_heldMs > 0; }

private:
    // releasing the key drops a completion that nobody consumed yet, otherwise a
    // late skip could fire after the player already let go
    void Reset() {
        m_heldMs    = 0;
        m_latched   = false;
        m_completed = false;
    }

    std::uint32_t m_holdMs{1200};
    std::uint32_t m_heldMs{0};
    bool          m_available{false};
    bool          m_latched{false};
    bool          m_completed{false};
};

// show and hide easing for the prompt, also pure so the test can drive it
class FadeAnim {
public:
    void Configure(std::uint32_t inMs, std::uint32_t outMs) {
        m_inMs  = inMs == 0 ? 1 : inMs;
        m_outMs = outMs == 0 ? 1 : outMs;
    }

    void Update(bool visible, std::uint32_t deltaMs) {
        const float duration = static_cast<float>(visible ? m_inMs : m_outMs);
        const float step     = static_cast<float>(deltaMs) / duration;
        m_value              = std::clamp(visible ? m_value + step : m_value - step, 0.0f, 1.0f);
    }

    float Value() const { return m_value; }

    // ease out cubic, the usual pop in and settle
    float Eased() const {
        const float inverse = 1.0f - m_value;
        return 1.0f - inverse * inverse * inverse;
    }

    bool Visible() const { return m_value > 0.001f; }

private:
    float         m_value{0.0f};
    std::uint32_t m_inMs{140};
    std::uint32_t m_outMs{220};
};

} // namespace mhs