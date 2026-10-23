#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include "AudioCapture.hpp"
#include "VoiceDetector.hpp"

using namespace geode::prelude;

namespace {
    voicedash::VoiceDetector g_detector;
    voicedash::AudioCapture g_capture(g_detector);

    // Jump is the logical OR of physical and voice input. The hook below
    // forwards only combined-state transitions, so one source cannot release
    // a button still held by the other source.
    PlayLayer* g_activeLayer = nullptr;
    bool g_physicalJumpHeld = false;
    bool g_voiceJumpHeld = false;
    bool g_dispatchedJumpHeld = false;

    bool voiceEnabled() {
        return Mod::get()->getSettingValue<bool>("voice-enabled");
    }

    void configureDetector() {
        voicedash::DetectorConfig config;
        config.activationThreshold = static_cast<float>(
            Mod::get()->getSettingValue<double>("activation-threshold"));
        config.releaseThreshold = static_cast<float>(
            Mod::get()->getSettingValue<double>("release-threshold"));
        config.attackMs = static_cast<uint32_t>(
            Mod::get()->getSettingValue<int64_t>("attack-ms"));
        config.releaseMs = static_cast<uint32_t>(
            Mod::get()->getSettingValue<int64_t>("release-ms"));
        g_detector.configure(config);
    }

    void dispatchCombinedJump(PlayLayer* layer) {
        const bool effective = g_physicalJumpHeld || g_voiceJumpHeld;
        if (effective == g_dispatchedJumpHeld) return;
        g_dispatchedJumpHeld = effective;

        if (layer) {
            // Qualified base call invokes the game's original input handler.
            layer->GJBaseGameLayer::handleButton(effective, 1, true);
        }
    }

    void resetInputState(PlayLayer* layer) {
        g_physicalJumpHeld = false;
        g_voiceJumpHeld = false;
        if (g_dispatchedJumpHeld && layer) {
            layer->GJBaseGameLayer::handleButton(false, 1, true);
        }
        g_dispatchedJumpHeld = false;
    }
}

class $modify(VoiceDashBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto* playLayer = typeinfo_cast<PlayLayer*>(this);
        if (!playLayer || button != 1 || !isPlayer1 || playLayer != g_activeLayer) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        g_physicalJumpHeld = down;
        dispatchCombinedJump(playLayer);
    }
};

class $modify(VoiceDashPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g_activeLayer = this;
        resetInputState(this);
        g_detector.reset();
        configureDetector();

        if (voiceEnabled() && !g_capture.start()) {
            log::error("VoiceDash could not start microphone capture.");
        }
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!voiceEnabled()) {
            if (g_voiceJumpHeld) {
                g_voiceJumpHeld = false;
                dispatchCombinedJump(this);
            }
            if (g_capture.isRunning()) g_capture.stop();
            return;
        }

        if (!g_capture.isRunning()) {
            if (g_voiceJumpHeld) {
                g_voiceJumpHeld = false;
                dispatchCombinedJump(this);
            }
            return;
        }

        const bool shouldHold = g_detector.isActive();
        if (shouldHold != g_voiceJumpHeld) {
            g_voiceJumpHeld = shouldHold;
            dispatchCombinedJump(this);
        }
    }

    void onQuit() {
        resetInputState(this);
        g_capture.stop();
        g_activeLayer = nullptr;
        PlayLayer::onQuit();
    }
};

$on_mod(Loaded) {
    log::info("VoiceDash loaded. Voice control is opt-in; the host Android app must declare and grant RECORD_AUDIO.");
}
