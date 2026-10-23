#include "VoiceDetector.hpp"

#include <algorithm>
#include <cmath>

namespace voicedash {
namespace {

float estimatePeriodicity(const std::int16_t* samples,
                          std::size_t count,
                          std::uint32_t sampleRate) noexcept {
    if (samples == nullptr || count < 128 || sampleRate < 8000) {
        return 0.0f;
    }

    double mean = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        mean += static_cast<double>(samples[i]);
    }
    mean /= static_cast<double>(count);

    double totalEnergy = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double value = static_cast<double>(samples[i]) - mean;
        totalEnergy += value * value;
    }
    if (totalEnergy < 1.0e6) {
        return 0.0f;
    }

    // The input has been decimated to approximately 8–12.5 kHz. Search a
    // broad 75–400 Hz fundamental range to accommodate different voices.
    const std::size_t minLag = std::max<std::size_t>(2, sampleRate / 400u);
    const std::size_t maxLag = std::min<std::size_t>(
        count - 32, sampleRate / 75u
    );
    if (minLag >= maxLag) {
        return 0.0f;
    }

    double best = 0.0;
    for (std::size_t lag = minLag; lag <= maxLag; ++lag) {
        double cross = 0.0;
        double leftEnergy = 0.0;
        double rightEnergy = 0.0;

        for (std::size_t i = 0; i + lag < count; ++i) {
            const double left = static_cast<double>(samples[i]) - mean;
            const double right = static_cast<double>(samples[i + lag]) - mean;
            cross += left * right;
            leftEnergy += left * left;
            rightEnergy += right * right;
        }

        const double denominator = std::sqrt(leftEnergy * rightEnergy);
        if (denominator > 0.0) {
            best = std::max(best, cross / denominator);
        }
    }

    return static_cast<float>(std::clamp(best, 0.0, 1.0));
}

} // namespace

void VoiceDetector::configure(float activationThreshold,
                              float releaseThreshold,
                              float minimumPeriodicity,
                              std::uint32_t activationMs,
                              std::uint32_t releaseMs) noexcept {
    if (!std::isfinite(activationThreshold)) activationThreshold = 0.035f;
    if (!std::isfinite(releaseThreshold)) releaseThreshold = 0.020f;
    if (!std::isfinite(minimumPeriodicity)) minimumPeriodicity = 0.22f;

    activationThreshold = std::clamp(activationThreshold, 0.001f, 0.5f);
    releaseThreshold = std::clamp(releaseThreshold, 0.0005f, activationThreshold);
    minimumPeriodicity = std::clamp(minimumPeriodicity, 0.05f, 0.95f);
    activationMs = std::clamp<std::uint32_t>(activationMs, 5, 500);
    releaseMs = std::clamp<std::uint32_t>(releaseMs, 10, 1000);

    m_activationThreshold.store(activationThreshold, std::memory_order_relaxed);
    m_releaseThreshold.store(releaseThreshold, std::memory_order_relaxed);
    m_minimumPeriodicity.store(minimumPeriodicity, std::memory_order_relaxed);
    m_activationMs.store(activationMs, std::memory_order_relaxed);
    m_releaseMs.store(releaseMs, std::memory_order_relaxed);
}

