#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace voicedash {

/**
 * Low-cost voiced-sound detector for the real-time audio callback.
 *
 * RMS and short-window coverage gate out brief loud noises; a decimated,
 * normalized autocorrelation score favours sustained vocal sounds such as
 * vowels and humming. This is not speech recognition and cannot reject every
 * tonal environmental sound.
 */
class VoiceDetector final {
public:
    void configure(float activationThreshold,
                   float releaseThreshold,
                   float minimumPeriodicity,
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

    [[nodiscard]] float periodicity() const noexcept {
        return m_periodicity.load(std::memory_order_relaxed);
    }

    // Call only after the audio stream is stopped and its callback has ended.
    void forceRelease() noexcept;

private:
    static constexpr std::size_t kWindowCapacity = 256;
    static constexpr std::size_t kCoverageBuckets = 4; // 4 x 5 ms ~= 20 ms

    std::atomic<bool> m_active{false};
    std::atomic<float> m_activationThreshold{0.035f};
    std::atomic<float> m_releaseThreshold{0.020f};
    std::atomic<float> m_minimumPeriodicity{0.22f};
    std::atomic<std::uint32_t> m_activationMs{25};
    std::atomic<std::uint32_t> m_releaseMs{60};
    std::atomic<float> m_lastRms{0.0f};
    std::atomic<float> m_periodicity{0.0f};

    // All fields below are audio-callback-thread-only. A rolling, decimated
    // window allows periodicity updates without waiting for a whole new frame.
    std::array<std::int16_t, kWindowCapacity> m_periodicityWindow{};
    std::size_t m_periodicityCount = 0;
    std::size_t m_periodicityWrite = 0;
    std::uint32_t m_currentSampleRate = 0;
    std::uint32_t m_decimationStride = 4;
    std::uint32_t m_decimationPhase = 0;
    std::uint32_t m_periodicityUpdateFrames = 0;

    double m_subframeSquareSum = 0.0;
    std::uint32_t m_subframeSamples = 0;
    std::array<bool, kCoverageBuckets> m_coverageHistory{};
    std::size_t m_coverageWrite = 0;
    std::size_t m_coverageCount = 0;
    std::uint32_t m_loudSubframes = 0;

    float m_noiseFloor = 0.002f;
    std::uint64_t m_aboveThresholdFrames = 0;
    std::uint64_t m_belowThresholdFrames = 0;
};

} // namespace voicedash
