#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace voicedash {

struct DetectorConfig {
    // Normalized RMS values in [0, 1]. Tune these on the target device.
    float activationThreshold = 0.035f;
    float releaseThreshold = 0.020f;
    float noiseMultiplier = 2.8f;
    float maxNoiseFloor = 0.025f;
    uint32_t attackMs = 18;
    uint32_t releaseMs = 85;
};

class VoiceDetector {
public:
    void configure(DetectorConfig config);
    void reset() noexcept;

    // Thread-safe snapshot consumed by the game thread.
    bool isActive() const noexcept { return m_active.load(std::memory_order_acquire); }
    float lastRms() const noexcept { return m_lastRms.load(std::memory_order_relaxed); }
    float noiseFloor() const noexcept { return m_noiseFloor.load(std::memory_order_relaxed); }

    // Called from the audio worker. PCM is signed 16-bit mono.
    void process(const int16_t* samples, size_t count, uint32_t sampleRate) noexcept;

private:
    DetectorConfig m_config{};
    std::atomic<bool> m_active{false};
    std::atomic<float> m_lastRms{0.f};
    std::atomic<float> m_noiseFloor{0.004f};
    uint64_t m_attackSamples = 0;
    uint64_t m_releaseSamples = 0;
};

} // namespace voicedash
