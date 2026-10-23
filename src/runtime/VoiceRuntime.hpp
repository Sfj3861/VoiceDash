#pragma once

#include "../audio/AndroidAudioCapture.hpp"

class PlayLayer;

namespace voicedash {

class VoiceRuntime final {
public:
    static VoiceRuntime& get() noexcept;

    void onLevelStart(PlayLayer* layer) noexcept;
    void onLevelUpdate(PlayLayer* layer) noexcept;
    void onLevelEnd(PlayLayer* layer) noexcept;
    void onPause(PlayLayer* layer) noexcept;
    void onResume(PlayLayer* layer) noexcept;

private:
    VoiceRuntime() = default;

    void stopCaptureAndRelease(PlayLayer* layer) noexcept;
    void applyConfiguration() noexcept;

    VoiceDetector m_detector;
    AndroidAudioCapture m_capture;
    PlayLayer* m_layer = nullptr; // Main-thread-only.
    bool m_enabled = false;
    bool m_paused = false;
    bool m_startAttempted = false;
    bool m_voiceApplied = false;
};

} // namespace voicedash
