#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace voicedash {

/**
 * Lightweight vocalization gate. processSamples() is called only from the
 * audio callback. The game thread changes configuration through atomics and
 * reads the active state without locking the real-time callback.
 *
 * This is not speech recognition: sufficiently loud non-vocal sounds can
 * still trigger it.
 */
class VoiceDetector final {
public:
    void configure(float activationThreshold,
                   float releaseThreshold,
                   std::uint32_t activationMs,
                   std::uint32_t releaseMs) noexcept;

    bool processSamples(const std::int16_t* samples,
                        std::size_t frameCount,
                        std::uint32_t sampleRate) noexcept;

    [[nodiscard]] bool isActive() const noexcept {
        return m_active.load(std::memory_order_acquire);
    }

    [[nodiscard]] float lastRms() const noexcept {
        return m_lastRms.load(std::memory_order_relaxed);
    }

    // Call only after the audio stream is stopped and its callback has ended.
    void forceRelease() noexcept;

private:
    std::atomic<bool> m_active{false};
    std::atomic<float> m_activationThreshold{0.035f};
    std::atomic<float> m_releaseThreshold{0.020f};
    std::atomic<std::uint32_t> m_activationMs{25};
    std::atomic<std::uint32_t> m_releaseMs{60};
    std::atomic<float> m_lastRms{0.0f};

    // Audio-callback-thread-only state.
    float m_noiseFloor = 0.002f;
    std::uint64_t m_aboveThresholdFrames = 0;
    std::uint64_t m_belowThresholdFrames = 0;
};

} // namespace voicedash
