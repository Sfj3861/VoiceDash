#pragma once

#include "../voice/VoiceDetector.hpp"

#include <atomic>
#include <cstdint>

#if defined(GEODE_IS_ANDROID)
#include <aaudio/AAudio.h>
#endif

namespace voicedash {

/**
 * AAudio PCM16 mono capture. Start/stop must be called on the game's thread.
 * The data callback performs bounded, allocation-free analysis only.
 */
class AndroidAudioCapture final {
public:
    AndroidAudioCapture() = default;
    ~AndroidAudioCapture();

    AndroidAudioCapture(const AndroidAudioCapture&) = delete;
    AndroidAudioCapture& operator=(const AndroidAudioCapture&) = delete;

    bool start(VoiceDetector& detector) noexcept;
    void stop() noexcept;

    [[nodiscard]] bool isRunning() const noexcept {
        return m_running.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::int32_t lastError() const noexcept {
        return m_lastError.load(std::memory_order_relaxed);
    }

private:
#if defined(GEODE_IS_ANDROID)
    static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        std::int32_t numFrames) noexcept;

    static void errorCallback(
        AAudioStream* stream,
        void* userData,
        aaudio_result_t error) noexcept;

    AAudioStream* m_stream = nullptr;
#endif

    VoiceDetector* m_detector = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<std::int32_t> m_lastError{0};
};

} // namespace voicedash
