#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "input/InputMixer.hpp"
#include "runtime/VoiceRuntime.hpp"

using namespace geode::prelude;

class $modify(VoiceDashBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        // GJBaseGameLayer is also used by the editor. Only merge voice input
        // into player-one Jump while the current object is a PlayLayer.
        const bool inPlayLayer = typeinfo_cast<PlayLayer*>(this) != nullptr;
        const auto routed = voicedash::InputMixer::route(
            down, button, isPlayer1, inPlayLayer
        );

        if (routed.forward) {
            GJBaseGameLayer::handleButton(routed.down, button, isPlayer1);
        }
    }
};

class $modify(VoiceDashPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        voicedash::InputMixer::reset(this);
        voicedash::VoiceRuntime::get().onLevelStart(this);
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        voicedash::VoiceRuntime::get().onLevelUpdate(this);
    }

    void onQuit() {
        voicedash::VoiceRuntime::get().onLevelEnd(this);
        PlayLayer::onQuit();
    }
};

class $modify(VoiceDashPauseLayer, PauseLayer) {
    void onEnter() {
        if (auto* layer = PlayLayer::get()) {
            voicedash::VoiceRuntime::get().onPause(layer);
        }
        PauseLayer::onEnter();
    }

    void onExit() {
        PauseLayer::onExit();
        if (auto* layer = PlayLayer::get()) {
            voicedash::VoiceRuntime::get().onResume(layer);
        }
    }
};

$on_mod(Loaded) {
    log::info(
        "VoiceDash loaded. Voice control is opt-in; the Android host app must declare "
        "and grant RECORD_AUDIO."
    );
}
