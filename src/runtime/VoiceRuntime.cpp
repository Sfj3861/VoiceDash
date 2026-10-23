#include "VoiceRuntime.hpp"

#include "../input/InputMixer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

namespace voicedash {

VoiceRuntime& VoiceRuntime::get() noexcept {
    static VoiceRuntime instance;
    return instance;
}

void VoiceRuntime::applyConfiguration() noexcept {
    const auto threshold = Mod::get()->getSettingValue<double>("activation-threshold");
    const auto activationMs = Mod::get()->getSettingValue<std::int64_t>("activation-ms");
    const auto releaseMs = Mod::get()->getSettingValue<std::int64_t>("release-ms");

    m_detector.configure(
        static_cast<float>(threshold),
        static_cast<std::uint32_t>(std::max<std::int64_t>(activationMs, 5)),
        static_cast<std::uint32_t>(std::max<std::int64_t>(releaseMs, 5))
    );
}

void VoiceRuntime::onLevelStart(PlayLayer* layer) noexcept {
    m_layer = layer;
    m_enabled = false; // Force a fresh read of the user setting on the next update.
    m_paused = false;
    m_startAttempted = false;
    m_voiceApplied = false;
    m_detector.forceRelease();
}

void VoiceRuntime::onLevelUpdate(PlayLayer* layer) noexcept {
    m_layer = layer;
    applyConfiguration();

    const bool shouldEnable = Mod::get()->getSettingValue<bool>("enable-voice");
    if (shouldEnable != m_enabled) {
        m_enabled = shouldEnable;
        m_startAttempted = false;

        if (!m_enabled) {
            stopCaptureAndRelease(layer);
        }
    }

    if (!m_enabled || m_paused) {
        if (m_capture.isRunning() || m_voiceApplied) {
            stopCaptureAndRelease(layer);
        }
        return;
    }

    if (!m_capture.isRunning() && !m_startAttempted) {
        m_startAttempted = true;
        if (!m_capture.start(m_detector)) {
            // Do not retry every frame. Toggle voice control off/on, or enter a
            // new level, after fixing the host app's permission/configuration.
            log::error(
                "VoiceDash: microphone did not start. Check Android RECORD_AUDIO "
                "permission and read docs/ANDROID_MICROPHONE.md."
            );
        }
    }

    // AAudio can report disconnection asynchronously. Do not leave Jump held
    // if capture stops unexpectedly.
    const bool shouldPress = m_enabled && !m_paused &&
        m_capture.isRunning() && m_detector.isActive();

    if (shouldPress != m_voiceApplied) {
        InputMixer::injectVoice(layer, shouldPress);
        m_voiceApplied = shouldPress;
    }
}

void VoiceRuntime::stopCaptureAndRelease(PlayLayer* layer) noexcept {
    m_capture.stop();
    m_detector.forceRelease();

    if (m_voiceApplied && layer != nullptr) {
        InputMixer::injectVoice(layer, false);
    }
    m_voiceApplied = false;
}

void VoiceRuntime::onLevelEnd(PlayLayer* layer) noexcept {
    stopCaptureAndRelease(layer);
    InputMixer::reset(layer);
    m_layer = nullptr;
    m_enabled = false;
    m_paused = false;
    m_startAttempted = false;
}

void VoiceRuntime::onPause(PlayLayer* layer) noexcept {
    if (layer != nullptr) {
        m_layer = layer;
    }
    m_paused = true;
    stopCaptureAndRelease(m_layer);
    m_startAttempted = false;
}

void VoiceRuntime::onResume(PlayLayer* layer) noexcept {
    if (layer != nullptr) {
        m_layer = layer;
    }
    m_paused = false;
    m_startAttempted = false;
    // Capture starts on the next PlayLayer update, never from the pause
    // overlay's onExit callback.
}

} // namespace voicedash
