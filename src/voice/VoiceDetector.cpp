#include "VoiceDetector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace voicedash {

void VoiceDetector::configure(float activationThreshold,
                              std::uint32_t activationMs,
                              std::uint32_t releaseMs) noexcept {
    if (!std::isfinite(activationThreshold)) {
        activationThreshold = 0.018f;
    }

    activationThreshold = std::clamp(activationThreshold, 0.002f, 0.25f);
    activationMs = std::clamp<std::uint32_t>(activationMs, 5, 500);
    releaseMs = std::clamp<std::uint32_t>(releaseMs, 5, 500);

    m_activationThreshold.store(activationThreshold, std::memory_order_relaxed);
    m_activationMs.store(activationMs, std::memory_order_relaxed);
    m_releaseMs.store(releaseMs, std::memory_order_relaxed);
}

bool VoiceDetector::processSamples(const std::int16_t* samples,
                                   std::size_t frameCount,
                                   std::uint32_t sampleRate) noexcept {
    if (samples == nullptr || frameCount == 0 || sampleRate == 0) {
        return isActive();
    }

    double squareSum = 0.0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        const double normalized = static_cast<double>(samples[i]) / 32768.0;
        squareSum += normalized * normalized;
    }

    const float rms = static_cast<float>(
        std::sqrt(squareSum / static_cast<double>(frameCount))
    );

    const float configuredThreshold =
        m_activationThreshold.load(std::memory_order_relaxed);
    const float activationThreshold =
        std::max(configuredThreshold, m_noiseFloor * 2.2f);
    const float releaseThreshold =
        std::max(configuredThreshold * 0.55f, m_noiseFloor * 1.65f);

    bool active = m_active.load(std::memory_order_relaxed);

    if (!active) {
        if (rms >= activationThreshold) {
            m_aboveThresholdFrames += frameCount;
            m_belowThresholdFrames = 0;

            const auto requiredFrames =
                static_cast<std::uint64_t>(sampleRate) *
                m_activationMs.load(std::memory_order_relaxed) / 1000u;

            if (m_aboveThresholdFrames >= std::max<std::uint64_t>(requiredFrames, 1u)) {
                active = true;
                m_active.store(true, std::memory_order_release);
                m_belowThresholdFrames = 0;
            }
        } else {
            m_aboveThresholdFrames = 0;

            // Adapt slowly only during clearly quiet frames. Never let the
            // estimated background level raise the threshold without a bound.
            if (rms < configuredThreshold * 0.75f) {
                const float updated = m_noiseFloor * 0.985f + rms * 0.015f;
                const float maxFloor = configuredThreshold * 0.70f;
                m_noiseFloor = std::clamp(updated, 0.0002f, maxFloor);
            }
        }
    } else {
        if (rms < releaseThreshold) {
            m_belowThresholdFrames += frameCount;
            const auto requiredFrames =
                static_cast<std::uint64_t>(sampleRate) *
                m_releaseMs.load(std::memory_order_relaxed) / 1000u;

            if (m_belowThresholdFrames >= std::max<std::uint64_t>(requiredFrames, 1u)) {
                active = false;
                m_active.store(false, std::memory_order_release);
                m_aboveThresholdFrames = 0;
                m_belowThresholdFrames = 0;
            }
        } else {
            m_belowThresholdFrames = 0;
        }
    }

    return active;
}

void VoiceDetector::forceRelease() noexcept {
    m_active.store(false, std::memory_order_release);
    m_aboveThresholdFrames = 0;
    m_belowThresholdFrames = 0;
}

} // namespace voicedash
