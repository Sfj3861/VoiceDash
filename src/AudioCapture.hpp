#pragma once

#include "VoiceDetector.hpp"
#include <atomic>
#include <thread>

namespace voicedash {

class AudioCapture {
public:
    explicit AudioCapture(VoiceDetector& detector);
    ~AudioCapture();

    AudioCapture(AudioCapture const&) = delete;
    AudioCapture& operator=(AudioCapture const&) = delete;

    // Returns false on unsupported platforms. On Android, true means the
    // worker was launched; microphone permission and device support are checked
    // asynchronously by the worker.
    bool start();
    void stop() noexcept;
    bool isRunning() const noexcept { return m_running.load(std::memory_order_acquire); }
    bool hadError() const noexcept { return m_error.load(std::memory_order_relaxed); }

private:
    void worker() noexcept;

    VoiceDetector& m_detector;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_error{false};
    std::thread m_thread;
};

} // namespace voicedash
