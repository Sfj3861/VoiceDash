#include "VoiceDetector.hpp"

#include <algorithm>
#include <cmath>

namespace voicedash {

void VoiceDetector::configure(DetectorConfig config) {
    config.activationThreshold = std::clamp(config.activationThreshold, 0.001f, 0.5f);
    config.releaseThreshold = std::clamp(config.releaseThreshold, 0.0005f, config.activationThreshold);
    config.noiseMultiplier = std::clamp(config.noiseMultiplier, 1.1f, 10.f);
    config.maxNoiseFloor = std::clamp(config.maxNoiseFloor, 0.001f, 0.2f);
    config.minPeriodicity = std::clamp(config.minPeriodicity, 0.05f, 0.9f);
    config.attackMs = std::clamp(config.attackMs, 0u, 250u);
    config.releaseMs = std::clamp(config.releaseMs, 10u, 1000u);
    m_config = config;
}

void VoiceDetector::reset() noexcept {
    m_active.store(false, std::memory_order_release);
    m_lastRms.store(0.f, std::memory_order_relaxed);
    m_noiseFloor.store(0.004f, std::memory_order_relaxed);
    m_periodicity.store(0.f, std::memory_order_relaxed);
    m_attackSamples = 0;
    m_releaseSamples = 0;
}

// Lightweight autocorrelation estimate. It favours sustained voiced sounds
// (vowels/hums) and rejects many impulsive noises. It is not a speech recognizer
// and cannot perfectly distinguish voice from tonal environmental sounds.
static float estimatePeriodicity(const int16_t* samples, size_t count, uint32_t sampleRate) noexcept {
    if (!samples || count < 256 || sampleRate < 8000) return 0.f;

    double mean = 0.0;
    for (size_t i = 0; i < count; ++i) mean += samples[i];
    mean /= static_cast<double>(count);

    double energy = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double x = static_cast<double>(samples[i]) - mean;
        energy += x * x;
    }
    if (energy < 1.0e6) return 0.f;

    const size_t minLag = std::max<size_t>(2, sampleRate / 1000u);
    const size_t maxLag = std::min<size_t>(count - 32, sampleRate / 85u);
    if (minLag >= maxLag) return 0.f;

    double best = 0.0;
    for (size_t lag = minLag; lag <= maxLag; ++lag) {
        double cross = 0.0;
        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        for (size_t i = 0; i + lag < count; ++i) {
            const double a = static_cast<double>(samples[i]) - mean;
            const double b = static_cast<double>(samples[i + lag]) - mean;
            cross += a * b;
            leftEnergy += a * a;
            rightEnergy += b * b;
        }
        const double denominator = std::sqrt(leftEnergy * rightEnergy);
        if (denominator > 0.0) best = std::max(best, cross / denominator);
    }
    return static_cast<float>(std::clamp(best, 0.0, 1.0));
}

void VoiceDetector::process(const int16_t* samples, size_t count, uint32_t sampleRate) noexcept {
    if (!samples || count == 0 || sampleRate == 0) return;

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double value = static_cast<double>(samples[i]) / 32768.0;
        sum += value * value;
    }
    const float rms = static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
    const float periodicity = estimatePeriodicity(samples, count, sampleRate);
    m_lastRms.store(rms, std::memory_order_relaxed);
    m_periodicity.store(periodicity, std::memory_order_relaxed);

    const bool active = m_active.load(std::memory_order_relaxed);
    float noise = m_noiseFloor.load(std::memory_order_relaxed);

    // Adapt only while idle, so sustained voice is not learned as background.
    if (!active) {
        noise = (noise * 0.985f) + (std::min(rms, m_config.maxNoiseFloor) * 0.015f);
        noise = std::clamp(noise, 0.0005f, m_config.maxNoiseFloor);
        m_noiseFloor.store(noise, std::memory_order_relaxed);
    }

    const float onThreshold = std::max(m_config.activationThreshold, noise * m_config.noiseMultiplier);
    const float offThreshold = std::max(m_config.releaseThreshold, noise * (m_config.noiseMultiplier * 0.62f));
    const uint64_t attackNeeded = (static_cast<uint64_t>(sampleRate) * m_config.attackMs) / 1000u;
    const uint64_t releaseNeeded = (static_cast<uint64_t>(sampleRate) * m_config.releaseMs) / 1000u;
    const bool voiced = rms >= onThreshold && periodicity >= m_config.minPeriodicity;
    const bool stillVoiced = rms >= offThreshold && periodicity >= (m_config.minPeriodicity * 0.62f);

    if (!active) {
        if (voiced) {
            m_attackSamples += count;
            if (m_attackSamples >= attackNeeded) {
                m_active.store(true, std::memory_order_release);
                m_releaseSamples = 0;
            }
        } else {
            m_attackSamples = 0;
        }
    } else {
        if (!stillVoiced) {
            m_releaseSamples += count;
            if (m_releaseSamples >= releaseNeeded) {
                m_active.store(false, std::memory_order_release);
                m_attackSamples = 0;
            }
        } else {
            m_releaseSamples = 0;
        }
    }
}

} // namespace voicedash
