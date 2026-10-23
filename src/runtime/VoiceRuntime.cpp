#include "VoiceRuntime.hpp"

#include "../input/InputMixer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

namespace voicedash {

VoiceRuntime& VoiceRuntime::get() noexcept {
    static VoiceRuntime instance;
    return instance;
}

void VoiceRuntime::applyConfiguration() noexcept {
    const auto activation = Mod::get()->getSettingValue<double>("activation-threshold");
    const auto release = Mod::get()->getSettingValue<double>("release-threshold");
    const auto activationMs = Mod::get()->getSettingValue<std::int64_t>("attack-ms");
    const auto releaseMs = Mod::get()->getSettingValue<std::int64_t>("release-ms");

    m_detector.configure(
        static_cast<float>(activation),
        static_cast<float>(release),
        static_cast<std::uint32_t>(std::max<std::int64_t>(activationMs, 5)),
        static_cast<std::uint32_t>(std::max<std::int64_t>(releaseMs, 5))
    );
}

void VoiceRuntime::onLevelStart(PlayLayer* layer) noexcept {
    m_layer = layer;
    m_enabled = false; // Force a fresh read of the saved setting on first update.
    m_paused = false;
    m_startAttempted = false;
    m_captureWasRunning = false;
    m_voiceApplied = false;
    m_detector.forceRelease();
}

void VoiceRuntime::onLevelUpdate(PlayLayer* layer) noexcept {
    m_layer = layer;
    applyConfiguration();

    const bool shouldEnable = Mod::get()->getSettingValue<bool>("voice-enabled");
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
            // Don't retry every frame. Toggle voice off/on or enter a new
            // level after fixing host-app permission/configuration.
            log::error(
                "VoiceDash: microphone did not start. Check the host app's "
                "RECORD_AUDIO declaration and permission; see docs/ANDROID_MICROPHONE.md."
            );
        } else {
            m_captureWasRunning = true;
        }
    }

    // AAudio may disconnect asynchronously. Clean up the stream on the game
    // thread after its callback reports an error; don't retry every frame.
    if (m_captureWasRunning && !m_capture.isRunning()) {
        m_capture.stop();
        m_captureWasRunning = false;
    }

    // Treat capture failure as silence and release any voice-held input.
    const bool shouldPress = m_enabled && !m_paused &&
        m_capture.isRunning() && m_detector.isActive();

    if (shouldPress != m_voiceApplied) {
        InputMixer::injectVoice(layer, shouldPress);
        m_voiceApplied = shouldPress;
    }
}

void VoiceRuntime::stopCaptureAndRelease(PlayLayer* layer) noexcept {
    m_capture.stop();
    m_captureWasRunning = false;
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
    // Capture starts during the next gameplay update, not from a pause UI callback.
}

} // namespace voicedash
