#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace voicedash {

/**
 * Lightweight voice-activity gate.
 *
 * processSamples() is called only from the audio callback. Configuration and
 * output state use atomics so the game thread never needs to lock the callback.
 * This is an amplitude-based vocalization detector, not speech recognition and
 * not a guarantee that every detected sound is human voice.
 */
class VoiceDetector final {
public:
    void configure(float activationThreshold,
                   std::uint32_t activationMs,
                   std::uint32_t releaseMs) noexcept;

    bool processSamples(const std::int16_t* samples,
                        std::size_t frameCount,
                        std::uint32_t sampleRate) noexcept;

    [[nodiscard]] bool isActive() const noexcept {
        return m_active.load(std::memory_order_acquire);
    }

    // Call only after the audio stream has stopped and its callback has ended.
    void forceRelease() noexcept;

private:
    std::atomic<bool> m_active{false};
    std::atomic<float> m_activationThreshold{0.018f};
    std::atomic<std::uint32_t> m_activationMs{25};
    std::atomic<std::uint32_t> m_releaseMs{60};

    // Callback-thread-only state; never touched by the game thread while running.
    float m_noiseFloor = 0.002f;
    std::uint64_t m_aboveThresholdFrames = 0;
    std::uint64_t m_belowThresholdFrames = 0;
};

} // namespace voicedash
