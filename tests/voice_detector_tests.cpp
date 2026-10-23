#include "../src/voice/VoiceDetector.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using voicedash::VoiceDetector;

namespace {
constexpr std::uint32_t kRate = 48000;
constexpr std::size_t kBlockFrames = 480; // 10 ms at 48 kHz

std::vector<std::int16_t> makeTone(std::size_t frames, float amplitude = 0.12f) {
    std::vector<std::int16_t> samples(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        const double phase = 2.0 * 3.14159265358979323846 * 220.0 *
            static_cast<double>(i) / static_cast<double>(kRate);
        samples[i] = static_cast<std::int16_t>(
            std::sin(phase) * amplitude * 32767.0f
        );
    }
    return samples;
}

std::vector<std::int16_t> silence(std::size_t frames) {
    return std::vector<std::int16_t>(frames, 0);
}

void feed(VoiceDetector& detector, const std::vector<std::int16_t>& samples) {
    detector.processSamples(samples.data(), samples.size(), kRate);
}
}

int main() {
    VoiceDetector detector;
    detector.configure(0.018f, 0.010f, 25, 60);

    const auto quiet = silence(kBlockFrames);
    for (int i = 0; i < 10; ++i) {
        feed(detector, quiet);
    }
    assert(!detector.isActive());

    // One 10 ms burst must not pass the 25 ms activation confirmation.
    feed(detector, makeTone(kBlockFrames, 0.15f));
    assert(!detector.isActive());

    feed(detector, makeTone(kBlockFrames * 5, 0.15f));
    assert(detector.isActive());

    // A short gap doesn't release: release confirmation is 60 ms.
    feed(detector, silence(kBlockFrames * 2));
    assert(detector.isActive());

    // Returning voice cancels the pending release.
    feed(detector, makeTone(kBlockFrames * 3, 0.15f));
    assert(detector.isActive());

    feed(detector, silence(kBlockFrames * 8));
    assert(!detector.isActive());

    feed(detector, makeTone(kBlockFrames * 5, 0.15f));
    assert(detector.isActive());
    detector.forceRelease();
    assert(!detector.isActive());

    std::cout << "VoiceDetector tests passed\n";
    return 0;
}