bool VoiceDetector::processSamples(const std::int16_t* samples,
                                   std::size_t frameCount,
                                   std::uint32_t sampleRate) noexcept {
    if (samples == nullptr || frameCount == 0 || sampleRate == 0) {
        return isActive();
    }

    if (sampleRate != m_currentSampleRate) {
        // Reset callback-owned analysis if Android changes the input route/rate.
        m_currentSampleRate = sampleRate;
        m_decimationStride = std::max<std::uint32_t>(
            1u, (sampleRate + 12499u) / 12500u
        );
        m_decimationPhase = 0;
        m_periodicityCount = 0;
        m_periodicityWrite = 0;
        m_periodicityUpdateFrames = 0;
        m_subframeSquareSum = 0.0;
        m_subframeSamples = 0;
        m_coverageHistory.fill(false);
        m_coverageWrite = 0;
        m_coverageCount = 0;
        m_loudSubframes = 0;
        m_noiseFloor = 0.002f;
        m_aboveThresholdFrames = 0;
        m_belowThresholdFrames = 0;
        m_periodicity.store(0.0f, std::memory_order_relaxed);
        m_active.store(false, std::memory_order_release);
    }

    const float configuredOn =
        m_activationThreshold.load(std::memory_order_relaxed);
    const float configuredOff =
        m_releaseThreshold.load(std::memory_order_relaxed);
    const float minPeriodicity =
        m_minimumPeriodicity.load(std::memory_order_relaxed);
    const auto activationMs = m_activationMs.load(std::memory_order_relaxed);
    const auto releaseMs = m_releaseMs.load(std::memory_order_relaxed);

    double squareSum = 0.0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        const double value = static_cast<double>(samples[i]) / 32768.0;
        squareSum += value * value;
    }
    const float rms = static_cast<float>(
        std::sqrt(squareSum / static_cast<double>(frameCount))
    );
    m_lastRms.store(rms, std::memory_order_relaxed);

    bool active = m_active.load(std::memory_order_relaxed);
    if (!active && rms < configuredOn * 0.75f) {
        const float updated = m_noiseFloor * 0.985f +
            std::min(rms, configuredOn * 0.65f) * 0.015f;
        m_noiseFloor = std::clamp(updated, 0.0002f, configuredOn * 0.65f);
    }

    const float onThreshold = std::max(configuredOn, m_noiseFloor * 2.2f);
    const float offThreshold = std::min(
        onThreshold,
        std::max(configuredOff, m_noiseFloor * 1.65f)
    );
    const float coverageThreshold = std::max(
        configuredOn * 0.50f, m_noiseFloor * 1.20f
    );

    const std::uint32_t subframeSize = std::max<std::uint32_t>(1u, sampleRate / 200u);
    for (std::size_t i = 0; i < frameCount; ++i) {
        const std::int16_t raw = samples[i];
        m_subframeSquareSum +=
            (static_cast<double>(raw) / 32768.0) *
            (static_cast<double>(raw) / 32768.0);
        ++m_subframeSamples;

        if (m_decimationPhase == 0) {
            m_periodicityWindow[m_periodicityWrite] = raw;
            m_periodicityWrite = (m_periodicityWrite + 1u) % kWindowCapacity;
            m_periodicityCount = std::min(m_periodicityCount + 1u, kWindowCapacity);
        }
        ++m_decimationPhase;
        if (m_decimationPhase >= m_decimationStride) {
            m_decimationPhase = 0;
        }

        if (m_subframeSamples >= subframeSize) {
            const float subframeRms = static_cast<float>(
                std::sqrt(m_subframeSquareSum / static_cast<double>(m_subframeSamples))
            );
            const bool loud = subframeRms >= coverageThreshold;
            if (m_coverageCount == kCoverageBuckets && m_coverageHistory[m_coverageWrite]) {
                --m_loudSubframes;
            } else if (m_coverageCount < kCoverageBuckets) {
                ++m_coverageCount;
            }
            m_coverageHistory[m_coverageWrite] = loud;
            if (loud) {
                ++m_loudSubframes;
            }
            m_coverageWrite = (m_coverageWrite + 1u) % kCoverageBuckets;
            m_subframeSquareSum = 0.0;
            m_subframeSamples = 0;
        }
    }

    const std::uint32_t effectiveRate =
        std::max<std::uint32_t>(1u, sampleRate / m_decimationStride);
    const std::size_t requiredWindow =
        std::min<std::size_t>(kWindowCapacity,
            std::max<std::size_t>(128u, effectiveRate / 50u));

    // Audio callbacks use a signed 32-bit frame count; conversion is safe here.
    m_periodicityUpdateFrames += static_cast<std::uint32_t>(frameCount);
    if (m_periodicityCount >= requiredWindow &&
        m_periodicityUpdateFrames >= std::max<std::uint32_t>(1u, sampleRate / 200u)) {
        std::array<std::int16_t, kWindowCapacity> ordered{};
        const std::size_t oldest =
            (m_periodicityWrite + kWindowCapacity - m_periodicityCount) % kWindowCapacity;
        for (std::size_t i = 0; i < m_periodicityCount; ++i) {
            ordered[i] = m_periodicityWindow[(oldest + i) % kWindowCapacity];
        }

        m_periodicity.store(
            estimatePeriodicity(ordered.data(), m_periodicityCount, effectiveRate),
            std::memory_order_relaxed
        );
        m_periodicityUpdateFrames = 0;
    }

    const bool periodReady = m_periodicityCount >= requiredWindow;
    const float periodicity = m_periodicity.load(std::memory_order_relaxed);
    const bool coverageReady = m_coverageCount == kCoverageBuckets;
    const bool voiced = rms >= onThreshold &&
        periodReady && periodicity >= minPeriodicity &&
        coverageReady && m_loudSubframes >= 3u;
    const bool stillVoiced = rms >= offThreshold &&
        (!periodReady || periodicity >= minPeriodicity * 0.62f) &&
        (!coverageReady || m_loudSubframes >= 2u);

    const auto attackNeeded = std::max<std::uint64_t>(
        1u, static_cast<std::uint64_t>(sampleRate) * activationMs / 1000u
    );
    const auto releaseNeeded = std::max<std::uint64_t>(
        1u, static_cast<std::uint64_t>(sampleRate) * releaseMs / 1000u
    );

    if (!active) {
        if (rms >= onThreshold) {
            m_aboveThresholdFrames += frameCount;
            if (m_aboveThresholdFrames >= attackNeeded && voiced) {
                active = true;
                m_active.store(true, std::memory_order_release);
                m_aboveThresholdFrames = 0;
                m_belowThresholdFrames = 0;
            }
        } else {
            m_aboveThresholdFrames = 0;
        }
    } else if (!stillVoiced) {
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

    return isActive();
}

void VoiceDetector::forceRelease() noexcept {
    m_active.store(false, std::memory_order_release);
    m_lastRms.store(0.0f, std::memory_order_relaxed);
    m_periodicity.store(0.0f, std::memory_order_relaxed);
    m_currentSampleRate = 0;
    m_decimationStride = 4;
    m_decimationPhase = 0;
    m_periodicityUpdateFrames = 0;
    m_periodicityCount = 0;
    m_periodicityWrite = 0;
    m_subframeSquareSum = 0.0;
    m_subframeSamples = 0;
    m_coverageHistory.fill(false);
    m_coverageWrite = 0;
    m_coverageCount = 0;
    m_loudSubframes = 0;
    m_noiseFloor = 0.002f;
    m_aboveThresholdFrames = 0;
    m_belowThresholdFrames = 0;
}

} // namespace voicedash
