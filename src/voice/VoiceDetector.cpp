#include "VoiceDetector.hpp"

#include <algorithm>
#include <cmath>

namespace voicedash {

void VoiceDetector::configure(float activationThreshold,
                              float releaseThreshold,
                              std::uint32_t activationMs,
                              std::uint32_t releaseMs) noexcept {
    if (!std::isfinite(activationThreshold)) {
        activationThreshold = 0.035f;
    }
    if (!std::isfinite(releaseThreshold)) {
        releaseThreshold = 0.020f;
    }

    activationThreshold = std::clamp(activationThreshold, 0.001f, 0.5f);
    // Enforce hysteresis: silence threshold can never exceed the activation
    // threshold, even if a malformed setting is received.
    releaseThreshold = std::clamp(releaseThreshold, 0.0005f, activationThreshold);
    activationMs = std::clamp<std::uint32_t>(activationMs, 5, 500);
    releaseMs = std::clamp<std::uint32_t>(releaseMs, 5, 500);

    m_activationThreshold.store(activationThreshold, std::memory_order_relaxed);
    m_releaseThreshold.store(releaseThreshold, std::memory_order_relaxed);
    m_activationMs.store(activationMs, std::memory_order_relaxed);
    m_releaseMs.store(releaseMs, std::memory_order_relaxed);
}

bool VoiceDetector::processSamples(const std::int16_t* samples,
                                   std::size_t frameCount,
                                   std::uint32_t sampleRate) noexcept {
    if (samples == nullptr || frameCount == 0 || sampleRate == 0) {
        return isActive();
    }

    // RMS calculation is bounded by the current audio callback size and does
    // not allocate, log, or take a mutex.
    double squareSum = 0.0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        const double value = static_cast<double>(samples[i]) / 32768.0;
        squareSum += value * value;
    }
    const float rms = static_cast<float>(
        std::sqrt(squareSum / static_cast<double>(frameCount))
    );
    m_lastRms.store(rms, std::memory_order_relaxed);

    const float configuredOn =
        m_activationThreshold.load(std::memory_order_relaxed);
    const float configuredOff =
        m_releaseThreshold.load(std::memory_order_relaxed);

    bool active = m_active.load(std::memory_order_relaxed);

    // Adapt slowly from quiet frames only. The floor is bounded and isn't
    // updated while a vocalization is active, so sustained voice isn't learned
    // as background noise.
    if (!active && rms < configuredOn * 0.75f) {
        const float updated = m_noiseFloor * 0.985f + rms * 0.015f;
        m_noiseFloor = std::clamp(updated, 0.0002f, configuredOn * 0.70f);
    }

    const float onThreshold = std::max(configuredOn, m_noiseFloor * 2.2f);
    const float offThreshold = std::min(
        onThreshold,
        std::max(configuredOff, m_noiseFloor * 1.65f)
    );

    const auto attackNeeded = std::max<std::uint64_t>(
        1u, static_cast<std::uint64_t>(sampleRate) *
        m_activationMs.load(std::memory_order_relaxed) / 1000u
    );
    const auto releaseNeeded = std::max<std::uint64_t>(
        1u, static_cast<std::uint64_t>(sampleRate) *
        m_releaseMs.load(std::memory_order_relaxed) / 1000u
    );

    if (!active) {
        if (rms >= onThreshold) {
            m_aboveThresholdFrames += frameCount;
            m_belowThresholdFrames = 0;
            if (m_aboveThresholdFrames >= attackNeeded) {
                active = true;
                m_active.store(true, std::memory_order_release);
                m_belowThresholdFrames = 0;
            }
        } else {
            m_aboveThresholdFrames = 0;
        }
    } else if (rms < offThreshold) {
        m_belowThresholdFrames += frameCount;
        if (m_belowThresholdFrames >= releaseNeeded) {
            active = false;
            m_active.store(false, std::memory_order_release);
            m_aboveThresholdFrames = 0;
            m_belowThresholdFrames = 0;
        }
    } else {
        m_belowThresholdFrames = 0;
    }

    return active;
}

void VoiceDetector::forceRelease() noexcept {
    m_active.store(false, std::memory_order_release);
    m_lastRms.store(0.0f, std::memory_order_relaxed);
    m_noiseFloor = 0.002f;
    m_aboveThresholdFrames = 0;
    m_belowThresholdFrames = 0;
}

} // namespace voicedash
