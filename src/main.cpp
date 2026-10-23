#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "AudioCapture.hpp"
#include "VoiceDetector.hpp"

using namespace geode::prelude;

namespace {
    voicedash::VoiceDetector g_detector;
    voicedash::AudioCapture g_capture(g_detector);
    bool g_voiceOwnsJump = false;

    bool voiceEnabled() {
        return Mod::get()->getSettingValue<bool>("voice-enabled");
    }

    void releaseVoiceInput(PlayLayer* layer) {
        if (g_voiceOwnsJump && layer && layer->m_player1) {
            layer->m_player1->releaseButton(PlayerButton::Jump);
        }
        g_voiceOwnsJump = false;
    }
}

class $modify(VoiceDashPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g_voiceOwnsJump = false;
        g_detector.reset();

        if (voiceEnabled()) {
            if (!g_capture.start()) {
                log::error("VoiceDash could not start microphone capture.");
            }
        }
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!voiceEnabled() || !g_capture.isRunning() || !m_player1) {
            releaseVoiceInput(this);
            return;
        }

        const bool shouldHold = g_detector.isActive();
        if (shouldHold && !g_voiceOwnsJump) {
            m_player1->pushButton(PlayerButton::Jump);
            g_voiceOwnsJump = true;
        } else if (!shouldHold && g_voiceOwnsJump) {
            m_player1->releaseButton(PlayerButton::Jump);
            g_voiceOwnsJump = false;
        }
    }

    void onQuit() {
        releaseVoiceInput(this);
        g_capture.stop();
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("VoiceDash loaded. Enable 'Voice control' in mod settings and grant microphone permission if Android asks.");
}
